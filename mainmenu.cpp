#include "mainmenu.h"
#include "enemygen.h"
#include "fast_rand.h"
#include "font1.h"

extern Game* game;
extern Terrain* terrain;
extern World* world;
extern Material mat[256];
extern char base_path[1024];

static int  game_loading = 0; // 0-not_loaded, 1-load requested, 2-loading, 3-loaded
static bool show_continue = false;

static uint64_t mainmenu_stamp = 0;

struct Manifest // till we have no real manifests
{
    const char* sprite_icon;
    const char* title_icon;
    const char* desc;
    const char* a3d;
    const char* js;

    /*
    // this should come from js execution:
    // ak.setWater
    // ak.setDir
    // ak.setYaw
    // ak.setPos
    // ak.setLight

    float water = 55;
    float dir = 0;
    float yaw = 45;
    float pos[3];
    float lt[4];
    */
};

static int manifests = 1;


static void ResetGame()
{
    // let's test fresh restart
    FreeGame(game);

    if (terrain)
        DeleteTerrain(terrain);

    if (world)
        DeleteWorld(world);

    PurgeItemInstCache();

    // we would also RESET:
    // - akAPI_This to {}
    // - all CB handlers to null
}

void MainMenu_Render(uint64_t _stamp, AnsiCell* ptr, int width, int height)
{
    if (game_loading == 0)
        show_continue = false;
    if (game_loading == 3)
        show_continue = true;

    mainmenu_stamp = _stamp;
    int s = width*height;
    for (int i=0; i<s; i++)
        *((uint32_t*)ptr+i) = fast_rand() | (fast_rand()<<15);// | (fast_rand()<<30);

    int x = 5, y = height - 5;
    Font1Paint(ptr,width,height, x,y, "ASCIICKER", FONT1_GOLD_SKIN);

    int w,h;
    Font1Size("ASCIICKER",&w,&h);
    y -= h;

    Font1Paint(ptr,width,height, x,y, "MAIN MENU", FONT1_PINK_SKIN);
    y-= 2*h;

    if (show_continue)
    {
        Font1Paint(ptr,width,height, x,y, "1.", FONT1_GOLD_SKIN);
        Font1Paint(ptr,width,height, x+7,y, "CONTINUE GAME", FONT1_GREY_SKIN);
        y -= h;

        Font1Paint(ptr,width,height, x,y, "2.", FONT1_GOLD_SKIN);
        Font1Paint(ptr,width,height, x+7,y, "START NEW GAME", FONT1_GREY_SKIN);
        y -= h;
    }
    else
    {
        Font1Paint(ptr,width,height, x,y, "1.", FONT1_GOLD_SKIN);
        Font1Paint(ptr,width,height, x+7,y, "START NEW GAME", FONT1_GREY_SKIN);
        y -= h;
    }

    if (game_loading == 1)
    {
        if (y > 5)
            y = 5;

        Font1Size("LOADING",&w,&h);
        Font1Paint(ptr,width,height, (width-w)/2,y, "LOADING", FONT1_PINK_SKIN);

        game_loading = 2;
    }
    else
    if (game_loading == 2)
    {
        float water = 55;
        float dir = 0;
        float yaw = 45;
        float pos[3] = {0,15,0};
        float lt[4] = {1,0,1,.5};

        {
            // here the path should be taken from the module manifest
            // ...

            char a3d_path[1024 + 20];
            sprintf(a3d_path,"%sa3d/game_map_y8.a3d", base_path);
            FILE* f = fopen(a3d_path, "rb");

            // TODO:
            // if GameServer* gs != 0
            // DO NOT LOAD ITEMS!
            // we will receive them from server

            if (f)
            {
                terrain = LoadTerrain(f);

                if (terrain)
                {
                    for (int i = 0; i < 256; i++)
                    {
                        if (fread(mat[i].shade, 1, sizeof(MatCell) * 4 * 16, f) != sizeof(MatCell) * 4 * 16)
                            break;
                    }

                    world = LoadWorld(f, false);
                    if (world)
                    {
                        // reload meshes too
                        Mesh* m = GetFirstMesh(world);

                        while (m)
                        {
                            char mesh_name[256];
                            GetMeshName(m, mesh_name, 256);
                            char obj_path[4096];
                            sprintf(obj_path, "%smeshes/%s", base_path, mesh_name);
                            if (!UpdateMesh(m, obj_path))
                            {
                                // what now?
                                // missing mesh file!
                            }

                            m = GetNextMesh(m);
                        }

                        LoadEnemyGens(f);
                    }
                }

                fclose(f);
            }

            // if (!terrain || !world)
            //    return -1;

            // add meshes from library that aren't present in scene file
            char mesh_dirname[4096];
            sprintf(mesh_dirname, "%smeshes", base_path);
            //a3dListDir(mesh_dirname, MeshScan, mesh_dirname);

            // this is the only case when instances has no valid bboxes yet
            // as meshes weren't present during their creation
            // now meshes are loaded ...
            // so we need to update instance boxes with (,true)

            if (world)
            {
                RebuildWorld(world, true);
                #ifdef DARK_TERRAIN
                // TODO: add Patch Iterator holding stack of parents
                // and single Patch dark updater
                UpdateTerrainDark(terrain, world, lt, false);
                #endif
            }
        }

        // here we should also execute some js module (given in manifest)
        // ...

        InitGame(game, water, pos, yaw, dir, lt, mainmenu_stamp);
        game_loading = 3;
        game->main_menu = false;
    }
}

void MainMenu_OnSize(int w, int h, int fw, int fh)
{
}

void MainMenu_OnKeyb(GAME_KEYB keyb, int key)
{
    if (keyb == GAME_KEYB::KEYB_CHAR)
    {
        if (game_loading==3)
        {
            if (key == '1')
                game->main_menu = false;
            if (key == '2')
            {
                ResetGame();
                game_loading = 1;
            }
        }
        else
        if (game_loading==0)
        {
            if (key == '1')
                game_loading = 1;
        }
    }
}

void MainMenu_OnMouse(GAME_MOUSE mouse, int x, int y)
{
    if (mouse == GAME_MOUSE::MOUSE_LEFT_BUT_DOWN ||
        mouse == GAME_MOUSE::MOUSE_RIGHT_BUT_DOWN ||
        mouse == GAME_MOUSE::MOUSE_MIDDLE_BUT_DOWN)
    {
        if (game_loading==3)
            game->main_menu = false;
        else
        if (game_loading==0)
            game_loading = 1;
    }
}

void MainMenu_OnTouch(GAME_TOUCH touch, int id, int x, int y)
{
    if (touch == GAME_TOUCH::TOUCH_BEGIN)
    {
        if (game_loading==3)
            game->main_menu = false;
        else
        if (game_loading==0)
            game_loading = 1;
    }
}

void MainMenu_OnFocus(bool set)
{
}

void MainMenu_OnPadMount(bool connect)
{
}

void MainMenu_OnPadButton(int b, bool down)
{
    if (game_loading==3)
        game->main_menu = false;
    else
    if (game_loading==0)
        game_loading = 1;
}

void MainMenu_OnPadAxis(int a, int16_t pos)
{
}
