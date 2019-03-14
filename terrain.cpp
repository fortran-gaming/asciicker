
#include <stdint.h>
#include <math.h>
#include <malloc.h>
#include <assert.h>

#include "terrain.h"

#if 0
#define TERRAIN_MATERIALS 64
#define DETAIL_ANGLES 12

struct Material
{
	struct
	{
		struct
		{
			uint8_t f; // foreground (0-transparent only for elevations)
			uint8_t b; // background (0-transparent only for elevations)
			uint8_t c; // character
		} elev_upper, elev_lower, fill[16/*light*/][16/*shade*/];
	} optic[2/*optic_flag*/];
};

struct Foliage
{
	struct
	{
		uint8_t x,y; // center offs
		uint8_t width;
		uint8_t height;
		struct
		{
			uint8_t f; // foreground (0-transparent)
			uint8_t b; // background (0-transparent)
			uint8_t c; // character
			int8_t  d; // depth or height?
		}* sprite;
	} image[2/*norm & refl*/][DETAIL_ANGLES];
};
#endif

struct Node;

struct QuadItem
{
	Node* parent;
	uint16_t lo, hi;
};

struct Node : QuadItem
{
	QuadItem* quad[4]; // all 4 are same, either Nodes or Patches, at least 1 must not be NULL
};

struct Patch : QuadItem // 564 bytes (512x512 'raster' map would require 564KB)
{
	// visual contains:                grass, sand, rock,
	// 1bit elevation, 1bit optic_flag, 6bits material_idx, 4bits_shade, 4bits_light
	// uint16_t visual[VISUAL_CELLS][VISUAL_CELLS];
	uint16_t height[HEIGHT_CELLS + 1][HEIGHT_CELLS + 1];
	uint16_t flags; // 8 bits of neighbors, bit0 is on (-,-), bit7 is on (-,0)
};

struct Terrain
{
	int x, y; // worldspace origin from tree origin
	int level; // 0 -> root is patch, -1 -> empty
	QuadItem* root;  // Node or Patch or NULL
};

Terrain* CreateTerrain(int z)
{
	Terrain* t = (Terrain*)malloc(sizeof(Terrain));
	t->x = 0;
	t->y = 0;

	if (z >= 0)
	{
		t->level = 0;

		Patch* p = (Patch*)malloc(sizeof(Patch));
		p->parent = 0;
		p->lo = 0; // (no neighbor)
		p->hi = z;
		p->flags = 0;

		for (int y = 0; y <= HEIGHT_CELLS; y++)
			for (int x = 0; x <= HEIGHT_CELLS; x++)
				p->height[y][x] = z;

		t->root = p;
	}
	else
	{
		t->level = -1;
		t->root = 0;
	}

	return t;
}


void DeleteTerrain(Terrain* t)
{
	if (!t)
		return;

	if (!t->root)
	{
		free(t);
		return;
	}

	if (t->level == 0)
	{
		Patch* p = (Patch*)t->root;
		free(p);
		free(t);
		return;
	}

	int lev = t->level;
	int xy = 0;
	Node* n = (Node*)t->root;
	free(t);

	while (true)
	{
		// __label__ recurse;

	recurse:
		lev--;

		if (!lev)
		{
			for (int i = 0; i < 4; i++)
			{
				Patch* p = (Patch*)n->quad[i];
				if (p)
					free(p);
			}
		}
		else
		{
			for (int i = 0; i < 4; i++)
			{
				if (n->quad[i])
				{
					xy = (xy << 2) + i;
					n = (Node*)n->quad[i];
					goto recurse;
				}
			}
		}

		while (true)
		{
			Node* p = n->parent;
			free(n);

			if (!p)
				return;

			while ((xy & 3) < 3)
			{
				xy++;
				if (p->quad[xy & 3])
				{
					n = (Node*)p->quad[xy & 3];
					goto recurse;
				}
			}

			xy <<= 2;
			n = p;
			lev++;
		}
	}
}

Patch* GetTerrainPatch(Terrain* t, int x, int y)
{
	if (!t->root)
		return 0;

	x += t->x;
	y += t->y;

	int range = 1 << t->level;
	if (x < 0 || y < 0 || x >= range || y >= range)
		return 0;

	if (t->level == 0)
		return (Patch*)t->root;

	int lev = t->level;

	Node* n = (Node*)t->root;
	while (n)
	{
		lev--;
		int i = ((x >> lev) & 1) | (((y >> lev) & 1) << 1);

		if (lev)
			n = (Node*)n->quad[i];
		else
			return (Patch*)n->quad[i];
	}

	return 0;
}

static void UpdateNodes(Patch* p)
{
	QuadItem* q = p;
	Node* n = p->parent;

	while (n)
	{
		int lo = 0xffff;
		int hi = 0x0000;

		for (int i = 0; i < 4; i++)
		{
			if (n->quad[i])
			{
				lo = n->quad[i]->lo < lo ? n->quad[i]->lo : lo;
				hi = n->quad[i]->hi > hi ? n->quad[i]->hi : hi;
			}
		}

		n->lo = lo;
		n->hi = hi;

		n = n->parent;
	}
}

bool DelTerrainPatch(Terrain* t, int x, int y)
{
	Patch* p = GetTerrainPatch(t, x, y);
	if (!p)
		return false;

	int flags = p->flags;
	Node* n = p->parent;
	free(p);

	if (!n)
	{
		t->level = -1;
		t->root = 0;
		return true;
	}

	// leaf trim

	QuadItem* q = p;

	while (n)
	{
		int c = 0;
		for (int i = 0; i < 4; i++)
		{
			if (n->quad[i] == q)
				n->quad[i] = 0;
			else
			if (n->quad[i])
				c++;
		}

		if (!c)
		{
			q = n;
			n = n->parent;
			free((Node*)q);
		}
	}

	// root trim

	n = (Node*)t->root;
	while (t->level)
	{
		int c = 0;
		int j = 0;
		for (int i = 0; i < 4; i++)
		{
			if (n->quad[i])
			{
				j = i;
				c++;
			}
		}

		if (c > 1)
			break;

		t->level--;

		if (j & 1)
			t->x -= 1 << t->level;
		if (j & 2)
			t->y -= 1 << t->level;
	}

	Patch* np[8] =
	{
		flags & 0x01 ? GetTerrainPatch(t, x - 1, y - 1) : 0,
		flags & 0x02 ? GetTerrainPatch(t, x, y - 1) : 0,
		flags & 0x04 ? GetTerrainPatch(t, x + 1, y - 1) : 0,
		flags & 0x08 ? GetTerrainPatch(t, x + 1, y) : 0,
		flags & 0x10 ? GetTerrainPatch(t, x + 1, y + 1) : 0,
		flags & 0x20 ? GetTerrainPatch(t, x, y + 1) : 0,
		flags & 0x40 ? GetTerrainPatch(t, x - 1, y + 1) : 0,
		flags & 0x80 ? GetTerrainPatch(t, x - 1, y) : 0,
	};

	for (int i = 0; i < 8; i++)
	{
		if (np[i])
		{
			int j = (i + 4) & 7;
			np[i]->flags &= ~(1 << j);
			if (np[i]->lo)
			{
				np[i]->lo = 0;
				UpdateNodes(np[i]);
			}
		}
	}

	return true;
}

Patch* AddTerrainPatch(Terrain* t, int x, int y, int z)
{
	if (z < 0)
		return 0;

	if (!t->root)
	{
		t->x = -x;
		t->y = -y;
		t->level = 0;

		Patch* p = (Patch*)malloc(sizeof(Patch));
		p->parent = 0;
		p->lo = 0; // no neighbor
		p->hi = z;
		p->flags = 0;

		for (int y = 0; y <= HEIGHT_CELLS; y++)
			for (int x = 0; x <= HEIGHT_CELLS; x++)
				p->height[y][x] = z;

		t->root = p;
		return p;
	}

	x += t->x;
	y += t->y;

	// create parents such root encloses x,y

	int range = 1 << t->level;

	while (x < 0)
	{
		Node* n = (Node*)malloc(sizeof(Node));

		if (2 * y < range)
		{
			n->quad[0] = 0;
			n->quad[1] = 0;
			n->quad[2] = 0;
			n->quad[3] = t->root;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->x += range;
			x += range;

			t->y += range;
			y += range;
		}
		else
		{
			n->quad[0] = 0;
			n->quad[1] = t->root;
			n->quad[2] = 0;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->x += range;
			x += range;
		}

		range *= 2;

		n->parent = 0;
		t->root->parent = n;
		t->root = n;
	}

	while (y < 0)
	{
		Node* n = (Node*)malloc(sizeof(Node));

		if (2 * x < range)
		{
			n->quad[0] = 0;
			n->quad[1] = 0;
			n->quad[2] = 0;
			n->quad[3] = t->root;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->x += range;
			x += range;

			t->y += range;
			y += range;
		}
		else
		{
			n->quad[0] = 0;
			n->quad[1] = 0;
			n->quad[2] = t->root;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->y += range;
			y += range;
		}

		range *= 2;

		n->parent = 0;
		t->root->parent = n;
		t->root = n;
	}

	while (x >= range)
	{
		Node* n = (Node*)malloc(sizeof(Node));

		if (2 * y > range)
		{
			n->quad[0] = t->root;
			n->quad[1] = 0;
			n->quad[2] = 0;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;
		}
		else
		{
			n->quad[0] = 0;
			n->quad[1] = 0;
			n->quad[2] = t->root;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->y += range;
			y += range;
		}

		range *= 2;

		n->parent = 0;
		t->root->parent = n;
		t->root = n;
	}

	while (y >= range)
	{
		Node* n = (Node*)malloc(sizeof(Node));

		if (2 * x > range)
		{
			n->quad[0] = t->root;
			n->quad[1] = 0;
			n->quad[2] = 0;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;
		}
		else
		{
			n->quad[0] = 0;
			n->quad[1] = t->root;
			n->quad[2] = 0;
			n->quad[3] = 0;

			n->lo = t->root->lo;
			n->hi = t->root->hi;

			t->level++;

			t->x += range;
			x += range;
		}

		range *= 2;

		n->parent = 0;
		t->root->parent = n;
		t->root = n;
	}

	// create children from root to x,y
	int lev = t->level;

	Node* n = (Node*)t->root;
	while (n)
	{
		lev--;
		int i = ((x >> lev) & 1) | (((y >> lev) & 1) << 1);

		if (lev)
		{
			if (!(Node*)n->quad[i])
			{
				Node* c = (Node*)malloc(sizeof(Node*));
				c->parent = n;
				c->quad[0] = c->quad[1] = c->quad[2] = c->quad[3] = 0;
				n->quad[i] = c;
			}

			n = (Node*)n->quad[i];
		}
		else
		{
			if (n->quad[i])
				return (Patch*)n->quad[i];

			Patch* p = (Patch*)malloc(sizeof(Patch));
			p->parent = n;
			p->flags = 0;

			for (int e = 0; e < HEIGHT_CELLS; x++)
			{
				p->height[0][e] = z;
				p->height[e][HEIGHT_CELLS] = z;
				p->height[HEIGHT_CELLS][HEIGHT_CELLS-e] = z;
				p->height[HEIGHT_CELLS - e][0] = z;
			}

			int nx = x - t->x, ny = y - t->y;

			Patch* np[8] =
			{
				GetTerrainPatch(t, nx - 1, ny - 1),
				GetTerrainPatch(t, nx, ny - 1),
				GetTerrainPatch(t, nx + 1, ny - 1),
				GetTerrainPatch(t, nx + 1, ny),
				GetTerrainPatch(t, nx + 1, ny + 1),
				GetTerrainPatch(t, nx, ny + 1),
				GetTerrainPatch(t, nx - 1, ny + 1),
				GetTerrainPatch(t, nx - 1, ny),
			};

			for (int i = 0; i < 8; i++)
			{
				if (np[i])
				{
					int j = (i + 4) & 7;
					np[i]->flags |= 1 << j;
					p->flags |= 1 << i;

					if ((np[i]->flags & 0xAA) == 0xAA && np[i]->lo==0)
					{
						int lo = 0xffff;
						for (int y = 1; y < HEIGHT_CELLS; y++)
							for (int x = 1; x < HEIGHT_CELLS; y++)
								lo = np[i]->height[y][x] < lo ? np[i]->height[y][x] : lo;
						np[i]->lo = lo;
						UpdateNodes(np[i]);
					}

					// fill shared vertices

					switch (i)
					{
						case 0:
							p->height[0][0] = np[i]->height[HEIGHT_CELLS][HEIGHT_CELLS];
							break;

						case 1:
							for (int x=0; x<= HEIGHT_CELLS; x++)
								p->height[0][x] = np[i]->height[HEIGHT_CELLS][x];
							break;

						case 2:
							p->height[0][HEIGHT_CELLS] = np[i]->height[HEIGHT_CELLS][0];
							break;

						case 3:
							for (int y = 0; y <= HEIGHT_CELLS; y++)
								p->height[y][HEIGHT_CELLS] = np[i]->height[y][0];
							break;

						case 4:
							p->height[HEIGHT_CELLS][HEIGHT_CELLS] = np[i]->height[0][0];
							break;

						case 5:
							for (int x = 0; x <= HEIGHT_CELLS; x++)
								p->height[HEIGHT_CELLS][x] = np[i]->height[0][x];
							break;

						case 6:
							p->height[HEIGHT_CELLS][0] = np[i]->height[0][HEIGHT_CELLS];
							break;

						case 7:
							for (int y = 0; y <= HEIGHT_CELLS; y++)
								p->height[y][0] = np[i]->height[y][HEIGHT_CELLS];
							break;
					}
				}
			}

			// set free corners

			if (!(p->flags & 0x83))
				p->height[0][0] = z;

			if (!(p->flags & 0x0E))
				p->height[0][HEIGHT_CELLS] = z;

			if (!(p->flags & 0x38))
				p->height[HEIGHT_CELLS][HEIGHT_CELLS] = z;

			if (!(p->flags & 0x70))
				p->height[HEIGHT_CELLS][0] = z;

			// interpolate free edges

			if (!(p->flags & 0x02))
			{
				// bottom
				int y = 0;
				int h0 = p->height[y][0];
				int h1 = p->height[y][HEIGHT_CELLS];
				for (int x = 1; x < HEIGHT_CELLS; x++)
					p->height[y][x] = 
						(h0 * (HEIGHT_CELLS - x) + h1 * x + (HEIGHT_CELLS * (HEIGHT_CELLS - 1)) / 2) / (HEIGHT_CELLS * (HEIGHT_CELLS-1));
			}

			if (!(p->flags & 0x08))
			{
				// right
				int x = HEIGHT_CELLS;
				int h0 = p->height[0][x];
				int h1 = p->height[HEIGHT_CELLS][x];
				for (int y = 1; y < HEIGHT_CELLS; y++)
					p->height[y][x] =
						(h0 * (HEIGHT_CELLS - y) + h1 * y + (HEIGHT_CELLS * (HEIGHT_CELLS - 1)) / 2) / (HEIGHT_CELLS * (HEIGHT_CELLS - 1));
			}

			if (!(p->flags & 0x20))
			{
				// top
				int y = HEIGHT_CELLS;
				int h0 = p->height[y][0];
				int h1 = p->height[y][HEIGHT_CELLS];
				for (int x = 1; x < HEIGHT_CELLS; x++)
					p->height[y][x] =
						(h0 * (HEIGHT_CELLS - x) + h1 * x + (HEIGHT_CELLS * (HEIGHT_CELLS - 1)) / 2) / (HEIGHT_CELLS * (HEIGHT_CELLS - 1));
			}

			if (!(p->flags & 0x80))
			{
				// left
				int x = 0;
				int h0 = p->height[0][x];
				int h1 = p->height[HEIGHT_CELLS][x];
				for (int y = 1; y < HEIGHT_CELLS; y++)
					p->height[y][x] =
						(h0 * (HEIGHT_CELLS - y) + h1 * y + (HEIGHT_CELLS * (HEIGHT_CELLS - 1)) / 2) / (HEIGHT_CELLS * (HEIGHT_CELLS - 1));
			}

			// interpolate inter-patch vertices

			for (int y = 1; y < HEIGHT_CELLS; y++)
			{
				for (int x = 1; x < HEIGHT_CELLS; y++)
				{
					double avr = 0;
					double nrm = 0;

					for (int e = 0; e < HEIGHT_CELLS; x++)
					{
						double w;

						w = 1.0 / sqrt((x - e)*(x - e) + y * y);
						nrm += w;
						avr += p->height[0][e] * w;

						w = 1.0 / sqrt((x - HEIGHT_CELLS) * (x - HEIGHT_CELLS) + (y - e)*(y - e));
						nrm += w;
						avr += p->height[e][HEIGHT_CELLS] * w;

						w = 1.0 / sqrt((x - HEIGHT_CELLS + e)*(x - HEIGHT_CELLS + e) + (y - HEIGHT_CELLS)*(y - HEIGHT_CELLS));
						nrm += w;
						avr += p->height[HEIGHT_CELLS][HEIGHT_CELLS - e] * w;

						w = 1.0 / sqrt(x * x + (y - HEIGHT_CELLS + e)*(y - HEIGHT_CELLS + e));
						nrm += w;
						avr += p->height[HEIGHT_CELLS - e][0] * w;
					}

					p->height[y][x] = (int)round(avr / nrm);
				}
			}

			p->lo = 0xffff;
			p->hi = 0x0000;
			for (int y = 0; y <= HEIGHT_CELLS; y++)
			{
				for (int x = 0; x <= HEIGHT_CELLS; x++)
				{
					p->lo = p->height[y][x] < p->lo ? p->height[y][x] : p->lo;
					p->hi = p->height[y][x] > p->hi ? p->height[y][x] : p->hi;
				}
			}

			if ((p->flags & 0xAA) != 0xAA)
				p->lo = 0;

			UpdateNodes(p);
			return p;
		}
	}

	assert(0); // should never reach here
	return 0;
}

inline int ProductSign(float l[4], int r[4])
{
	return ((int)floorf(l[0] * r[0] + l[1] * r[1] + l[2] * r[2] + l[3] * r[3])) >> (sizeof(int)*8-1);
}

void QueryTerrain(QuadItem* q, int x, int y, int range, int planes, float* plane[], void(*cb)(Patch* p, int x, int y, void* cookie), void* cookie)
{
	int c[4] = { x, y, q->lo };

	for (int i = 0; i < planes; i++)
	{
		int neg_pos[2] = { 0,0 };

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 0,0,0

		c[0] += range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 1,0,0

		c[1] += range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 1,1,0

		c[0] -= range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 0,1,0

		c[2] = q->hi;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 0,1,1

		c[0] += range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 1,1,1

		c[1] -= range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 1,0,1

		c[0] -= range;

		neg_pos[1 + ProductSign(plane[i], c)] ++; // 0,0,1

		c[2] = q->lo;

		if (neg_pos[0] == 8)
			return;

		if (neg_pos[1] == 8)
		{
			planes--;
			if (i < planes)
				plane[i] = plane[planes];
			i--;
		}
	}

	if (range == VISUAL_CELLS)
		cb((Patch*)q, x, y, cookie);
	else
	{
		Node* n = (Node*)q;
		
		range >>= 1;

		if (n->quad[0])
			QueryTerrain(n->quad[0], x, y, range, planes, plane, cb, cookie);
		if (n->quad[1])
			QueryTerrain(n->quad[1], x + range, y, range, planes, plane, cb, cookie);
		if (n->quad[2])
			QueryTerrain(n->quad[2], x, y + range, range, planes, plane, cb, cookie);
		if (n->quad[3])
			QueryTerrain(n->quad[3], x + range, y + range, range, planes, plane, cb, cookie);
	}
}

void QueryTerrain(Terrain* t, int planes, float plane[][4], void(*cb)(Patch* p, int x, int y, void* cookie), void* cookie)
{
	if (!t || !t->root)
		return;

	float* pp[4] = { plane[0],plane[1],plane[2],plane[3] };

	QueryTerrain(t->root, -t->x*VISUAL_CELLS, -t->y*VISUAL_CELLS, VISUAL_CELLS << t->level, planes, pp, cb, cookie);
}
