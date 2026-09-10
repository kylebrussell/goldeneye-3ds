#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "ge_original_multiplayer_views.h"
#include "ge_3ds_stereo.h"

static struct player players[4];
struct player *g_CurrentPlayer;
struct player *g_playerPointers[4];
static Mtxf frustum_camera;

struct player *ge_original_spawn_player_get(void) { return g_CurrentPlayer; }
void ge_original_bondview_update_frustum_planes_exact(void)
{
    frustum_camera = *g_CurrentPlayer->field_10CC;
}

typedef struct Fixture {
    uint32_t count, local, selected, visible, simulation_mask, captures, restores;
    int fail_select, fail_visibility, fail_capture, fail_restore;
    int duplicate_owner;
    struct player *replacement_owner;
    coord3d transient[4];
    coord3d captured[4];
} Fixture;

static int select_player(void *context, uint32_t slot)
{
    Fixture *f = context;
    if (f->fail_select) return 0;
    f->selected = slot;
    g_CurrentPlayer = &players[f->duplicate_owner ? 0 : slot];
    if (f->replacement_owner != NULL) g_CurrentPlayer = f->replacement_owner;
    return 1;
}

static int publish_visibility(void *context, uint32_t slot,
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *camera)
{
    Fixture *f = context;
    assert(f->selected == slot);
    assert(config->room == slot + 1);
    assert(memcmp(g_CurrentPlayer->field_10CC->m, camera->view,
        sizeof(camera->view)) == 0);
    assert(memcmp(frustum_camera.m, camera->view, sizeof(camera->view)) == 0);
    if (f->fail_visibility) return 0;
    f->visible = slot;
    return 1;
}

/* Instrument the caller-owned props boundary. This test uses original camera
 * and matrix producers; it does not substitute this fixture for playerTick
 * or claim to test character animation/damage. */
static void caller_props_and_guns(Fixture *f, uint32_t slot)
{
    uint32_t body;
    assert(f->selected == slot && f->visible == slot);
    assert((f->simulation_mask & (1U << slot)) == 0);
    f->simulation_mask |= 1U << slot;
    for (body = 0; body < f->count; ++body) {
        coord3d position = {.x = 30.0f * body, .y = 75.0f, .z = -400.0f};
        mtx4TransformVecInPlace(g_CurrentPlayer->field_10CC, &position);
        f->transient[body] = position;
    }
}

static int capture_local(void *context, uint32_t slot,
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *camera)
{
    Fixture *f = context;
    (void)config;
    assert(slot == f->local && f->selected == slot && f->visible == slot);
    assert((f->simulation_mask & (1U << slot)) != 0);
    assert(memcmp(camera->view, g_CurrentPlayer->field_10CC->m,
        sizeof(camera->view)) == 0);
    if (f->fail_capture) return 0;
    memcpy(f->captured, f->transient, sizeof(f->captured));
    ++f->captures;
    return 1;
}

static int restore_local(void *context, uint32_t slot,
    const GeOriginalBondCameraConfig *config,
    const GeOriginalBondCameraResult *camera)
{
    Fixture *f = context;
    assert(f->captures == 1 && slot == f->local && f->selected == slot);
    assert(config->room == slot + 1);
    assert(memcmp(frustum_camera.m, camera->view, sizeof(camera->view)) == 0);
    if (f->fail_restore) return 0;
    f->visible = slot;
    ++f->restores;
    return 1;
}

static GeOriginalMultiplayerViewProviders providers(Fixture *f)
{
    GeOriginalMultiplayerViewProviders p = {
        f, select_player, publish_visibility, capture_local, restore_local};
    return p;
}

static void camera_for(uint32_t slot, uint32_t frame,
    GeOriginalBondCameraConfig *config, GeOriginalBondCameraResult *camera)
{
    memset(config, 0, sizeof(*config));
    config->camera_position[0] = 40.0f * slot + (float)frame;
    config->camera_position[1] = 150.0f;
    config->camera_look_direction[0] = 0.03f * slot;
    config->camera_look_direction[2] = -1.0f;
    config->camera_up[1] = 1.0f;
    config->room_position_scale = config->camera_local_scale = 1.0f;
    config->visibility_scale = 1.0f;
    config->viewport_scale[0] = 640;
    config->viewport_scale[1] = 480;
    config->viewport_translation[0] = 800;
    config->viewport_translation[1] = 480;
    config->room = (uint8_t)(slot + 1);
    assert(ge_original_bond_camera_set_perspective(config,
        60.0f, 4.0f / 3.0f, 1.0f, 10000.0f) == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(ge_original_bond_camera_run(config, camera) == GE_ORIGINAL_BOND_CAMERA_OK);
}

static void test_match(uint32_t count, uint32_t local)
{
    uint32_t frame, pass, slot;
    memset(players, 0, sizeof(players));
    for (slot = 0; slot < 4; ++slot) g_playerPointers[slot] = &players[slot];
    for (frame = 0; frame < 24; ++frame) {
        Fixture f = {.count = count, .local = local};
        GeOriginalMultiplayerViewProviders p = providers(&f);
        GeOriginalMultiplayerViews views;
        GeOriginalBondCameraConfig config;
        GeOriginalBondCameraResult camera;
        struct player before[4];
        Mtxf frustum_before;
        coord3d expected[4];
        Ge3dsStereoConfig stereo;
        assert(ge_original_multiplayer_views_begin_frame(&views, count, local, &p)
            == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
        for (pass = 0; pass < count; ++pass) {
            /* Rotate and reverse so every local slot appears first/middle/last. */
            slot = frame & 1 ? (frame + count - pass) % count : (frame + pass) % count;
            camera_for(slot, frame, &config, &camera);
            assert(ge_original_multiplayer_views_begin_shooter(&views, slot, &config, &camera)
                == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
            caller_props_and_guns(&f, slot);
            if (slot == local) {
                memcpy(expected, f.transient, sizeof(expected));
                assert(ge_original_multiplayer_views_capture_local(&views)
                    == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
            }
            /* The adapter must own the local camera, not retain caller stack pointers. */
            memset(&config, 0xa5, sizeof(config));
            memset(&camera, 0xa5, sizeof(camera));
        }
        memcpy(before, players, sizeof(before));
        assert(ge_original_multiplayer_views_finish_local(&views) == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
        assert(memcmp(before, players, sizeof(before)) == 0);
        assert(g_CurrentPlayer == &players[local] && f.visible == local);
        assert(f.simulation_mask == (1U << count) - 1U);
        assert(f.captures == 1 && f.restores == 1);
        assert(memcmp(expected, f.captured, sizeof(expected)) == 0);
        frustum_before = frustum_camera;
        ge_3ds_stereo_config_defaults(&stereo);
        for (slot = 0; slot < count; ++slot) {
            float left_constant, left_coefficient, right_constant, right_coefficient;
            float depth = -f.captured[slot].z;
            float mono = f.captured[slot].x / depth;
            float left, right;
            assert(depth > 0);
            ge_3ds_stereo_pixel_shift(&stereo, 1.0f, -1,
                GE_3DS_STEREO_LAYER_WORLD, &left_constant, &left_coefficient);
            ge_3ds_stereo_pixel_shift(&stereo, 1.0f, 1,
                GE_3DS_STEREO_LAYER_WORLD, &right_constant, &right_coefficient);
            left = mono + left_constant / depth + left_coefficient;
            right = mono + right_constant / depth + right_coefficient;
            assert(fabsf((left + right) * 0.5f - mono) < 1.0e-5f);
            assert(left != right);
        }
        assert(memcmp(before, players, sizeof(before)) == 0);
        assert(memcmp(&frustum_before, &frustum_camera, sizeof(frustum_before)) == 0);
        assert(f.simulation_mask == (1U << count) - 1U && f.captures == 1);
        assert(ge_original_multiplayer_views_finish_local(&views)
            == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    }
}

static void test_failures(void)
{
    Fixture f = {.count = 2, .local = 0};
    GeOriginalMultiplayerViewProviders p = providers(&f);
    GeOriginalMultiplayerViews views;
    GeOriginalBondCameraConfig config;
    GeOriginalBondCameraResult camera;
    camera_for(0, 0, &config, &camera);
    assert(ge_original_multiplayer_views_begin_frame(&views, 5, 0, &p)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ARGUMENT);
    assert(ge_original_multiplayer_views_begin_frame(&views, 2, 0, &p)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
    assert(ge_original_multiplayer_views_capture_local(&views)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    assert(ge_original_multiplayer_views_finish_local(&views)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_OK);
    assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    assert(ge_original_multiplayer_views_begin_shooter(&views, 1, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER); /* local capture required */
    caller_props_and_guns(&f, 0);
    f.fail_capture = 1;
    assert(ge_original_multiplayer_views_capture_local(&views)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_CAPTURE_FAILED);
    assert(ge_original_multiplayer_views_capture_local(&views)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    f.fail_capture = 0;
    f.fail_select = 1;
    assert(ge_original_multiplayer_views_begin_frame(&views, 2, 0, &p) == 0);
    assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
    f.fail_select = 0;
    f.fail_visibility = 1;
    assert(ge_original_multiplayer_views_begin_frame(&views, 2, 0, &p) == 0);
    assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_VISIBILITY_FAILED);
    f.fail_visibility = 0;
    assert(ge_original_multiplayer_views_begin_frame(&views, 2, 0, &p) == 0);
    config.vertical_fov_degrees = INFINITY;
    assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera)
        == GE_ORIGINAL_MULTIPLAYER_VIEW_CAMERA_FAILED);
}

static void test_finish_failures(void)
{
    static struct player replacement;
    unsigned mode;
    for (mode = 0; mode < 5; ++mode) {
        Fixture f = {.count = 2, .local = 0};
        GeOriginalMultiplayerViewProviders p = providers(&f);
        GeOriginalMultiplayerViews views;
        GeOriginalBondCameraConfig config;
        GeOriginalBondCameraResult camera;
        GeOriginalMultiplayerViewStatus expected;
        assert(ge_original_multiplayer_views_begin_frame(&views, 2, 0, &p) == 0);
        camera_for(0, 0, &config, &camera);
        assert(ge_original_multiplayer_views_begin_shooter(&views, 0, &config, &camera) == 0);
        caller_props_and_guns(&f, 0);
        assert(ge_original_multiplayer_views_capture_local(&views) == 0);
        camera_for(1, 0, &config, &camera);
        if (mode == 4) {
            f.duplicate_owner = 1;
            assert(ge_original_multiplayer_views_begin_shooter(&views, 1, &config, &camera)
                == GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED);
            continue;
        }
        assert(ge_original_multiplayer_views_begin_shooter(&views, 1, &config, &camera) == 0);
        caller_props_and_guns(&f, 1);
        if (mode == 0) {
            f.fail_restore = 1;
            expected = GE_ORIGINAL_MULTIPLAYER_VIEW_VISIBILITY_FAILED;
        } else if (mode == 1) {
            f.replacement_owner = &replacement;
            expected = GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED;
        } else if (mode == 2) {
            f.fail_select = 1;
            expected = GE_ORIGINAL_MULTIPLAYER_VIEW_SELECTION_FAILED;
        } else {
            players[0].field_10CC = NULL;
            expected = GE_ORIGINAL_MULTIPLAYER_VIEW_CAMERA_FAILED;
        }
        assert(ge_original_multiplayer_views_finish_local(&views) == expected);
        assert(ge_original_multiplayer_views_finish_local(&views)
            == GE_ORIGINAL_MULTIPLAYER_VIEW_INVALID_ORDER);
    }
}

int main(void)
{
    uint32_t count, local;
    for (count = 2; count <= 4; ++count)
        for (local = 0; local < count; ++local) test_match(count, local);
    test_failures();
    test_finish_failures();
    puts("multiplayer views: 216 shuffled frames, original cameras, owned local capture, stereo baseline and failure ordering passed");
    return 0;
}
