#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "ge_original_bond_camera.h"

static struct player player;
struct player *g_CurrentPlayer;
static uint32_t frustum_publications;

void ge_original_bondview_update_frustum_planes_exact(void)
{
    ++frustum_publications;
}

struct player *ge_original_spawn_player_get(void)
{
    return &player;
}

static void identity(float matrix[4][4])
{
    unsigned index;
    memset(matrix, 0, sizeof(float) * 16U);
    for (index = 0U; index < 4U; ++index) matrix[index][index] = 1.0f;
}

static int near(float actual, float expected)
{
    return fabsf(actual - expected) < 1.0e-5f;
}

int main(void)
{
    GeOriginalBondCameraConfig config;
    GeOriginalBondCameraResult first;
    GeOriginalBondCameraResult second;
    Mtxf *first_view;
    Mtxf *first_world_to_view;
    float expected_scaley;
    float expected_scalex;
    float slope;
    float inverse_length;

    memset(&player, 0, sizeof(player));
    memset(&config, 0, sizeof(config));
    memset(&first, 0, sizeof(first));
    memset(&second, 0, sizeof(second));
    identity(first.view);
    identity(first.view_to_world);
    identity(first.projection);
    first.view[3][0] = -10.0f;
    first.view_to_world[3][0] = 10.0f;
    first.projection[2][2] = -1.25f;
    config.viewport_scale[0] = 640;
    config.viewport_scale[1] = 480;
    config.viewport_translation[0] = 800;
    config.viewport_translation[1] = 480;
    config.vertical_fov_degrees = 60.0f;
    config.perspective_aspect = 4.0f / 3.0f;
    config.near_distance = 100.0f;

    assert(ge_original_bond_camera_publish_live_player(&config, &first));
    assert(frustum_publications == 1U);
    assert(g_CurrentPlayer == &player);
    first_view = player.viewtoworldmtxf;
    first_world_to_view = player.field_10CC;
    assert(first_view != NULL && first_world_to_view != NULL);
    assert(player.field_10EC == NULL && player.field_10E8 == NULL);
    assert(first_view->m[3][0] == 10.0f);
    assert(first_world_to_view->m[3][0] == -10.0f);
    assert(player.projmatrixf != (Mtxf *)first.projection);

    assert(player.c_screenwidth == 320.0f);
    assert(player.c_screenheight == 240.0f);
    assert(player.c_halfwidth == 160.0f);
    assert(player.c_halfheight == 120.0f);
    assert(player.c_screenleft == 40.0f);
    assert(player.c_screentop == 0.0f);
    assert(player.c_perspnear == 100.0f);
    assert(player.c_perspfovy == 60.0f);
    assert(near(player.c_perspaspect, 4.0f / 3.0f));
    expected_scaley = tanf(60.0f * (float)M_PI / 360.0f) / 120.0f;
    expected_scalex = expected_scaley * (4.0f / 3.0f) * 120.0f / 160.0f;
    assert(near(player.c_scaley, expected_scaley));
    assert(near(player.c_scalex, expected_scalex));
    assert(near(player.c_recipscaley, 1.0f / expected_scaley));
    assert(near(player.c_recipscalex, 1.0f / expected_scalex));
    slope = 120.0f * expected_scaley;
    inverse_length = 1.0f / sqrtf(slope * slope + 1.0f);
    assert(player.c_cameratopnorm.x == 0.0f);
    assert(near(player.c_cameratopnorm.y, inverse_length));
    assert(near(player.c_cameratopnorm.z, slope * inverse_length));
    slope = -160.0f * expected_scalex;
    inverse_length = 1.0f / sqrtf(slope * slope + 1.0f);
    assert(near(player.c_cameraleftnorm.x, -inverse_length));
    assert(player.c_cameraleftnorm.y == 0.0f);
    assert(near(player.c_cameraleftnorm.z, -slope * inverse_length));

    identity(second.view);
    identity(second.view_to_world);
    identity(second.projection);
    second.view[3][0] = -20.0f;
    second.view_to_world[3][0] = 20.0f;
    assert(ge_original_bond_camera_publish_live_player(&config, &second));
    assert(frustum_publications == 2U);
    assert(player.field_10EC == first_view);
    assert(player.field_10E8 == first_world_to_view);
    assert(player.viewtoworldmtxf != first_view);
    assert(player.field_10CC != first_world_to_view);
    assert(first_view->m[3][0] == 10.0f);
    assert(first_world_to_view->m[3][0] == -10.0f);
    assert(player.viewtoworldmtxf->m[3][0] == 20.0f);
    assert(player.field_10CC->m[3][0] == -20.0f);

    config.vertical_fov_degrees = INFINITY;
    assert(!ge_original_bond_camera_publish_live_player(&config, &second));
    assert(frustum_publications == 2U);
    assert(player.viewtoworldmtxf->m[3][0] == 20.0f);
    puts("original live camera publication passed");
    return 0;
}
