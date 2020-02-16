
#include <stdint.h>

#include "terrain.h"
#include "game.h"
#include "sprite.h"
#include "mesh.h"
#include "render.h"

#include <time.h>
uint64_t GetTime()
{
	static timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

Game* game = 0;
Terrain* terrain = 0;
World* world = 0;
AnsiCell* render_buf = 0;

Material mat[256];
void* GetMaterialArr()
{
    return mat;
}

int main()
{
    float water = 55.0f;
    float yaw = 45;
    float dir = 0;
    float pos[3] = {0,15,0};
    uint64_t stamp;

    LoadSprites();

    {
        FILE* f = fopen("a3d/game_sprites.a3d","rb");

        if (f)
        {
            terrain = LoadTerrain(f);
            
            if (terrain)
            {
                for (int i=0; i<256; i++)
                {
                    if ( fread(mat[i].shade,1,sizeof(MatCell)*4*16,f) != sizeof(MatCell)*4*16 )
                        break;
                }

                world = LoadWorld(f);
                if (world)
                {
                    // reload meshes too
                    Mesh* m = GetFirstMesh(world);

                    while (m)
                    {
                        char mesh_name[256];
                        GetMeshName(m,mesh_name,256);
                        char obj_path[4096];
                        sprintf(obj_path,"%smeshes/%s","./"/*root_path*/,mesh_name);
                        if (!UpdateMesh(m,obj_path))
                        {
                            // what now?
                            // missing mesh file!
                            printf("failed to load mesh %s\n", mesh_name);
                            return -5;
                        }

                        m = GetNextMesh(m);
                    }
                }
                else
                {
                    printf("failed to load world\n");
                    return -4;
                }
            }
            else
            {
                printf("failed to load terrain\n");
                return -3;
            }

            fclose(f);
        }
        else
        {
            printf("failed to open game.a3d\n");
            return -2;
        }

        // if (!terrain || !world)
        //    return -1;

        // add meshes from library that aren't present in scene file
        char mesh_dirname[4096];
        sprintf(mesh_dirname,"%smeshes","./"/*root_path*/);
        //a3dListDir(mesh_dirname, MeshScan, mesh_dirname);

        // this is the only case when instances has no valid bboxes yet
        // as meshes weren't present during their creation
        // now meshes are loaded ...
        // so we need to update instance boxes with (,true)

        if (world)
            RebuildWorld(world, true);
    }

    render_buf = (AnsiCell*)malloc(sizeof(AnsiCell) * 160 * 160);
    if (!render_buf)
    {
        printf("failed to allocate render buffer\n");
        return -7;
    }

    stamp = GetTime();
    game = CreateGame(water,pos,yaw,dir,stamp);

    printf("all ok\n");
    return 0;
}

extern "C"
{
    void* Render(int width, int height)
    {
        game->Render(GetTime(),render_buf,width,height);
        return render_buf;
    }

    void Size(int w, int h, int fw, int fh)
    {
        game->OnSize(w,h,fw,fh);
    }

    void Keyb(int type, int val)
    {
        game->OnKeyb((Game::GAME_KEYB)type,val);
    }

    void Mouse(int type, int x, int y)
    {
        game->OnMouse((Game::GAME_MOUSE)type, x, y);
    }

    void Touch(int type, int id, int x, int y)
    {
        game->OnTouch((Game::GAME_TOUCH)type, id, x, y);
    }

    void Focus(int set)
    {
        game->OnFocus(set!=0);
    }
}