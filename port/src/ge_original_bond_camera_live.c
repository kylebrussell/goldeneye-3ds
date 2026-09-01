#include "ge_original_bond_camera.h"
#include "ge_original_player_spawn_internal.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"

#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif
#ifndef M_U32_MAX_VALUE_F
#define M_U32_MAX_VALUE_F 4294967296.0f
#endif

/* The original setters retain the previous matrix pointer.  Ping-pong storage
 * is required here because GeOriginalBondCameraResult is caller-owned and a
 * single static matrix would make the previous and current pointers alias. */
static Mtxf ge_live_world_to_view[2];
static Mtxf ge_live_view_to_world[2];
static Mtxf ge_live_projection;

/* Strong in the full canonical chr/prop support slice.  Focused camera-only
 * host tests intentionally omit that large translation unit; the weak
 * boundary keeps those tests isolated while the ARM runtime always executes
 * the unchanged bondviewUpdateFrustumPlanes body against the shared player
 * and g_CamFrustum* globals. */
extern void ge_original_bondview_update_frustum_planes_exact(void)
    __attribute__((weak));
extern struct player *g_CurrentPlayer __attribute__((weak));

static int ge_camera_matrix_finite(const float matrix[4][4])
{
    unsigned row;
    unsigned column;
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            if (!isfinite(matrix[row][column])) return 0;
    return 1;
}

int ge_original_bond_camera_publish_live_player(
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *result)
{
    struct player *player = ge_original_spawn_player_get();
    Mtxf *world_to_view;
    Mtxf *view_to_world;
    float width;
    float height;
    float half_fov;
    float lod_scale;
    float slope;
    float inverse_length;

    if (player == NULL || config == NULL || result == NULL
            || !ge_camera_matrix_finite(result->view)
            || !ge_camera_matrix_finite(result->view_to_world)
            || !ge_camera_matrix_finite(result->projection)
            || config->viewport_scale[0] <= 0
            || config->viewport_scale[1] <= 0
            || !isfinite(config->vertical_fov_degrees)
            || !isfinite(config->perspective_aspect)
            || !isfinite(config->near_distance)
            || !(config->vertical_fov_degrees > 0.0f)
            || !(config->perspective_aspect > 0.0f)
            || !(config->near_distance > 0.0f)) return 0;

    world_to_view = player->field_10CC == &ge_live_world_to_view[0]
        ? &ge_live_world_to_view[1] : &ge_live_world_to_view[0];
    view_to_world = player->viewtoworldmtxf == &ge_live_view_to_world[0]
        ? &ge_live_view_to_world[1] : &ge_live_view_to_world[0];
    memcpy(world_to_view->m, result->view, sizeof(world_to_view->m));
    memcpy(view_to_world->m, result->view_to_world,
           sizeof(view_to_world->m));
    memcpy(ge_live_projection.m, result->projection,
           sizeof(ge_live_projection.m));
    player->field_10EC = player->viewtoworldmtxf;
    player->viewtoworldmtxf = view_to_world;
    player->field_10E8 = player->field_10CC;
    player->field_10CC = world_to_view;
    player->projmatrixf = &ge_live_projection;

    /* Exact currentPlayerSetScreenSize/Position/Perspective semantics. N64
     * viewport scale is twice the pixel extent and translation is in
     * quarter-pixel units. */
    width = (float)config->viewport_scale[0] * 0.5f;
    height = (float)config->viewport_scale[1] * 0.5f;
    player->c_screenwidth = width;
    player->c_screenheight = height;
    player->c_halfwidth = width * 0.5f;
    player->c_halfheight = height * 0.5f;
    player->c_screenleft = ((float)config->viewport_translation[0]
        - (float)config->viewport_scale[0]) * 0.25f;
    player->c_screentop = ((float)config->viewport_translation[1]
        - (float)config->viewport_scale[1]) * 0.25f;
    player->c_perspnear = config->near_distance;
    player->c_perspfovy = config->vertical_fov_degrees;
    player->c_perspaspect = config->perspective_aspect;

    /* Exact currentPlayerSetCameraScale equations from bondview.c. */
    half_fov = mDegToHalfRad(player->c_perspfovy);
    player->c_scaley = sinf(half_fov)
        / (cosf(half_fov) * player->c_halfheight);
    player->c_scalex = (player->c_scaley * player->c_perspaspect
        * player->c_halfheight) / player->c_halfwidth;
    player->c_recipscalex = 1.0f / player->c_scalex;
    player->c_recipscaley = 1.0f / player->c_scaley;
    player->c_scalelod = player->c_scaley;
    player->c_scalelod60 = sinf(DegToRad(30.0f))
        / (cosf(DegToRad(30.0f)) * 120.0f);
    player->c_lodscalez = player->c_scalelod / player->c_scalelod60;
    lod_scale = player->c_lodscalez * M_U16_MAX_VALUE_F;
    player->c_lodscalezu32 = lod_scale > M_U32_MAX_VALUE_F
        ? UINT32_MAX : (u32)lod_scale;

    slope = player->c_halfheight * player->c_scaley;
    inverse_length = 1.0f / sqrtf(slope * slope + 1.0f);
    player->c_cameratopnorm.x = 0.0f;
    player->c_cameratopnorm.y = inverse_length;
    player->c_cameratopnorm.z = slope * inverse_length;

    slope = -player->c_halfwidth * player->c_scalex;
    inverse_length = 1.0f / sqrtf(slope * slope + 1.0f);
    player->c_cameraleftnorm.x = -inverse_length;
    player->c_cameraleftnorm.y = 0.0f;
    player->c_cameraleftnorm.z = -slope * inverse_length;
    /* The intro camera can publish before bondviewProcessInput has installed
     * the single-player alias.  The exact frustum body dereferences that
     * canonical global, so establish the same player identity here without
     * replacing a different active-player selection. */
    if ((uintptr_t)(void *)&g_CurrentPlayer != 0U
            && g_CurrentPlayer == NULL)
        g_CurrentPlayer = player;
    if (ge_original_bondview_update_frustum_planes_exact != NULL
            && (uintptr_t)(void *)&g_CurrentPlayer != 0U
            && g_CurrentPlayer == player)
        ge_original_bondview_update_frustum_planes_exact();
    return 1;
}
