
// this is CPU scene renderer into ANSI framebuffer
#include "asciiid_render.h"

#include <stdint.h>
#include <malloc.h>
#include <stdio.h>
#include "terrain.h"
#include "mesh.h"
#include "matrix.h"
#include "fast_rand.h"
// #include "sprite.h"

#define _USE_MATH_DEFINES
#include <math.h>

#define DBL


template <typename Sample>
inline void Bresenham(Sample* buf, int w, int h, int from[3], int to[3])
{
	int sx = to[0] - from[0];
	int sy = to[1] - from[1];

	if (sx == 0 && sy==0)
		return;

	int sz = to[2] - from[2];

	int ax = sx >= 0 ? sx : -sx;
	int ay = sy >= 0 ? sy : -sy;

	if (ax >= ay)
	{
		float n = +1.0f / sx;
		// horizontal domain

		if (from[0] > to[0])
		{
			int* swap = from;
			from = to;
			to = swap;
		}

		int	x0 = (std::max(0, from[0]) + 1) & ~1; // round up start x, so we won't produce out of domain samples
		int	x1 = std::min(w, to[0]);

		for (int x = x0; x < x1; x+=2)
		{
			float a = x - from[0] + 0.5f;
			int y = (int)floor((a * sy)*n + from[1] + 0.5f);
			if (y >= 0 && y < h)
			{
				float z = (a * sz) * n + from[2];
				Sample* ptr = buf + w * y + x;
				if (ptr->DepthTest_RO(z))
					ptr->spare |= 4;
				ptr++;
				if (ptr->DepthTest_RO(z))
					ptr->spare |= 4;
			}
		}
	}
	else
	{
		float n = 1.0f / sy;
		// vertical domain

		if (from[1] > to[1])
		{
			int* swap = from;
			from = to;
			to = swap;
		}

		int y0 = std::max(0, from[1]);
		int y1 = std::min(h, to[1]);

		for (int y = y0; y < y1; y++)
		{
			int a = y - from[1];
			int x = (int)floor((a * sx) * n + from[0] + 0.5f);
			if (x >= 0 && x < w)
			{
				float z = (a * sz)*n + from[2];
				Sample* ptr = buf + w * y + x;
				if (ptr->DepthTest_RO(z))
					ptr->spare |= 4;
			}
		}
	}
}

template <typename Sample, typename Shader>
inline void Rasterize(Sample* buf, int w, int h, Shader* s, const int* v[3])
{
	// each v[i] must point to 4 ints: {x,y,z,f} where f should indicate culling bits (can be 0)
	// shader must implement: bool Shader::Fill(Sample* s, int bc[3])
	// where bc contains 3 barycentric weights which are normalized to 0x8000 (use '>>15' after averaging)
	// Sample must implement bool DepthTest(int z, int divisor);
	// it must return true if z/divisor passes depth test on this sample
	// if test passes, it should write new z/d to sample's depth (if something like depth write mask is enabled)

	// produces samples between buffer cells 
	#define BC_A(a,b,c) (2*(((b)[0] - (a)[0]) * ((c)[1] - (a)[1]) - ((b)[1] - (a)[1]) * ((c)[0] - (a)[0])))

	// produces samples at centers of buffer cells 
	#define BC_P(a,b,c) (((b)[0] - (a)[0]) * (2*(c)[1]+1 - 2*(a)[1]) - ((b)[1] - (a)[1]) * (2*(c)[0]+1 - 2*(a)[0]))

	if ((v[0][3] & v[1][3] & v[2][3]) == 0)
	{
		int area = BC_A(v[0],v[1],v[2]);
		if (area != 0)
		{
			assert(area < 0x10000);
			float normalizer = (1.0f - FLT_EPSILON) / area;

			// canvas intersection with triangle bbox
			int left = std::max(0, std::min(v[0][0], std::min(v[1][0], v[2][0])));
			int right = std::min(w, std::max(v[0][0], std::max(v[1][0], v[2][0])));
			int bottom = std::max(0, std::min(v[0][1], std::min(v[1][1], v[2][1])));
			int top = std::min(h, std::max(v[0][1], std::max(v[1][1], v[2][1])));

			Sample* col = buf + bottom * w + left;
			for (int y = bottom; y < top; y++, col+=w)
			{
				Sample* row = col;
				for (int x = left; x < right; x++, row++)
				{
					int p[2] = { x,y };

					int bc[3] =
					{
						BC_P(v[1], v[2], p),
						BC_P(v[2], v[0], p),
						BC_P(v[0], v[1], p)
					};

					// outside
					if (bc[0] < 0 || bc[1] < 0 || bc[2] < 0)
						continue;

					// edge pairing
					if (bc[0] == 0 && v[1][0] <= v[2][0] ||
						bc[1] == 0 && v[2][0] <= v[0][0] ||
						bc[2] == 0 && v[0][0] <= v[1][0])
					{
						continue;
					}

					assert(bc[0] + bc[1] + bc[2] == area);

					float nbc[3] =
					{
						bc[0] * normalizer,
						bc[1] * normalizer,
						bc[2] * normalizer
					};

					float z = nbc[0] * v[0][2] + nbc[1] * v[1][2] + nbc[2] * v[2][2];

					if (row->DepthTest_RW(z))
						s->Fill(row, nbc);
				}
			}
		}
	}
	#undef BC
}



struct Sample
{
	uint16_t visual;
	uint8_t diffuse;
	uint8_t spare;   // refl, patch xy parity etc..., direct color bit (meshes): visual has just 565 color?
	float height;

	inline bool DepthTest_RW(float z)
	{
		if (height > z)
			return false;
		spare &= ~0x4; // clear lines
		height = z;
		return true;
	}

	inline bool DepthTest_RO(float z)
	{
		if (height > z)
		{
			int a = 0;
		}
		return height <= z;
	}
};

struct SampleBuffer
{
	int w, h; // make 2x +2 bigger than terminal buffer
	Sample* ptr;
};

struct Renderer
{
	Renderer()
	{
		memset(this, 0, sizeof(Renderer));
	}

	~Renderer()
	{
		if (sample_buffer.ptr)
			free(sample_buffer.ptr);
	}

	SampleBuffer sample_buffer; // render surface

	uint8_t* buffer;
	int buffer_size; // ansi_buffer allocation size in cells (minimize reallocs)

	// current angles & position!
	// ....

	// derived transform and clipping planes
	// ....

	void Clear();
	void PostFx();
	void Ansify();

	// it has its own Shaders for fill & stroke
	// but requires here some extra state
	// ....
	static void RenderPatch(Patch* p, int x, int y, int view_flags, void* cookie /*Renderer*/);
	static void RenderMesh(Mesh* m, const double* tm, void* cookie /*Renderer*/);
	static void RenderFace(float coords[9], uint8_t colors[12], uint32_t visual, void* cookie /*Renderer*/);

	template <typename Shader>
	void LineStroke(Shader* shader, int xyz[2][3]);

	template <typename Shader>
	void TriangleFill(Shader* shader, int xyz[3][3], float uv[3][2]);

	// transform
	double mul[6]; // 3x2 rot part
	double add[2]; // post rotated and rounded translation
	float yaw;
	float water;
	float light[4];
	bool yaw_changed;

	double inst_tm[16];
};

void create_auto_mat(uint8_t mat[/*b*/32/*g*/*32/*r*/*32/*bg,fg,gl*/*3])
{
	#define FLO(x) ((int)floor(5 * x / 31.0f))
	#define REM(x) (5*x-31*flo[x])
	static const int flo[32]=
	{
		FLO(0),  FLO(1),  FLO(2),  FLO(3),
		FLO(4),  FLO(5),  FLO(6),  FLO(7),
		FLO(8),  FLO(9),  FLO(10), FLO(11),
		FLO(12), FLO(13), FLO(14), FLO(15),
		FLO(16), FLO(17), FLO(18), FLO(19),
		FLO(20), FLO(21), FLO(22), FLO(23),
		FLO(24), FLO(25), FLO(26), FLO(27),
		FLO(28), FLO(29), FLO(30), FLO(31),
	};

	static const int rem[32]=
	{
		REM(0),  REM(1),  REM(2),  REM(3),
		REM(4),  REM(5),  REM(6),  REM(7),
		REM(8),  REM(9),  REM(10), REM(11),
		REM(12), REM(13), REM(14), REM(15),
		REM(16), REM(17), REM(18), REM(19),
		REM(20), REM(21), REM(22), REM(23),
		REM(24), REM(25), REM(26), REM(27),
		REM(28), REM(29), REM(30), REM(31),
	};

	static const char glyph[] = " ..::%";

	int i = 0;
	for (int b=0; b<32; b++)
	{
		int p[3];
		p[2] = rem[b];
		int B[2] = { flo[b],std::min(5, flo[b] + 1) };
		for (int g = 0; b < 32; b++)
		{
			p[1] = rem[g];
			int G[2] = { flo[g],std::min(5, flo[g] + 1) };
			for (int r = 0; b < 32; b++,i++)
			{
				p[0] = rem[r];
				int R[2] = { flo[r],std::min(5, flo[r] + 1) };

				int best_sd = -1;
				int best_pr;
				int best_lo;
				int best_hi;

				for (int lo = 0; lo < 7; lo++)
				{
					int v0[3] = { R[lo & 1], G[(lo & 2) >> 1], B[(lo & 4) >> 2] };
					for (int hi = lo + 1; hi < 8; hi++)
					{
						int v1[3] = { R[hi & 1], G[(hi & 2) >> 1], B[(hi & 4) >> 2] };
						int v[3] = { 31*(v1[0] - v0[0]), 31*(v1[1] - v0[1]), 31*(v1[2] - v0[2]) };

						// so we have a p[3] and v[3] (same coord system)
						// calc distance & projection

						// projection
						int pr = v[0] * p[0] + v[1] * p[1] + v[2] * p[2]; // normalized to 2883=3*31*31

						// projection point
						int pp[3] = { v[0] * pr, v[1] * pr, v[2] * pr }; // normalized to 89373=31*3*31*31

						// dist vect, renormalized so sqaure dist fit in 32 bits
						int pv[3] = { (p[0] - pp[0] + 1) >> 1, (p[1] - pp[1] + 1) >> 1, (p[2] - pp[2] + 1) >> 1 };

						// square dist
						int sd = pv[0] * pv[0] + pv[1] * pv[1] + pv[2] * pv[2];

						if (sd < best_sd || best_sd<0)
						{
							best_sd = sd;
							best_pr = pr;
							best_lo = lo;
							best_hi = hi;
						}
					}
				}

				int idx = 3 * (r + 32 * (g + 32 * b));
				int shd = best_pr / 241; // 0..11
				if (shd < 6)
				{
					mat[idx + 0] = 16 + R[best_lo & 1] + 6 * G[(best_lo & 2) >> 1] + 36 * B[(best_lo & 4) >> 2];
					mat[idx + 1] = 16 + R[best_hi & 1] + 6 * G[(best_hi & 2) >> 1] + 36 * B[(best_hi & 4) >> 2];
					mat[idx + 2] = glyph[shd];
				}
				else
				{
					mat[idx + 0] = 16 + R[best_hi & 1] + 6 * G[(best_hi & 2) >> 1] + 36 * B[(best_hi & 4) >> 2];
					mat[idx + 1] = 16 + R[best_lo & 1] + 6 * G[(best_lo & 2) >> 1] + 36 * B[(best_lo & 4) >> 2];
					mat[idx + 2] = glyph[6-shd];
				}
			}
		}
	}
}

void Renderer::RenderFace(float coords[9], uint8_t colors[12], uint32_t visual, void* cookie)
{
	struct Shader
	{
		void Fill(Sample* s, float bc[3]) const
		{
			if (s->height >= water)
			{
				int r8 = (int)floor(rgb[0][0] * bc[0] + rgb[1][0] * bc[1] + rgb[2][0] * bc[2]);
				int r5 = (r8 * 249 + 1014) >> 11;
				int g8 = (int)floor(rgb[0][1] * bc[0] + rgb[1][1] * bc[1] + rgb[1][1] * bc[2]);
				int g5 = (g8 * 249 + 1014) >> 11;
				int b8 = (int)floor(rgb[0][2] * bc[0] + rgb[1][2] * bc[1] + rgb[1][2] * bc[2]);
				int b5 = (b8 * 249 + 1014) >> 11;

				s->visual = r5 | (g5 << 5) | (b5 << 10);
				s->diffuse = diffuse;
				s->spare |= 0x8;
			}
			else
				s->spare = 3;
		}

		/*
		inline void Diffuse(int dzdx, int dzdy)
		{
			float nl = (float)sqrt(dzdx * dzdx + dzdy * dzdy + HEIGHT_SCALE * HEIGHT_SCALE);
			float df = (dzdx * light[0] + dzdy * light[1] + HEIGHT_SCALE * light[2]) / nl;
			df = df * (1.0f - 0.5f*light[3]) + 0.5f*light[3];
			diffuse = df <= 0 ? 0 : (int)(df * 0xFF);
		}
		*/

		uint8_t* rgb[3]; // per vertex colors
		float water;
		float light[4];
		uint8_t diffuse; // shading experiment
	} shader;

	shader.rgb[0] = colors + 0;
	shader.rgb[1] = colors + 4;
	shader.rgb[2] = colors + 8;

	Renderer* r = (Renderer*)cookie;

	// temporarily, let's transform verts for each face separately

	int v[3][3];
	const int* pv[3] = { v[0],v[1],v[2] };
	
	float tmp[4];
	{
		float xyzw[] = { VISUAL_CELLS * coords[0], VISUAL_CELLS * coords[1], VISUAL_CELLS * coords[2], 1.0f };
		Product(r->inst_tm, xyzw, tmp);
		v[0][0] = (int)floor(tmp[0] + 0.5f);
		v[0][1] = (int)floor(tmp[1] + 0.5f);
		v[0][2] = (int)floor(tmp[2] + 0.5f);
	}

	{
		float xyzw[] = { VISUAL_CELLS * coords[3], VISUAL_CELLS * coords[4], VISUAL_CELLS * coords[5], 1.0f };
		Product(r->inst_tm, xyzw, tmp);
		v[1][0] = (int)floor(tmp[0] + 0.5f);
		v[1][1] = (int)floor(tmp[1] + 0.5f);
		v[1][2] = (int)floor(tmp[2] + 0.5f);
	}

	{
		float xyzw[] = { VISUAL_CELLS * coords[6], VISUAL_CELLS * coords[7], VISUAL_CELLS * coords[8], 1.0f };
		Product(r->inst_tm, xyzw, tmp);
		v[2][0] = (int)floor(tmp[0] + 0.5f);
		v[2][1] = (int)floor(tmp[1] + 0.5f);
		v[2][2] = (int)floor(tmp[2] + 0.5f);
	}

	int w = r->sample_buffer.w;
	int h = r->sample_buffer.h;
	Sample* ptr = r->sample_buffer.ptr;

	Rasterize(r->sample_buffer.ptr, r->sample_buffer.w, r->sample_buffer.h, &shader, pv);
}

void Renderer::RenderMesh(Mesh* m, const double* tm, void* cookie)
{

	Renderer* r = (Renderer*)cookie;
	double view_tm[16]=
	{
		r->mul[0], r->mul[1], 0.0, 0.0,
		r->mul[2], r->mul[3], 0.0, 0.0,
		r->mul[4], r->mul[5], 1.0, 0.0,
		r->add[0], r->add[1], 0.0, 1.0
	};

	MatProduct(view_tm, tm, r->inst_tm);
	QueryMesh(m, Renderer::RenderFace, r);

	// transform verts int integer coords
	// ...

	// given interpolated RGB -> round to 555, store it in visual
	// copy to diffuse to diffuse
	// mark mash 'auto-material' as 0x8 flag in spare

	// in post pass:
	// if sample has 0x8 flag
	//   multiply rgb by diffuse (into 888 bg=fg)
	// apply color mixing with neighbours
	// if at least 1 sample have mesh bit in spare
	// - round mixed bg rgb to R5G5B5 and use auto_material[32K] -> {bg,fg,gl}
	// else apply gridlines etc.
}

// we could easily make it template of <Sample,Shader>
void Renderer::RenderPatch(Patch* p, int x, int y, int view_flags, void* cookie /*Renderer*/)
{
	struct Shader
	{
		void Fill(Sample* s, float bc[3]) const
		{
			if (s->height >= water)
			{
				int u = (int)floor(uv[0] * bc[0] + uv[2] * bc[1] + uv[4] * bc[2]);
				int v = (int)floor(uv[1] * bc[0] + uv[3] * bc[1] + uv[5] * bc[2]);

				/*
				// UV-TEST
				s->visual = (u * 36) | ((v * 36)<<8);
				s->diffuse = diffuse;
				s->spare |= parity;
				*/

				if (u >= VISUAL_CELLS || v >= VISUAL_CELLS)
				{
					// detect overflow
					s->visual = 2;
				}
				else
				{
					s->visual = map[v * VISUAL_CELLS + u];
					s->diffuse = diffuse;
					s->spare |= parity;
				}
			}
			else
				s->spare = 3;
		}

		inline void Diffuse(int dzdx, int dzdy)
		{
			float nl = (float)sqrt(dzdx * dzdx + dzdy * dzdy + HEIGHT_SCALE * HEIGHT_SCALE);
			float df = (dzdx * light[0] + dzdy * light[1] + HEIGHT_SCALE * light[2]) / nl;
			df = df * (1.0f - 0.5f*light[3]) + 0.5f*light[3];
			diffuse = df <= 0 ? 0 : (int)(df * 0xFF);
		}

		int* uv; // points to array of 6 ints (u0,v0,u1,v1,u2,v2) each is equal to 0 or VISUAL_CELLS
		uint16_t* map; // points to array of VISUAL_CELLS x VISUAL_CELLS ushorts
		float water;
		float light[4];
		uint8_t diffuse; // shading experiment
		uint8_t parity;
	} shader;

	Renderer* r = (Renderer*)cookie;

	double* mul = r->mul;

	int iadd[2] = { (int)r->add[0], (int)r->add[1] };
	double* add = r->add;

	int w = r->sample_buffer.w;
	int h = r->sample_buffer.h;
	Sample* ptr = r->sample_buffer.ptr;

	uint16_t* hmap = GetTerrainHeightMap(p);
	
	uint16_t* hm = hmap;

	// transform patch verts xy+dx+dy, together with hmap into this array
	int xyzf[HEIGHT_CELLS + 1][HEIGHT_CELLS + 1][4];

	for (int dy = 0; dy <= HEIGHT_CELLS; dy++)
	{
		int vy = y * HEIGHT_CELLS + dy * VISUAL_CELLS;

		for (int dx = 0; dx <= HEIGHT_CELLS; dx++)
		{
			int vx = x * HEIGHT_CELLS + dx * VISUAL_CELLS;
			int vz = *(hm++);

			// transform 
			if (r->yaw_changed)
			{
				int tx = (int)floor(mul[0] * vx + mul[2] * vy + 0.5 + add[0]);
				int ty = (int)floor(mul[1] * vx + mul[3] * vy + mul[5] * vz + 0.5 + add[1]);

				xyzf[dy][dx][0] = tx;
				xyzf[dy][dx][1] = ty;
				xyzf[dy][dx][2] = vz;

				// todo: if patch is known to fully fit in screen, set f=0 
				// otherwise we need to check if / which screen edges cull each vertex
				xyzf[dy][dx][3] = (tx < 0) | ((tx > w) << 1) | ((ty < 0) << 2) | ((ty > h) << 3);
			}
			else
			{
				int tx = (int)floor(mul[0] * vx + mul[2] * vy + 0.5) + iadd[0];
				int ty = (int)floor(mul[1] * vx + mul[3] * vy + mul[5] * vz + 0.5) + iadd[1];

				xyzf[dy][dx][0] = tx;
				xyzf[dy][dx][1] = ty;
				xyzf[dy][dx][2] = vz;

				// todo: if patch is known to fully fit in screen, set f=0 
				// otherwise we need to check if / which screen edges cull each vertex
				xyzf[dy][dx][3] = (tx < 0) | ((tx > w) << 1) | ((ty < 0) << 2) | ((ty > h) << 3);
			}
		}
	}

	uint16_t  diag = GetTerrainDiag(p);

	// 2 parity bits for drawing lines around patches
	// 0 - no patch rendered here
	// 1 - odd
	// 2 - even
	// 3 - under water
	shader.parity = (((x^y)/VISUAL_CELLS) & 1) + 1; 
	shader.water = r->water;
	shader.map = GetTerrainVisualMap(p);

	shader.light[0] = r->light[0];
	shader.light[1] = r->light[1];
	shader.light[2] = r->light[2];
	shader.light[3] = r->light[3];

	/*
	shader.light[0] = 0;
	shader.light[1] = 0;
	shader.light[2] = 1;
	*/

//	if (shader.parity == 1)
//		return;

	hm = hmap;

	static const int uv[HEIGHT_CELLS][2] =
	{
		{0, VISUAL_CELLS / HEIGHT_CELLS},
		{VISUAL_CELLS / HEIGHT_CELLS, 2 * VISUAL_CELLS / HEIGHT_CELLS},
		{2 * VISUAL_CELLS / HEIGHT_CELLS, 3 * VISUAL_CELLS / HEIGHT_CELLS},
		{3 * VISUAL_CELLS / HEIGHT_CELLS, 4 * VISUAL_CELLS / HEIGHT_CELLS}
	};

	for (int dy = 0; dy < HEIGHT_CELLS; dy++, hm++)
	{
		for (int dx = 0; dx < HEIGHT_CELLS; dx++,diag>>=1, hm++)
		{
			//if (!(diag & 1))
			if (diag & 1)
			{
				// .
				// |\
				// |_\
				// '  '
				// lower triangle

				// terrain should keep diffuse map with timestamp of light modification it was updated to
				// then if current light timestamp is different than in terrain we need to update diffuse (into terrain)
				// now we should simply use diffuse from terrain
				// note: if terrain is being modified, we should clear its timestamp or immediately update diffuse

				int lo_uv[] = {uv[dx][0],uv[dy][0], uv[dx][1],uv[dy][0], uv[dx][0],uv[dy][1]};
				const int* lo[3] = { xyzf[dy][dx], xyzf[dy][dx + 1], xyzf[dy + 1][dx] };
				shader.uv = lo_uv;
				shader.Diffuse(xyzf[dy][dx][2] - xyzf[dy][dx + 1][2], xyzf[dy][dx][2] - xyzf[dy + 1][dx][2]);
				Rasterize(ptr, w, h, &shader, lo);

				// .__.
				//  \ |
				//   \|
				//    '
				// upper triangle
				int up_uv[] = {uv[dx][1],uv[dy][1], uv[dx][0],uv[dy][1], uv[dx][1],uv[dy][0]};
				const int* up[3] = { xyzf[dy + 1][dx + 1], xyzf[dy + 1][dx], xyzf[dy][dx + 1] };
				shader.uv = up_uv;
				shader.Diffuse(xyzf[dy+1][dx][2] - xyzf[dy+1][dx+1][2], xyzf[dy][dx+1][2] - xyzf[dy+1][dx+1][2]);
				Rasterize(ptr, w, h, &shader, up);
			}
			else
			{
				// lower triangle
				//    .
				//   /|
				//  /_|
				// '  '
				int lo_uv[] = {uv[dx][1],uv[dy][0], uv[dx][1],uv[dy][1], uv[dx][0],uv[dy][0]};
				const int* lo[3] = { xyzf[dy][dx + 1], xyzf[dy + 1][dx + 1], xyzf[dy][dx] };
				shader.uv = lo_uv;
				shader.Diffuse(xyzf[dy][dx][2] - xyzf[dy][dx+1][2], xyzf[dy][dx+1][2] - xyzf[dy+1][dx+1][2]);
				Rasterize(ptr, w, h, &shader, lo);

				// upper triangle
				// .__.
				// | / 
				// |/  
				// '
				int up_uv[] = {uv[dx][0],uv[dy][1], uv[dx][0],uv[dy][0], uv[dx][1],uv[dy][1]};
				const int* up[3] = { xyzf[dy + 1][dx], xyzf[dy][dx], xyzf[dy + 1][dx + 1] };
				shader.uv = up_uv;
				shader.Diffuse(xyzf[dy+1][dx][2] - xyzf[dy+1][dx+1][2], xyzf[dy][dx][2] - xyzf[dy+1][dx][2]);
				Rasterize(ptr, w, h, &shader, up);
			}
		}
	}


	// grid lines thru middle of patch?
	// TODO: RENDER AFTER ALL PATCHES!
	// 

	int mid = (HEIGHT_CELLS + 1) / 2;

	for (int lin = 0; lin <= HEIGHT_CELLS; lin++)
	{
		xyzf[lin][mid][2] += HEIGHT_SCALE/2;
		if (mid!=lin)
			xyzf[mid][lin][2] += HEIGHT_SCALE / 2;
	}

	for (int lin = 0; lin < HEIGHT_CELLS; lin++)
	{
		Bresenham(ptr, w, h, xyzf[lin][mid], xyzf[lin + 1][mid]);
		Bresenham(ptr, w, h, xyzf[mid][lin], xyzf[mid][lin + 1]);
	}
}

bool Render(Terrain* t, World* w, float water, float zoom, float yaw, float pos[3], float lt[4], int width, int height, AnsiCell* ptr)
{
	static Renderer r;

#ifdef DBL
	float scale = 3.0;
#else
	float scale = 1.5;
#endif

	zoom *= scale;

#ifdef DBL
	int dw = 4+2*width;
	int dh = 4+2*height;
#else
	int dw = 1 + width + 1;
	int dh = 1 + height + 1;
#endif

	float ds = 2*zoom / VISUAL_CELLS;

	r.yaw_changed = true;

	if (!r.sample_buffer.ptr)
	{
		r.sample_buffer.w = dw;
		r.sample_buffer.h = dh;
		r.sample_buffer.ptr = (Sample*)malloc(dw*dh * sizeof(Sample));
	}
	else
	if (r.sample_buffer.w != dw || r.sample_buffer.h != dh)
	{
		r.sample_buffer.w = dw;
		r.sample_buffer.h = dh;
		free(r.sample_buffer.ptr);
		r.sample_buffer.ptr = (Sample*)malloc(dw*dh * sizeof(Sample));
	}
	else
	if (yaw == r.yaw)
	{
		r.yaw_changed = false;
	}

	r.yaw = yaw;
	r.water = water;
	r.light[0] = lt[0];
	r.light[1] = lt[1];
	r.light[2] = lt[2];
	r.light[3] = lt[3];

	// clear (at least depth!)
	memset(r.sample_buffer.ptr, 0x00, dw*dh * sizeof(Sample));

	static const double sin30 = sin(M_PI*30.0/180.0); 
	static const double cos30 = cos(M_PI*30.0/180.0);

	double a = yaw * M_PI / 180.0;
	double sinyaw = sin(a);
	double cosyaw = cos(a);

	double tm[16];
	tm[0] = +cosyaw *ds;
	tm[1] = -sinyaw * sin30*ds;
	tm[2] = 0;
	tm[3] = 0;
	tm[4] = +sinyaw * ds;
	tm[5] = +cosyaw * sin30*ds;
	tm[6] = 0;
	tm[7] = 0;
	tm[8] = 0;
	tm[9] = +cos30/HEIGHT_SCALE*ds*HEIGHT_CELLS;
	tm[10] = 1.0; //+2./0xffff;
	tm[11] = 0;
	tm[12] = dw*0.5 - (pos[0] * tm[0] + pos[1] * tm[4] + pos[2] * tm[8]) * HEIGHT_CELLS;
	tm[13] = dh*0.5 - (pos[0] * tm[1] + pos[1] * tm[5] + pos[2] * tm[9]) * HEIGHT_CELLS;
	tm[14] = 0.0; //-1.0;
	tm[15] = 1.0;

	r.mul[0] = tm[0];
	r.mul[1] = tm[1];
	r.mul[2] = tm[4];
	r.mul[3] = tm[5];
	r.mul[4] = 0;
	r.mul[5] = tm[9];

	// if yaw didn't change, make it INTEGRAL (and EVEN in case of DBL)
	r.add[0] = tm[12];
	r.add[1] = tm[13] + 0.5;

	if (!r.yaw_changed)
	{
		int x = (int)floor(r.add[0] + 0.5);
		int y = (int)floor(r.add[1] + 0.5);

		#ifdef DBL
		x &= ~1;
		y &= ~1;
		#endif

		r.add[0] = (double)x;
		r.add[1] = (double)y;
	}

	int planes = 4;
	int view_flags = 0xAA; // should contain only bits that face viewing direction

	double clip_world[4][4];

	double clip_left[4] =   { 1, 0, 0, .9 };
	double clip_right[4] =  {-1, 0, 0, .9 };
	double clip_bottom[4] = { 0, 1, 0, .9 };
	double clip_top[4] =    { 0,-1, 0, .9 };

	// easier to use another transform for clipping
	{
		// somehow it works
		double clip_tm[16];
		clip_tm[0] = +cosyaw / (0.5 * dw) * ds * HEIGHT_CELLS;
		clip_tm[1] = -sinyaw*sin30 / (0.5 * dh) * ds * HEIGHT_CELLS;
		clip_tm[2] = 0;
		clip_tm[3] = 0;
		clip_tm[4] = +sinyaw / (0.5 * dw) * ds * HEIGHT_CELLS;
		clip_tm[5] = +cosyaw*sin30 / (0.5 * dh) * ds * HEIGHT_CELLS;
		clip_tm[6] = 0;
		clip_tm[7] = 0;
		clip_tm[8] = 0;
		clip_tm[9] = +cos30 / HEIGHT_SCALE / (0.5 * dh) * ds * HEIGHT_CELLS;
		clip_tm[10] = +2. / 0xffff;
		clip_tm[11] = 0;
		clip_tm[12] = -(pos[0] * clip_tm[0] + pos[1] * clip_tm[4] + pos[2] * clip_tm[8]);
		clip_tm[13] = -(pos[0] * clip_tm[1] + pos[1] * clip_tm[5] + pos[2] * clip_tm[9]);
		clip_tm[14] = -1.0;
		clip_tm[15] = 1.0;

		TransposeProduct(clip_tm, clip_left, clip_world[0]);
		TransposeProduct(clip_tm, clip_right, clip_world[1]);
		TransposeProduct(clip_tm, clip_bottom, clip_world[2]);
		TransposeProduct(clip_tm, clip_top, clip_world[3]);
	}

	QueryTerrain(t, planes, clip_world, view_flags, Renderer::RenderPatch, &r);
	QueryWorld(w, 0/*planes*/, clip_world, Renderer::RenderMesh, &r);

	void* GetMaterialArr();
	Material* matlib = (Material*)GetMaterialArr();

	Sample* src = r.sample_buffer.ptr + 2 + 2*dw;
	for (int y = 0; y < height; y++)
	{
		for (int x = 0; x < width; x++, ptr++)
		{
			#ifdef DBL
			
			// average 4 backgrounds
			// mask 11 (something rendered)
			int spr[4] = { src[0].spare & 11, src[1].spare & 11, src[dw].spare & 11, src[dw + 1].spare & 11 };
			int mat[4] = { src[0].visual & 0x00FF , src[1].visual & 0x00FF, src[dw].visual & 0x00FF, src[dw + 1].visual & 0x00FF };
			int dif[4] = { src[0].diffuse , src[1].diffuse, src[dw].diffuse, src[dw + 1].diffuse };

			// TODO:
			// every material must have 16x16 map and uses visual shade to select Y and lighting to select X
			// animated materials additionaly pre shifts and wraps visual shade by current time scaled by material's 'speed'

			int elv = 0; // (src[0].visual >> 15) & 0x0001;

			/*
			int shd = 0; // (src[0].visual >> 8) & 0x007F;

			int gl = matlib[mat[0]].shade[1][shd].gl;
			int bg[3] = { 0,0,0 };
			int fg[3] = { 0,0,0 };
			for (int i = 0; i < 4; i++)
			{
				bg[0] += matlib[mat[i]].shade[1][shd].bg[0] * dif[i];
				bg[1] += matlib[mat[i]].shade[1][shd].bg[1] * dif[i];
				bg[2] += matlib[mat[i]].shade[1][shd].bg[2] * dif[i];
				fg[0] += matlib[mat[i]].shade[1][shd].fg[0] * dif[i];
				fg[1] += matlib[mat[i]].shade[1][shd].fg[1] * dif[i];
				fg[2] += matlib[mat[i]].shade[1][shd].fg[2] * dif[i];
			}
			*/

			int shd = (dif[0] + dif[1] + dif[2] + dif[3] + 17*2) / (17 * 4); // 17: FF->F, 4: avr
			int gl = matlib[mat[0]].shade[1][shd].gl;

			int bg[3] = { 0,0,0 };
			int fg[3] = { 0,0,0 };

			for (int i = 0; i < 4; i++)
			{
				if (spr[i])
				{
					bg[0] += matlib[mat[i]].shade[1][shd].bg[0];
					bg[1] += matlib[mat[i]].shade[1][shd].bg[1];
					bg[2] += matlib[mat[i]].shade[1][shd].bg[2];
					fg[0] += matlib[mat[i]].shade[1][shd].fg[0];
					fg[1] += matlib[mat[i]].shade[1][shd].fg[1];
					fg[2] += matlib[mat[i]].shade[1][shd].fg[2];
				}
			}

			int bk_rgb[3] =
			{
				(bg[0] + 102) / 204,
				(bg[1] + 102) / 204,
				(bg[2] + 102) / 204
			};

			ptr->gl = gl;
			ptr->bk = 16 + bk_rgb[0] + bk_rgb[1] * 6 + bk_rgb[2] * 36;
			ptr->fg = 16 + (((fg[0] + 102) / 204) + (((fg[1] + 102) / 204) * 6) + (((fg[2] + 102) / 204) * 36));
			ptr->spare = 0xFF;

			// collect line bits
			int linecase = ((src[0].spare & 0x4)>>2) | ((src[1].spare & 0x4) >> 1) | (src[dw].spare & 0x4) | ((src[dw+1].spare & 0x4)<<1);

			static const int linecase_glyph[] = {0, ',', ',', ',', '`', ';', ';', ';', '`', ';', ';', ';', '`', ';', ';', ';'};
			if (linecase)
			{
				/*
				if ((bk_rgb[0] | bk_rgb[1] | bk_rgb[2]) == 0)
				{
					ptr->fg = 16 + 1 + 1 * 6 + 1 * 36;
				}
				else
				{
					bk_rgb[0] = std::max(0, bk_rgb[0] - 1);
					bk_rgb[1] = std::max(0, bk_rgb[1] - 1);
					bk_rgb[2] = std::max(0, bk_rgb[2] - 1);
					ptr->fg = 16 + bk_rgb[0] + bk_rgb[1] * 6 + bk_rgb[2] * 36;
				}
				*/
				ptr->gl = linecase_glyph[linecase];
			}


			// silhouette repetitoire:  _-/\| (should not be used by materials?)

			float z_hi = src[dw].height + src[dw + 1].height;
			float z_lo = src[0].height + src[1].height;
			float z_pr = src[-dw].height + src[1-dw].height;

			float minus = z_lo - z_hi;
			float under = z_pr - z_lo;

			static const float thresh = 1 * HEIGHT_SCALE;

			if (minus > under)
			{
				if (minus > thresh)
				{
					ptr->gl = 0xC4; // '-'
					bk_rgb[0] = std::max(0, bk_rgb[0] - 1);
					bk_rgb[1] = std::max(0, bk_rgb[1] - 1);
					bk_rgb[2] = std::max(0, bk_rgb[2] - 1);
					ptr->fg = 16 + bk_rgb[0] + bk_rgb[1] * 6 + bk_rgb[2] * 36;
				}
			}
			else
			{
				if (under > thresh)
				{
					ptr->gl = 0x5F; // '_'
					bk_rgb[0] = std::max(0, bk_rgb[0] - 1);
					bk_rgb[1] = std::max(0, bk_rgb[1] - 1);
					bk_rgb[2] = std::max(0, bk_rgb[2] - 1);
					ptr->fg = 16 + bk_rgb[0] + bk_rgb[1] * 6 + bk_rgb[2] * 36;
				}
			}

			src += 2;


			
			#else
			
			int mat = src[0].visual & 0x00FF;
			int shd = 0; // (src[0].visual >> 8) & 0x007F;
			int elv = 0; // (src[0].visual >> 15) & 0x0001;

			// fill from material
			const MatCell* cell = &(matlib[mat].shade[1][shd]);
			const uint8_t* bg = matlib[mat].shade[1][shd].bg;
			const uint8_t* fg = matlib[mat].shade[1][shd].fg;

			ptr->gl = cell->gl;
			ptr->bk = 16 + (((bg[0] + 25) / 51) + (((bg[1] + 25) / 51) * 6) + (((bg[2] + 25) / 51) * 36));
			ptr->fg = 16 + (((fg[0] + 25) / 51) + (((fg[1] + 25) / 51) * 6) + (((fg[2] + 25) / 51) * 36));
			ptr->spare = 0xFF;

			src++;
			#endif

		}

		#ifdef DBL
		src += 4 + dw;
		#else
		src += 2;
		#endif
	}

	return true;
}
