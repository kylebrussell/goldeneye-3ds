#include "ge_original_covert_modem_fire.h"
#include "ge_original_covert_modem_projectile.h"
#include "ge_original_default_object.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_player_thrown_object.h"
#include "ge_original_prop_state.h"
#include "ge_original_bond_input_provider.h"
#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
#include "ge_dam_camera.h"
#include "ge_original_bond_camera.h"
#include "ge_original_dam_guard_model.h"
#include "ge_original_dam_guards.h"
#include "ge_original_first_person_assets.h"
#include "ge_original_first_person_scene.h"
#include "ge_original_gun_frame_arena.h"
#include "ge_original_gun_live.h"
#endif
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bg.h"
#include "game/bondview.h"
#include "game/chrai.h"
#include "game/dyn.h"
#include "game/gun.h"
#include "game/player.h"

bg_portal_data_entry *g_BgPortals;
extern f32 room_data_float1;
s32 D_80048380 = 77;

#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
extern void ge_original_gun_update_and_fire_both_hands_exact(void);
extern void ge_original_bond_input_initialize_player_hands(
    void *right_buffer, void *left_buffer);
static unsigned char exact_gun_frame_arena[65536];
static unsigned char exact_right_hand_buffer[
    GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];
static unsigned char exact_left_hand_buffer[
    GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE];
static ModelNode *exact_host_switches[64] __attribute__((aligned(16)));
extern GunModelFileRecord gitem_structs[];

static uint64_t exact_first_person_profile_clock(void *context)
{
    (void)context;
    return (uint64_t)clock();
}
#endif

s32 sub_GAME_7F0B9F14(s32 portal, coord3d *from, coord3d *to)
{
    (void)portal;
    (void)from;
    (void)to;
    assert(!"portal test reached in terminator-only world");
    return 0;
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    unsigned char *bytes;
    assert(file != NULL);
    assert(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0L);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    *size = (size_t)length;
    bytes = malloc(*size);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, *size, file) == *size);
    assert(fclose(file) == 0);
    return bytes;
}

static int tile_rgb(void *context, void *stan, float x, float z,
                    uint8_t rgb[3])
{
    (void)context;
    assert(stan != NULL && isfinite(x) && isfinite(z));
    rgb[0] = 160U;
    rgb[1] = 144U;
    rgb[2] = 128U;
    return 1;
}

#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
static void assert_first_person_cached_transform_byte_exact(
    const GeOriginalFirstPersonSceneCache *cache,
    const GeDamRoomWorldVertex *published)
{
    size_t input_index;
    for (input_index = 0U; input_index < cache->input_count; ++input_index) {
        const GeOriginalModelSceneInput *input = &cache->inputs[input_index];
        const float (*matrices)[4][4] = cache->quantized_matrices
            + cache->input_quantized_matrix_offsets[input_index];
        const size_t vertex_base = cache->input_vertex_offsets[input_index];
        size_t local_vertex;
        for (local_vertex = 0U;
                local_vertex < cache->queries[input_index].required_vertex_count;
                ++local_vertex) {
            const size_t vertex_index = vertex_base + local_vertex;
            const GeDamRoomWorldVertex *source =
                &cache->template_vertices[vertex_index];
            const uint16_t matrix_index =
                cache->template_matrix_indices[vertex_index];
            const float object[4] = {
                (float)source->source.x, (float)source->source.y,
                (float)source->source.z, 1.0f
            };
            float eye[4];
            float world[3];
            size_t axis;
            size_t row;
            for (axis = 0U; axis < 4U; ++axis) {
                eye[axis] = 0.0f;
                for (row = 0U; row < 4U; ++row)
                    eye[axis] += object[row]
                        * matrices[matrix_index][row][axis];
            }
            for (axis = 0U; axis < 3U; ++axis) {
                world[axis] = input->position[axis];
                for (row = 0U; row < 4U; ++row)
                    world[axis] += eye[row] * input->matrix[row][axis];
            }
            assert(memcmp(eye, published[vertex_index].processed.eye,
                          sizeof(eye)) == 0);
            assert(memcmp(world, published[vertex_index].world,
                          sizeof(world)) == 0);
        }
    }
}
#endif

int main(int argc, char **argv)
{
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeOriginalPropState prop_state;
    GeOriginalDefaultObjectPrepared prepared;
    GeOriginalDefaultObjectProviders object_providers = {0};
    GeOriginalCovertModemFireStats stats;
    GeOriginalCovertModemProjectileStats projectile_stats;
    struct player player;
    struct player_data permissions;
    PropRecord *player_prop;
    Projectile *projectile;
    InvItem *inventory_before;
    bg_portal_data_entry terminator = {0};
    Mtxf view;
    unsigned char *collision_bytes;
    unsigned char *native_bytes;
    size_t collision_size;
    size_t native_size;
    float x = 0.0f;
    float z = 0.0f;
    uint16_t point;
#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
    GeAssetPack exact_pack;
    GeOriginalFirstPersonAssets exact_assets;
    ModelFileHeader *exact_plain_pp7_header;
    ModelFileHeader *exact_pp7_header;
    WeaponStats *exact_pp7_stats;
    GeOriginalBondCameraConfig exact_camera_config;
    GeOriginalBondCameraResult exact_camera_result;
    s32 exact_beam_count_before;
    GeOriginalGunLiveStats exact_live_stats;
    GeOriginalGunLiveHand exact_live_hand;
    GeOriginalDynFrameAudit exact_dyn_frame_audit;
    GeOriginalFirstPersonScene exact_hand_scene;
    GeOriginalFirstPersonSceneCache exact_hand_scene_cache = {0};
    GeDamRoomSceneStorage exact_hand_storage;
    GeDamRoomWorldVertex *exact_hand_vertices;
    GeDamRoomDrawBatch *exact_hand_batches;
    GeDamRoomWorldVertex *exact_hand_vertices_snapshot;
    GeDamRoomDrawBatch *exact_hand_batches_snapshot;
    GeDamCamera exact_hand_camera;
    GeDamCameraSceneResult exact_hand_projected;
    GeDamCameraSceneStorage exact_hand_projected_storage;
    GeDamCameraVertex *exact_hand_projected_vertices;
    GeDamCameraBatch *exact_hand_projected_batches;
    size_t exact_hand_nondegenerate_triangles = 0U;
    size_t exact_hand_projected_vertex;
    unsigned char *exact_hand_matrices_snapshot;
    void *exact_guard_matrices;
    size_t exact_gun_frame_bytes;
    size_t exact_guard_matrix_bytes;
    size_t exact_guard_matrix_allocation_bytes;
    size_t exact_hand_matrix_bytes;
    uint64_t exact_dyn_frame_generation;
    float exact_changed_view_to_world[4][4];
#endif

#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
    assert(argc == 3);
#else
    assert(argc == 2);
#endif
    collision_bytes = read_file(argv[1], &collision_size);
    assert(ge_stan_collision_open(collision_bytes, collision_size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f,
               native_bytes, native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    for (point = 0U; point < ge_stan_native_point_count(native.spawn_tile);
            point++) {
        x += native.spawn_tile->points[point].x;
        z += native.spawn_tile->points[point].z;
    }
    x /= ge_stan_native_point_count(native.spawn_tile) * native.level_scale;
    z /= ge_stan_native_point_count(native.spawn_tile) * native.level_scale;

    memset(&player, 0, sizeof(player));
    memset(&permissions, 0, sizeof(permissions));
    memset(&view, 0, sizeof(view));
    view.m[0][0] = view.m[1][1] = view.m[2][2] = view.m[3][3] = 1.0f;
    assert(ge_original_prop_state_reset(&prop_state, 137U));
    player_prop = ge_original_prop_state_allocate_player(&prop_state);
    assert(player_prop != NULL);
    player_prop->type = PROP_TYPE_PLAYER;
    player_prop->stan = (StandTile *)native.spawn_tile;
    player_prop->pos.x = x;
    player_prop->pos.y = ge_original_stan_get_position_y(
        &native, native.spawn_tile, x, z) + 175.0f;
    player_prop->pos.z = z;
    player.prop = player_prop;
    player.stanHeight = 175.0f;
    player.cameramode = 1;
    player.cameratile = (StandTile *)native.spawn_tile;
    player.pos3 = player_prop->pos;
    player.bondprevpos = player_prop->pos;
    player.field_488.current_tile_ptr_for_portals =
        (StandTile *)native.spawn_tile;
    player.viewtoworldmtxf = &view;
    player.c_screenwidth = 320.0f;
    player.c_screenheight = 240.0f;
    player.c_halfwidth = 160.0f;
    player.c_halfheight = 120.0f;
    player.c_scalex = 1.0f;
    player.c_scaley = 1.0f;
    player.c_perspaspect = 4.0f / 3.0f;
    player.crosshair_angle.x = 160.0f;
    player.crosshair_angle.y = 120.0f;
    player.hands[GUNRIGHT].weaponnum = ITEM_BUG;
    player.hands[GUNRIGHT].weaponnum_watchmenu = -1;
    player.hands[GUNRIGHT].weapon_firing_status = 1;
    player.hands[GUNLEFT].weaponnum = ITEM_UNARMED;
    player.hands[GUNLEFT].weaponnum_watchmenu = -1;
    ge_original_bond_input_bind_player(&player, &permissions);
    ge_original_bond_input_provider_reset_normal_dam();

    object_providers.get_tile_rgb = tile_rgb;
    ge_original_default_object_bind(&object_providers, &prepared);
    ge_original_gameplay_services_reset();
    ge_original_covert_modem_fire_reset();
    room_data_float1 = 0.23363999f;
    g_BgPortals = &terminator;

    assert(ge_original_covert_modem_fire_tick()
           == GE_ORIGINAL_COVERT_MODEM_FIRE_POSE_UNAVAILABLE);
    player.hands[GUNRIGHT].throw_item_pos_related = view;
    player.hands[GUNRIGHT].throw_item_pos_related.m[3][0] = player_prop->pos.x;
    player.hands[GUNRIGHT].throw_item_pos_related.m[3][1] = player_prop->pos.y;
    player.hands[GUNRIGHT].throw_item_pos_related.m[3][2] = player_prop->pos.z;
    assert(ge_original_covert_modem_fire_tick()
           == GE_ORIGINAL_COVERT_MODEM_FIRE_THROWN);

    projectile = &g_Projectiles[0];
    assert(projectile->obj != NULL);
    assert(projectile->ownerprop == player_prop);
    assert(projectile->obj->prop->stan == (StandTile *)native.spawn_tile);
    assert(ge_original_prop_state_room_contains(
        (int16_t)native.spawn_tile->room, projectile->obj->prop));
    assert(projectile->flags & PROJECTILEFLAG_STICKY);
    assert(projectile->flags & 2U);
    assert(projectile->refreshrate == 60);
    ge_original_covert_modem_fire_snapshot(&stats);
    assert(stats.both_hands_ticks == 2U);
    assert(stats.hand_dispatches == 4U);
    assert(stats.throw_attempts == 2U);
    assert(stats.successful_throws == 1U);
    assert(stats.pose_rejections == 1U);
    inventory_before = player.ptr_inventory_first_in_cycle;
    ge_original_generate_player_thrown_object_exact(GUNRIGHT);
    projectile = &g_Projectiles[1];
    assert(projectile->obj != NULL);
    assert(projectile->ownerprop == player_prop);
    assert(projectile->obj->prop->stan == (StandTile *)native.spawn_tile);
    assert(projectile->flags & PROJECTILEFLAG_STICKY);
    assert(projectile->flags & 2U);
    assert(projectile->refreshrate == 60);
    assert(player.ptr_inventory_first_in_cycle == inventory_before);
    ge_original_covert_modem_projectile_snapshot(&projectile_stats);
    assert(projectile_stats.launch_calls == 2U);
    assert(projectile_stats.pool_allocations == 2U);
    assert(projectile_stats.pool_exhaustions == 0U);
    /* Both launches used the unchanged generated caller, so the compatibility
     * launch wrapper's success counter is intentionally untouched. */
    assert(projectile_stats.successful_launches == 0U);
#ifdef GE_TEST_EXACT_GUN_BOTH_HANDS
    /* Exercise the unchanged both-hands dispatcher and its ITEM_BUG branch.
     * The model is deliberately hidden; the canonical body must still publish
     * the throw pose and invoke the exact generated projectile constructor. */
    ge_original_bond_input_initialize_player_hands(
        exact_right_hand_buffer, exact_left_hand_buffer);
    player.hands[GUNRIGHT].weapon_firing_status = 1;
    player.hands[GUNRIGHT].weaponnum = ITEM_BUG;
    player.hands[GUNRIGHT].weaponnum_watchmenu = -1;
    player.hands[GUNRIGHT].weapon_ammo_in_magazine = 1;
    player.hands[GUNLEFT].weapon_firing_status = 0;
    player.hands[GUNLEFT].weaponnum = ITEM_UNARMED;
    player.hands[GUNLEFT].weaponnum_watchmenu = -1;
    player.field_2A44[GUNRIGHT] = -1;
    player.field_2A44[GUNLEFT] = -1;
    player.field_FFC.x = 160.0f;
    player.field_FFC.y = 120.0f;
    player.c_screenleft = 0.0f;
    player.c_screentop = 0.0f;
    for (point = 0U; point < 4U; point++) {
        player.hands[GUNRIGHT].blendlook[point].z = -1.0f;
        player.hands[GUNRIGHT].blendup[point].y = 1.0f;
        player.hands[GUNLEFT].blendlook[point].z = -1.0f;
        player.hands[GUNLEFT].blendup[point].y = 1.0f;
    }
    assert(ge_original_gun_frame_arena_begin(
        exact_gun_frame_arena, sizeof(exact_gun_frame_arena)));
    ge_original_gun_update_and_fire_both_hands_exact();
    projectile = &g_Projectiles[2];
    assert(projectile->obj != NULL);
    assert(projectile->ownerprop == player_prop);
    assert(projectile->obj->prop->stan == (StandTile *)native.spawn_tile);
    assert(projectile->flags & PROJECTILEFLAG_STICKY);
    assert(projectile->refreshrate == 60);
    assert(isfinite(player.hands[GUNRIGHT].throw_item_pos_related.m[3][0]));
    assert(isfinite(player.hands[GUNRIGHT].throw_item_pos_related.m[3][1]));
    assert(isfinite(player.hands[GUNRIGHT].throw_item_pos_related.m[3][2]));
    assert(ge_original_gun_frame_arena_finalize(NULL));
    puts("exact unchanged gunUpdateAndFireBothHands ITEM_BUG path: ok");

    /* Relocate the authored silenced PP7 hand model, then run the same exact
     * dispatcher through its cartridge-ejection sink.  Keeping hand_item at
     * ITEM_UNARMED intentionally hides only the render/model-relations branch;
     * casing selection and creation remain the unchanged WPPKSIL behavior. */
    assert(ge_asset_pack_open(&exact_pack, argv[2]) == GE_ASSET_PACK_OK);
    assert(ge_original_first_person_assets_init(
        &exact_assets, &exact_pack,
        exact_right_hand_buffer, sizeof(exact_right_hand_buffer),
        exact_left_hand_buffer, sizeof(exact_left_hand_buffer))
        == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    exact_plain_pp7_header = NULL;
    exact_pp7_header = NULL;
    assert(ge_original_first_person_assets_load_item_native(
        &exact_assets, GUNRIGHT, ITEM_WPPK,
        (void **)&exact_plain_pp7_header) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(ge_original_first_person_assets_load_item_native(
        &exact_assets, GUNLEFT, ITEM_WPPKSIL,
        (void **)&exact_pp7_header) == GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
    assert(exact_plain_pp7_header != NULL);
    assert(exact_pp7_header != NULL && exact_pp7_header->RootNode != NULL);
    player.copy_of_body_obj_header[GUNRIGHT] = *exact_pp7_header;
#if UINTPTR_MAX > UINT32_MAX
    assert(exact_pp7_header->numSwitches
        <= sizeof(exact_host_switches) / sizeof(exact_host_switches[0]));
    memcpy(exact_host_switches, exact_pp7_header->Switches,
        exact_pp7_header->numSwitches * sizeof(exact_host_switches[0]));
    player.copy_of_body_obj_header[GUNRIGHT].Switches = exact_host_switches;
#endif
    player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].weapon_firing_status = 1;
    player.hands[GUNRIGHT].weapon_ammo_in_magazine = 5;
    player.hand_item[GUNRIGHT] = ITEM_UNARMED;
    memset(g_Casings, 0, sizeof(g_Casings));
    assert(ge_original_gun_frame_arena_begin(
        exact_gun_frame_arena, sizeof(exact_gun_frame_arena)));
    ge_original_gun_update_and_fire_both_hands_exact();
    exact_pp7_stats = gitem_structs[ITEM_WPPKSIL].item_weapon_stats;
    assert(exact_pp7_stats != NULL);
    assert(g_Casings[0].header == exact_pp7_stats->ptr_cartridge_struct);
    assert(g_Casings[0].header != NULL);
    assert(g_Casings[0].header->RootNode != NULL);
    assert(isfinite(g_Casings[0].pos.x));
    assert(isfinite(g_Casings[0].pos.y));
    assert(isfinite(g_Casings[0].pos.z));
    assert(isfinite(g_Casings[0].vel.x));
    assert(isfinite(g_Casings[0].vel.y));
    assert(isfinite(g_Casings[0].vel.z));
    assert(ge_original_gun_frame_arena_finalize(NULL));
    memset(&exact_camera_config, 0, sizeof(exact_camera_config));
    exact_camera_config.camera_position[0] = player_prop->pos.x;
    exact_camera_config.camera_position[1] = player_prop->pos.y;
    exact_camera_config.camera_position[2] = player_prop->pos.z;
    exact_camera_config.camera_look_direction[2] = 1.0f;
    exact_camera_config.camera_up[1] = 1.0f;
    exact_camera_config.room_position_scale = 0.23363999f;
    exact_camera_config.camera_local_scale = 1.0f;
    exact_camera_config.visibility_scale = 1.0f;
    exact_camera_config.viewport_scale[0] = 640;
    exact_camera_config.viewport_scale[1] = 480;
    exact_camera_config.viewport_scale[2] = 511;
    exact_camera_config.viewport_translation[0] = 640;
    exact_camera_config.viewport_translation[1] = 480;
    exact_camera_config.viewport_translation[2] = 511;
    exact_camera_config.room = (uint8_t)native.spawn_tile->room;
    assert(ge_original_bond_camera_set_perspective(
        &exact_camera_config, 60.0f, 4.0f / 3.0f, 5.0f, 15000.0f)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(ge_original_bond_camera_run(
        &exact_camera_config, &exact_camera_result)
        == GE_ORIGINAL_BOND_CAMERA_OK);

    /* Enable the relocated visible hand and drive unchanged model relations
     * and gunCreateBeamForHand against the exact camera output. */
    player.hand_invisible[GUNRIGHT] = 1;
    player.hand_item[GUNRIGHT] = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].item_related.x = player_prop->pos.x;
    player.hands[GUNRIGHT].item_related.y = player_prop->pos.y;
    player.hands[GUNRIGHT].item_related.z = player_prop->pos.z + 1000.0f;
    exact_beam_count_before = player.hands[GUNRIGHT].field_8A0;
    ge_original_gun_live_reset();
    assert(ge_original_gun_live_frame_begin());
    assert(ge_original_gun_frame_arena_audit(&exact_dyn_frame_audit));
    assert(exact_dyn_frame_audit.active && exact_dyn_frame_audit.used == 0U);
    exact_dyn_frame_generation = exact_dyn_frame_audit.generation;
    assert(ge_original_gun_live_tick());
    assert(ge_original_gun_frame_arena_audit(&exact_dyn_frame_audit));
    assert(exact_dyn_frame_audit.active);
    assert(exact_dyn_frame_audit.generation == exact_dyn_frame_generation);
    ge_original_gun_live_snapshot(&exact_live_stats);
    assert(exact_live_stats.ticks == 1U);
    assert(exact_live_stats.last_frame_generation
        == exact_dyn_frame_generation);
    assert(exact_live_stats.last_frame_bytes
        >= exact_pp7_header->numMatrices * sizeof(Mtxf));
    assert(exact_live_stats.peak_frame_bytes
        == exact_live_stats.last_frame_bytes);
    assert(player.hands[GUNRIGHT].field_87F != 0);
    assert(player.hands[GUNRIGHT].mtxlist != NULL);
    assert(ge_original_gun_frame_arena_used()
        >= exact_pp7_header->numMatrices * sizeof(Mtxf));
    assert(player.hands[GUNRIGHT].field_8A0 == exact_beam_count_before + 1);
    assert(isfinite(player.hands[GUNRIGHT].field_B58.x));
    assert(isfinite(player.hands[GUNRIGHT].field_B58.y));
    assert(isfinite(player.hands[GUNRIGHT].field_B58.z));
    assert(isfinite(player.hands[GUNRIGHT].weapon_beam.delta.x));
    assert(isfinite(player.hands[GUNRIGHT].weapon_beam.delta.y));
    assert(isfinite(player.hands[GUNRIGHT].weapon_beam.delta.z));
    assert(ge_original_gun_live_hand_snapshot(
        GUNRIGHT, &exact_live_hand));
    assert(exact_live_hand.visible != 0);
    assert(exact_live_hand.model != NULL);
    assert(exact_live_hand.matrices
        == (const float (*)[4][4])(const void *)player.hands[GUNRIGHT].mtxlist);
    assert(exact_live_hand.matrix_count
        == (size_t)exact_pp7_header->numMatrices);

    /* The exact gun body allocates its hand matrices first.  Model the later
     * exact chrTick matrix demand from all authored initial Dam guards through
     * the same dynAllocate cursor and prove that it appends without modifying
     * the live hand matrices that rendering still consumes this frame. */
    exact_gun_frame_bytes = ge_original_gun_frame_arena_used();
    exact_hand_matrix_bytes = exact_live_hand.matrix_count * sizeof(Mtxf);
    exact_hand_matrices_snapshot = malloc(exact_hand_matrix_bytes);
    assert(exact_hand_matrices_snapshot != NULL);
    memcpy(exact_hand_matrices_snapshot, exact_live_hand.matrices,
           exact_hand_matrix_bytes);
    assert(ge_original_dam_guard_model_prepare());
    exact_guard_matrix_bytes = GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY
        * ge_original_dam_guard_model_matrix_count() * sizeof(Mtxf);
    assert(exact_guard_matrix_bytes > 0U
        && exact_guard_matrix_bytes <= (size_t)INT32_MAX);
    exact_guard_matrices = dynAllocate((s32)exact_guard_matrix_bytes);
    assert(exact_guard_matrices != NULL);
    assert((unsigned char *)exact_guard_matrices
        >= (unsigned char *)exact_live_hand.matrices
            + exact_hand_matrix_bytes);
    memset(exact_guard_matrices, 0xa5, exact_guard_matrix_bytes);
    assert(memcmp(exact_live_hand.matrices, exact_hand_matrices_snapshot,
                  exact_hand_matrix_bytes) == 0);
    assert(ge_original_gun_live_frame_finalize(&exact_dyn_frame_audit));
    exact_guard_matrix_allocation_bytes =
        (exact_guard_matrix_bytes + 15U) & ~(size_t)15U;
    assert(exact_dyn_frame_audit.within_bounds);
    assert(!exact_dyn_frame_audit.active);
    assert(exact_dyn_frame_audit.used
        == exact_gun_frame_bytes + exact_guard_matrix_allocation_bytes);
    assert(exact_dyn_frame_audit.used <= exact_dyn_frame_audit.capacity);
    assert(!ge_original_gun_frame_arena_active());
    assert(ge_original_first_person_scene_build(
        &exact_assets, GUNRIGHT, exact_camera_result.view_to_world,
        NULL, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_CAPACITY_EXCEEDED);
    assert(exact_hand_scene.display_list_count > 0U);
    assert(exact_hand_scene.required_vertex_count > 0U);
    assert(exact_hand_scene.required_batch_count > 0U);
    exact_hand_vertices = calloc(exact_hand_scene.required_vertex_count,
                                 sizeof(*exact_hand_vertices));
    exact_hand_batches = calloc(exact_hand_scene.required_batch_count,
                                sizeof(*exact_hand_batches));
    assert(exact_hand_vertices != NULL && exact_hand_batches != NULL);
    exact_hand_storage = (GeDamRoomSceneStorage){
        exact_hand_vertices, exact_hand_scene.required_vertex_count,
        exact_hand_batches, exact_hand_scene.required_batch_count,
    };
    assert(ge_original_first_person_scene_build(
        &exact_assets, GUNRIGHT, exact_camera_result.view_to_world,
        &exact_hand_storage, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    assert(exact_hand_scene.vertex_count
        == exact_hand_scene.required_vertex_count);
    assert(exact_hand_scene.batch_count
        == exact_hand_scene.required_batch_count);
    assert(exact_hand_scene.triangle_count * 3U
        == exact_hand_scene.vertex_count);
    assert(ge_original_first_person_scene_cache_init(
        &exact_hand_scene_cache));
    assert(ge_original_first_person_scene_build_cached(
        &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
        exact_camera_result.view_to_world,
        &exact_hand_storage, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    assert(exact_hand_scene_cache.topology_rebuilds == 1U);
    assert(exact_hand_scene_cache.single_pass_builds == 1U);
    assert(exact_hand_scene_cache.matrix_bank_quantizations == 1U);
    assert(exact_hand_scene_cache.duplicate_vertex_transforms_avoided > 0U);
    assert(exact_hand_scene_cache.cross_input_duplicate_transforms_avoided
        > 0U);
    assert(exact_hand_scene.display_list_count > 1U
        && exact_hand_scene_cache.shared_matrix_banks_reused
            == exact_hand_scene.display_list_count - 1U);
    assert_first_person_cached_transform_byte_exact(
        &exact_hand_scene_cache, exact_hand_vertices);
    exact_hand_vertices_snapshot = malloc(
        exact_hand_scene.vertex_count * sizeof(*exact_hand_vertices_snapshot));
    exact_hand_batches_snapshot = malloc(
        exact_hand_scene.batch_count * sizeof(*exact_hand_batches_snapshot));
    assert(exact_hand_vertices_snapshot != NULL
        && exact_hand_batches_snapshot != NULL);
    memcpy(exact_hand_vertices_snapshot, exact_hand_vertices,
           exact_hand_scene.vertex_count * sizeof(*exact_hand_vertices));
    memcpy(exact_hand_batches_snapshot, exact_hand_batches,
           exact_hand_scene.batch_count * sizeof(*exact_hand_batches));
    assert(ge_original_first_person_scene_build_cached(
        &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
        exact_camera_result.view_to_world,
        &exact_hand_storage, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    assert(exact_hand_scene_cache.topology_rebuilds == 1U);
    assert(exact_hand_scene_cache.single_pass_builds == 1U);
    assert(exact_hand_scene_cache.unchanged_builds == 1U);
    assert(memcmp(exact_hand_vertices_snapshot, exact_hand_vertices,
                  exact_hand_scene.vertex_count
                      * sizeof(*exact_hand_vertices)) == 0);
    assert(memcmp(exact_hand_batches_snapshot, exact_hand_batches,
                  exact_hand_scene.batch_count
                      * sizeof(*exact_hand_batches)) == 0);
    free(exact_hand_batches_snapshot);
    free(exact_hand_vertices_snapshot);
    memcpy(exact_changed_view_to_world, exact_camera_result.view_to_world,
           sizeof(exact_changed_view_to_world));
    exact_changed_view_to_world[3][0] += 1.0f;
    assert(ge_original_first_person_scene_build_cached(
        &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
        exact_changed_view_to_world,
        &exact_hand_storage, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    assert(exact_hand_scene_cache.single_pass_builds == 2U);
    assert(exact_hand_scene_cache.unchanged_builds == 1U);
    assert(exact_hand_scene_cache.matrix_bank_quantizations == 2U);
    assert(exact_hand_scene_cache.static_vertex_copies_avoided
        == exact_hand_scene.vertex_count);
    assert(exact_hand_scene_cache.static_batch_copies_avoided
        == exact_hand_scene.batch_count);
    assert(exact_hand_scene_cache.matrix_elements_requantized_avoided
        > 0U);
    assert_first_person_cached_transform_byte_exact(
        &exact_hand_scene_cache, exact_hand_vertices);
    assert(ge_original_first_person_scene_build_cached(
        &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
        exact_camera_result.view_to_world,
        &exact_hand_storage, &exact_hand_scene)
        == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    assert(exact_hand_scene_cache.single_pass_builds == 3U);
    assert(exact_hand_scene_cache.unchanged_builds == 1U);
    assert(exact_hand_scene_cache.matrix_bank_quantizations == 3U);
    {
        static const float eye_space_identity[4][4] = {
            {1.0f, 0.0f, 0.0f, 0.0f},
            {0.0f, 1.0f, 0.0f, 0.0f},
            {0.0f, 0.0f, 1.0f, 0.0f},
            {0.0f, 0.0f, 0.0f, 1.0f},
        };
        float (*live_matrices)[4][4] =
            (float (*)[4][4])(uintptr_t)exact_live_hand.matrices;
        size_t changed_matrix = SIZE_MAX;
        size_t changed_matrix_vertices = SIZE_MAX;
        float changed_matrix_original = 0.0f;
        uint64_t unchanged_matrix_vertices_before;
        size_t eye_vertex;
        size_t matrix_index;
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            eye_space_identity, &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
        assert(exact_hand_scene_cache.single_pass_builds == 4U);
        assert(exact_hand_scene_cache.eye_space_vertices_published
            == exact_hand_scene.vertex_count);
        for (eye_vertex = 0U;
                eye_vertex < exact_hand_scene.vertex_count; ++eye_vertex) {
            size_t axis;
            for (axis = 0U; axis < 3U; ++axis)
                assert(exact_hand_vertices[eye_vertex].world[axis]
                    == exact_hand_vertices[eye_vertex].processed.eye[axis]);
        }
        /* A canonical gun tick generally changes only a small subtree of the
         * shared hand matrix bank.  Select an actually referenced, non-global
         * matrix and prove that a subsequent eye-space publication retains
         * every vertex driven by byte-identical quantized matrices while the
         * changed subtree remains byte-exact against a fresh transform. */
        for (matrix_index = 0U;
                matrix_index < exact_live_hand.matrix_count;
                ++matrix_index) {
            size_t referenced_vertices = 0U;
            for (eye_vertex = 0U;
                    eye_vertex < exact_hand_scene.vertex_count; ++eye_vertex)
                if ((size_t)exact_hand_scene_cache
                        .template_matrix_indices[eye_vertex] == matrix_index)
                    referenced_vertices++;
            if (referenced_vertices != 0U
                    && referenced_vertices < changed_matrix_vertices) {
                changed_matrix = matrix_index;
                changed_matrix_vertices = referenced_vertices;
            }
        }
        assert(changed_matrix != SIZE_MAX
            && changed_matrix_vertices > 0U
            && changed_matrix_vertices < exact_hand_scene.vertex_count);
        changed_matrix_original = live_matrices[changed_matrix][3][0];
        live_matrices[changed_matrix][3][0] =
            changed_matrix_original + 0.25f;
        unchanged_matrix_vertices_before = exact_hand_scene_cache
            .unchanged_matrix_vertices_reused;
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            eye_space_identity, &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
        assert(exact_hand_scene_cache.unchanged_matrix_vertices_reused
                - unchanged_matrix_vertices_before
            == exact_hand_scene.vertex_count - changed_matrix_vertices);
        assert_first_person_cached_transform_byte_exact(
            &exact_hand_scene_cache, exact_hand_vertices);
        live_matrices[changed_matrix][3][0] = changed_matrix_original;
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            eye_space_identity, &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
        assert_first_person_cached_transform_byte_exact(
            &exact_hand_scene_cache, exact_hand_vertices);
        /* Restore the world publication consumed by the legacy CPU camera
         * projection assertions below. The 3DS renderer instead keeps the
         * eye-space publication and binds its projection-only matrix. */
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            exact_camera_result.view_to_world,
            &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    }
    if (getenv("GE_FIRST_PERSON_SCENE_BENCH") != NULL) {
        const char *iterations_text =
            getenv("GE_FIRST_PERSON_SCENE_BENCH_ITERS");
        const size_t iterations = iterations_text != NULL
            ? (size_t)strtoull(iterations_text, NULL, 10) : 2000U;
        GeDamRoomWorldVertex *legacy_vertices = calloc(
            exact_hand_scene.vertex_count, sizeof(*legacy_vertices));
        GeDamRoomDrawBatch *legacy_batches = calloc(
            exact_hand_scene.batch_count, sizeof(*legacy_batches));
        GeDamRoomSceneStorage legacy_storage = {
            legacy_vertices, exact_hand_scene.vertex_count,
            legacy_batches, exact_hand_scene.batch_count,
        };
        GeOriginalFirstPersonScene legacy_scene;
        clock_t legacy_ticks = 0;
        clock_t cached_ticks = 0;
        const uint64_t duplicate_transforms_before =
            exact_hand_scene_cache.duplicate_vertex_transforms_avoided;
        uint64_t profile_build_before;
        uint64_t profile_input_before;
        uint64_t profile_matrix_before;
        uint64_t profile_vertex_before;
        uint64_t profile_batch_before;
        size_t iteration;
        assert(iterations > 0U && iterations <= 100000U
            && legacy_vertices != NULL && legacy_batches != NULL);
        ge_original_first_person_scene_cache_bind_profile_clock(
            &exact_hand_scene_cache,
            exact_first_person_profile_clock, NULL);
        profile_build_before = exact_hand_scene_cache.profile_build_ticks;
        profile_input_before =
            exact_hand_scene_cache.profile_input_topology_ticks;
        profile_matrix_before =
            exact_hand_scene_cache.profile_matrix_signature_ticks;
        profile_vertex_before =
            exact_hand_scene_cache.profile_vertex_transform_ticks;
        profile_batch_before =
            exact_hand_scene_cache.profile_batch_publication_ticks;
        for (iteration = 0U; iteration < iterations; ++iteration) {
            clock_t started;
            memcpy(exact_changed_view_to_world,
                   exact_camera_result.view_to_world,
                   sizeof(exact_changed_view_to_world));
            exact_changed_view_to_world[3][0] +=
                (float)(1U + (iteration & 1U));
            if ((iteration & 1U) == 0U) {
                started = clock();
                assert(ge_original_first_person_scene_build(
                    &exact_assets, GUNRIGHT, exact_changed_view_to_world,
                    &legacy_storage, &legacy_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                legacy_ticks += clock() - started;
                started = clock();
                assert(ge_original_first_person_scene_build_cached(
                    &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
                    exact_changed_view_to_world,
                    &exact_hand_storage, &exact_hand_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                cached_ticks += clock() - started;
            } else {
                started = clock();
                assert(ge_original_first_person_scene_build_cached(
                    &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
                    exact_changed_view_to_world,
                    &exact_hand_storage, &exact_hand_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                cached_ticks += clock() - started;
                started = clock();
                assert(ge_original_first_person_scene_build(
                    &exact_assets, GUNRIGHT, exact_changed_view_to_world,
                    &legacy_storage, &legacy_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                legacy_ticks += clock() - started;
            }
        }
        assert(legacy_scene.vertex_count == exact_hand_scene.vertex_count
            && legacy_scene.batch_count == exact_hand_scene.batch_count);
        for (iteration = 0U;
                iteration < exact_hand_scene.vertex_count; ++iteration) {
            size_t axis;
            assert(memcmp(&legacy_vertices[iteration].source,
                          &exact_hand_vertices[iteration].source,
                          sizeof(legacy_vertices[iteration].source)) == 0);
            for (axis = 0U; axis < 3U; ++axis)
                assert(fabsf(legacy_vertices[iteration].world[axis]
                    - exact_hand_vertices[iteration].world[axis]) < 0.001f);
        }
        assert(memcmp(legacy_batches, exact_hand_batches,
                      exact_hand_scene.batch_count
                          * sizeof(*legacy_batches)) == 0);
        printf("first-person scene benchmark: %zu iterations, %zu parts, "
               "%zu vertices, legacy %.3f us, cached %.3f us, %.2fx; "
               "%llu requantized elements, %llu static vertex copies, and "
               "%llu duplicate transforms avoided (%llu cross-input); "
               "profile %.3f us = input/topology %.3f + matrix/signature "
               "%.3f + vertex %.3f + batch %.3f\n",
               iterations, exact_hand_scene.display_list_count,
               exact_hand_scene.vertex_count,
               (double)legacy_ticks * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               (double)cached_ticks * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               cached_ticks != 0
                    ? (double)legacy_ticks / (double)cached_ticks : 0.0,
               (unsigned long long)exact_hand_scene_cache
                    .matrix_elements_requantized_avoided,
               (unsigned long long)exact_hand_scene_cache
                    .static_vertex_copies_avoided,
               (unsigned long long)(exact_hand_scene_cache
                    .duplicate_vertex_transforms_avoided
                    - duplicate_transforms_before),
               (unsigned long long)exact_hand_scene_cache
                    .cross_input_duplicate_transforms_avoided,
               (double)(exact_hand_scene_cache.profile_build_ticks
                    - profile_build_before) * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               (double)(exact_hand_scene_cache.profile_input_topology_ticks
                    - profile_input_before) * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               (double)(exact_hand_scene_cache.profile_matrix_signature_ticks
                    - profile_matrix_before) * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               (double)(exact_hand_scene_cache.profile_vertex_transform_ticks
                    - profile_vertex_before) * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations,
               (double)(exact_hand_scene_cache.profile_batch_publication_ticks
                    - profile_batch_before) * 1000000.0
                    / (double)CLOCKS_PER_SEC / (double)iterations);
        {
            static const float eye_space_identity[4][4] = {
                {1.0f, 0.0f, 0.0f, 0.0f},
                {0.0f, 1.0f, 0.0f, 0.0f},
                {0.0f, 0.0f, 1.0f, 0.0f},
                {0.0f, 0.0f, 0.0f, 1.0f},
            };
            float (*live_matrices)[4][4] =
                (float (*)[4][4])(uintptr_t)exact_live_hand.matrices;
            size_t changed_matrix = SIZE_MAX;
            size_t changed_matrix_vertices = SIZE_MAX;
            float changed_matrix_original = 0.0f;
            clock_t alternating_storage_ticks = 0;
            clock_t retained_storage_ticks = 0;
            uint64_t unchanged_matrix_vertices_before;
            size_t matrix_index;
            for (matrix_index = 0U;
                    matrix_index < exact_live_hand.matrix_count;
                    ++matrix_index) {
                size_t referenced_vertices = 0U;
                size_t vertex_index;
                for (vertex_index = 0U;
                        vertex_index < exact_hand_scene.vertex_count;
                        ++vertex_index)
                    if ((size_t)exact_hand_scene_cache
                            .template_matrix_indices[vertex_index]
                            == matrix_index)
                        referenced_vertices++;
                if (referenced_vertices != 0U
                        && referenced_vertices < changed_matrix_vertices) {
                    changed_matrix = matrix_index;
                    changed_matrix_vertices = referenced_vertices;
                }
            }
            assert(changed_matrix != SIZE_MAX
                && changed_matrix_vertices > 0U
                && changed_matrix_vertices < exact_hand_scene.vertex_count);
            changed_matrix_original = live_matrices[changed_matrix][3][0];
            /* Alternate output storage to measure the same retained topology
             * and quantized-matrix path with incremental output reuse
             * deliberately disabled. */
            for (iteration = 0U; iteration < iterations; ++iteration) {
                GeDamRoomSceneStorage *target = (iteration & 1U) != 0U
                    ? &exact_hand_storage : &legacy_storage;
                clock_t started;
                live_matrices[changed_matrix][3][0] =
                    changed_matrix_original
                        + ((iteration & 1U) != 0U ? 0.0f : 0.25f);
                started = clock();
                assert(ge_original_first_person_scene_build_cached(
                    &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
                    eye_space_identity, target, &exact_hand_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                alternating_storage_ticks += clock() - started;
            }
            live_matrices[changed_matrix][3][0] = changed_matrix_original;
            assert(ge_original_first_person_scene_build_cached(
                &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
                eye_space_identity, &exact_hand_storage, &exact_hand_scene)
                == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
            unchanged_matrix_vertices_before = exact_hand_scene_cache
                .unchanged_matrix_vertices_reused;
            for (iteration = 0U; iteration < iterations; ++iteration) {
                clock_t started;
                live_matrices[changed_matrix][3][0] =
                    changed_matrix_original
                        + ((iteration & 1U) != 0U ? 0.0f : 0.25f);
                started = clock();
                assert(ge_original_first_person_scene_build_cached(
                    &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
                    eye_space_identity, &exact_hand_storage,
                    &exact_hand_scene)
                    == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
                retained_storage_ticks += clock() - started;
            }
            assert(exact_hand_scene_cache.unchanged_matrix_vertices_reused
                    - unchanged_matrix_vertices_before
                == (exact_hand_scene.vertex_count - changed_matrix_vertices)
                    * iterations);
            assert_first_person_cached_transform_byte_exact(
                &exact_hand_scene_cache, exact_hand_vertices);
            printf("first-person incremental matrix benchmark: matrix %zu "
                   "drives %zu/%zu vertices, alternating storage %.3f us, "
                   "retained storage %.3f us, %.2fx; %.1f unchanged-matrix "
                   "vertices reused/frame\n",
                   changed_matrix, changed_matrix_vertices,
                   exact_hand_scene.vertex_count,
                   (double)alternating_storage_ticks * 1000000.0
                        / (double)CLOCKS_PER_SEC / (double)iterations,
                   (double)retained_storage_ticks * 1000000.0
                        / (double)CLOCKS_PER_SEC / (double)iterations,
                   retained_storage_ticks != 0
                        ? (double)alternating_storage_ticks
                            / (double)retained_storage_ticks : 0.0,
                   (double)(exact_hand_scene_cache
                        .unchanged_matrix_vertices_reused
                        - unchanged_matrix_vertices_before)
                        / (double)iterations);
            live_matrices[changed_matrix][3][0] = changed_matrix_original;
        }
        ge_original_first_person_scene_cache_bind_profile_clock(
            &exact_hand_scene_cache, NULL, NULL);
        free(legacy_batches);
        free(legacy_vertices);
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            exact_camera_result.view_to_world,
            &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
    }
    assert(ge_dam_camera_prepare_rsp_viewport(
        exact_camera_result.view, exact_camera_result.projection,
        exact_camera_result.viewport_scale,
        exact_camera_result.viewport_translation, &exact_hand_camera)
        == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project_batches(
        &exact_hand_camera, exact_hand_vertices, exact_hand_scene.vertex_count,
        exact_hand_batches, exact_hand_scene.batch_count,
        NULL, &exact_hand_projected) == GE_DAM_CAMERA_CAPACITY_EXCEEDED);
    assert(exact_hand_projected.required_vertex_count > 0U);
    assert(exact_hand_projected.required_batch_count > 0U);
    exact_hand_projected_vertices = calloc(
        exact_hand_projected.required_vertex_count,
        sizeof(*exact_hand_projected_vertices));
    exact_hand_projected_batches = calloc(
        exact_hand_projected.required_batch_count,
        sizeof(*exact_hand_projected_batches));
    assert(exact_hand_projected_vertices != NULL
        && exact_hand_projected_batches != NULL);
    exact_hand_projected_storage = (GeDamCameraSceneStorage){
        exact_hand_projected_vertices,
        exact_hand_projected.required_vertex_count,
        exact_hand_projected_batches,
        exact_hand_projected.required_batch_count,
    };
    assert(ge_dam_camera_project_batches(
        &exact_hand_camera, exact_hand_vertices, exact_hand_scene.vertex_count,
        exact_hand_batches, exact_hand_scene.batch_count,
        &exact_hand_projected_storage, &exact_hand_projected)
        == GE_DAM_CAMERA_OK);
    assert(exact_hand_projected.vertex_count > 0U);
    assert(exact_hand_projected.batch_count > 0U);
    for (exact_hand_projected_vertex = 0U;
            exact_hand_projected_vertex + 2U
                < exact_hand_projected.vertex_count;
            exact_hand_projected_vertex += 3U) {
        const GeDamCameraVertex *a = &exact_hand_projected_vertices[
            exact_hand_projected_vertex];
        const GeDamCameraVertex *b = &exact_hand_projected_vertices[
            exact_hand_projected_vertex + 1U];
        const GeDamCameraVertex *c = &exact_hand_projected_vertices[
            exact_hand_projected_vertex + 2U];
        const float signed_area =
            (b->screen[0] - a->screen[0]) * (c->screen[1] - a->screen[1])
            - (b->screen[1] - a->screen[1]) * (c->screen[0] - a->screen[0]);
        assert(isfinite(signed_area));
        if (fabsf(signed_area) > 1.0e-5f)
            exact_hand_nondegenerate_triangles++;
    }
    assert(exact_hand_nondegenerate_triangles > 0U);
    fprintf(stderr, "first-person projection at authored Dam near 5: %zu/%zu triangles, %zu vertices\n",
            exact_hand_projected.visible_input_triangle_count,
            exact_hand_projected.input_triangle_count,
            exact_hand_projected.required_vertex_count);
    assert(ge_original_bond_camera_set_perspective(
        &exact_camera_config, 60.0f, 4.0f / 3.0f, 100.0f, 10000.0f)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(ge_original_bond_camera_run(
        &exact_camera_config, &exact_camera_result)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(ge_dam_camera_prepare_rsp_viewport(
        exact_camera_result.view, exact_camera_result.projection,
        exact_camera_result.viewport_scale,
        exact_camera_result.viewport_translation, &exact_hand_camera)
        == GE_DAM_CAMERA_OK);
    assert(ge_dam_camera_project_batches(
        &exact_hand_camera, exact_hand_vertices, exact_hand_scene.vertex_count,
        exact_hand_batches, exact_hand_scene.batch_count,
        NULL, &exact_hand_projected) == GE_DAM_CAMERA_OK);
    fprintf(stderr, "first-person projection at rejected near 100: %zu/%zu triangles, %zu vertices\n",
            exact_hand_projected.visible_input_triangle_count,
            exact_hand_projected.input_triangle_count,
            exact_hand_projected.required_vertex_count);
    assert(exact_hand_projected.required_vertex_count == 0U);
    free(exact_hand_projected_batches);
    free(exact_hand_projected_vertices);
    assert(memcmp(exact_live_hand.matrices, exact_hand_matrices_snapshot,
                  exact_hand_matrix_bytes) == 0);
    free(exact_hand_matrices_snapshot);

    /* Keep the exact gun model publication live across a sustained sequence
     * of input-derived lateral pose changes.  This is the renderer boundary
     * exercised while Bond traverses Dam: every canonical hand tick must
     * publish a fresh mtxlist and the cached ROM GBI scene must consume it
     * before the shared frame arena is finalized. */
    player.hands[GUNRIGHT].weapon_firing_status = 0;
    for (point = 0U; point < 24U; ++point) {
        player.hands[GUNRIGHT].weapon_theta_displacement =
            (float)point * 0.125f;
        assert(ge_original_gun_live_frame_begin());
        assert(ge_original_gun_live_tick());
        assert(ge_original_gun_live_hand_snapshot(
            GUNRIGHT, &exact_live_hand));
        assert(exact_live_hand.generation == (uint64_t)point + 2U);
        assert(exact_live_hand.visible);
        assert(exact_live_hand.model != NULL);
        assert(exact_live_hand.matrices != NULL);
        assert(ge_original_first_person_scene_build_cached(
            &exact_hand_scene_cache, &exact_assets, GUNRIGHT,
            exact_camera_result.view_to_world,
            &exact_hand_storage, &exact_hand_scene)
            == GE_ORIGINAL_FIRST_PERSON_SCENE_OK);
        assert(exact_hand_scene.generation == exact_live_hand.generation);
        assert(exact_hand_scene.vertex_count > 0U);
        assert(exact_hand_scene.batch_count > 0U);
        assert(ge_original_gun_live_frame_finalize(
            &exact_dyn_frame_audit));
        assert(exact_dyn_frame_audit.within_bounds);
    }
    free(exact_hand_batches);
    free(exact_hand_vertices);
    ge_original_first_person_scene_cache_close(&exact_hand_scene_cache);

    /* Legacy/test callers that do not own an outer frame still receive one
     * private begin/finalize around the unchanged gun tick. */
    assert(ge_original_gun_live_tick());
    assert(!ge_original_gun_frame_arena_active());
    assert(ge_original_gun_frame_arena_audit(&exact_dyn_frame_audit));
    assert(exact_dyn_frame_audit.used <= exact_dyn_frame_audit.capacity);
    ge_original_gun_live_snapshot(&exact_live_stats);
    assert(exact_live_stats.ticks == 26U);
    assert(exact_live_stats.last_frame_generation
        == exact_dyn_frame_audit.generation);
    ge_original_first_person_assets_close(&exact_assets);
    ge_asset_pack_close(&exact_pack);
    puts("exact unchanged WPPKSIL authored cartridge/casing path: ok");
    puts("exact visible WPPKSIL model-relations/beam path: ok");
    puts("exact visible WPPKSIL hand mtxlist -> ROM GBI scene path: ok");
    puts("exact WPPKSIL sustained movement-pose scene publication: ok");
    printf("shared gun->chr dyn arena: gun=%zu guard=%zu total=%zu/%zu; "
           "standalone gun frame: ok\n",
           exact_gun_frame_bytes, exact_guard_matrix_allocation_bytes,
           exact_gun_frame_bytes + exact_guard_matrix_allocation_bytes,
           exact_dyn_frame_audit.capacity);
#endif
    puts("exact generate_player_thrown_object ITEM_BUG chain: ok");
    puts("original both-hands/ITEM_BUG fire-to-projectile chain: ok");

    free(native_bytes);
    free(collision_bytes);
    return 0;
}
