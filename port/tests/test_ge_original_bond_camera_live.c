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
struct player *g_playerPointers[4];
static struct player roster[4];
static uint32_t frustum_publications;

void ge_original_bondview_update_frustum_planes_exact(void)
{
    ++frustum_publications;
}

struct player *ge_original_spawn_player_get(void)
{
    return g_CurrentPlayer != NULL ? g_CurrentPlayer : &player;
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

static void test_multiplayer_isolation(GeOriginalBondCameraConfig config)
{
    static const unsigned order[4] = {2, 0, 3, 1};
    GeOriginalBondCameraResult result;
    unsigned frame, pass, slot;
    config.vertical_fov_degrees = 60.0f;
    memset(roster, 0, sizeof(roster));
    memset(&result, 0, sizeof(result));
    identity(result.view);
    identity(result.view_to_world);
    identity(result.projection);
    for (slot = 0; slot < 4; ++slot) g_playerPointers[slot] = &roster[slot];
    for (frame = 0; frame < 24; ++frame) {
        for (pass = 0; pass < 4; ++pass) {
            Mtxf saved[4][5];
            unsigned other;
            slot = order[(pass + frame) % 4];
            for (other = 0; other < 4; ++other) {
                struct player *p = &roster[other];
                if (p->field_10CC == NULL) continue;
                saved[other][0] = *p->field_10CC;
                saved[other][1] = *p->viewtoworldmtxf;
                saved[other][2] = *p->projmatrixf;
                if (p->field_10E8 != NULL) saved[other][3] = *p->field_10E8;
                if (p->field_10EC != NULL) saved[other][4] = *p->field_10EC;
            }
            g_CurrentPlayer = &roster[slot];
            result.view[3][0] = -(float)(100 * slot + frame);
            result.view_to_world[3][0] = -result.view[3][0];
            result.projection[0][0] = (float)(slot + 1);
            assert(ge_original_bond_camera_publish_live_player(&config, &result));
            for (other = 0; other < 4; ++other) {
                struct player *p = &roster[other];
                if (other == slot || p->field_10CC == NULL) continue;
                assert(memcmp(&saved[other][0], p->field_10CC, sizeof(Mtxf)) == 0);
                assert(memcmp(&saved[other][1], p->viewtoworldmtxf, sizeof(Mtxf)) == 0);
                assert(memcmp(&saved[other][2], p->projmatrixf, sizeof(Mtxf)) == 0);
                if (p->field_10E8 != NULL)
                    assert(memcmp(&saved[other][3], p->field_10E8, sizeof(Mtxf)) == 0);
                if (p->field_10EC != NULL)
                    assert(memcmp(&saved[other][4], p->field_10EC, sizeof(Mtxf)) == 0);
            }
            if (frame != 0) {
                assert(g_CurrentPlayer->field_10E8->m[3][0]
                    == -(float)(100 * slot + frame - 1));
                assert(g_CurrentPlayer->field_10EC->m[3][0]
                    == (float)(100 * slot + frame - 1));
            }
        }
        for (slot = 0; slot < 4; ++slot) {
            struct player before;
            uint32_t publications = frustum_publications;
            g_CurrentPlayer = &roster[slot];
            memcpy(&before, g_CurrentPlayer, sizeof(before));
            assert(ge_original_bond_camera_restore_live_player());
            assert(frustum_publications == publications + 1);
            assert(memcmp(&before, g_CurrentPlayer, sizeof(before)) == 0);
            assert(g_CurrentPlayer->field_10CC->m[3][0]
                == -(float)(100 * slot + frame));
        }
    }
    g_CurrentPlayer = &roster[0];
    g_CurrentPlayer->field_10CC = NULL;
    assert(!ge_original_bond_camera_restore_live_player());
    g_CurrentPlayer = NULL;
    assert(!ge_original_bond_camera_restore_live_player());
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
    test_multiplayer_isolation(config);
    puts("original live camera publication: solo and four-player history isolation passed");
    return 0;
}
