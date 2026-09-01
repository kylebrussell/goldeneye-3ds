#include "ge_original_dam_guards.h"
#include "ge_original_dam_guard_scene.h"
#include "ge_original_dam_guard_weapon_model.h"
#include "ge_original_guard_grenade_model.h"
#include "ge_original_guard_grenade_object.h"
#include "ge_original_dam_intro.h"
#include "ge_original_prop_state.h"
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
#include "game/bondview.h"
#include "game/chrobjdata.h"
typedef int PLAYERFLAG;
#include "game/bondview.h"

extern stagesetup UsetupdamZ;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
static struct player guard_player;

struct player *ge_original_spawn_player_get(void)
{
    return &guard_player;
}
struct player *g_CurrentPlayer;

typedef struct GuardHarness {
    GeStanNativeMap native;
    GeOriginalPropState props;
} GuardHarness;

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

static stagesetup *load_setup(void *context, int32_t stage_id)
{
    (void)context;
    return stage_id == LEVELID_DAM ? &UsetupdamZ : NULL;
}

static float room_scale_reciprocal(void *context)
{
    (void)context;
    return 1.0f / 0.23363999f;
}

static void *resolve_stan(void *context, const char *name)
{
    GuardHarness *harness = context;
    return ge_original_stan_match_tile_name(&harness->native, name);
}

static int compare_u64(const void *left, const void *right)
{
    const uint64_t a = *(const uint64_t *)left;
    const uint64_t b = *(const uint64_t *)right;
    return (a > b) - (a < b);
}

static uint64_t monotonic_ns(void)
{
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (uint64_t)now.tv_sec * UINT64_C(1000000000)
        + (uint64_t)now.tv_nsec;
}

static uint64_t hash_bytes(uint64_t hash, const void *bytes, size_t size)
{
    const unsigned char *cursor = bytes;
    size_t index;
    for (index = 0U; index < size; index++) {
        hash ^= cursor[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static uint64_t scene_output_hash(const GeDamRoomWorldVertex *vertices,
                                  size_t vertex_count,
                                  const GeDamRoomDrawBatch *batches,
                                  size_t batch_count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    hash = hash_bytes(hash, vertices, vertex_count * sizeof(*vertices));
    return hash_bytes(hash, batches, batch_count * sizeof(*batches));
}

static uint64_t scene_semantic_hash(
    const GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count)
{
    uint64_t hash = UINT64_C(1469598103934665603);
    size_t index;
    for (index = 0U; index < vertex_count; index++) {
        int32_t quantized[5];
        size_t axis;
        hash = hash_bytes(hash, &vertices[index].source,
                          sizeof(vertices[index].source));
        for (axis = 0U; axis < 3U; axis++)
            quantized[axis] = (int32_t)lroundf(
                vertices[index].world[axis] * 100.0f);
        for (axis = 0U; axis < 2U; axis++)
            quantized[3U + axis] = (int32_t)lroundf(
                vertices[index].processed.texture[axis] * 100.0f);
        hash = hash_bytes(hash, quantized, sizeof(quantized));
        hash = hash_bytes(hash, vertices[index].processed.rgba,
                          sizeof(vertices[index].processed.rgba));
    }
    for (index = 0U; index < batch_count; index++) {
        hash = hash_bytes(hash, &batches[index].room_id,
                          sizeof(batches[index].room_id));
        hash = hash_bytes(hash, &batches[index].list_kind,
                          sizeof(batches[index].list_kind));
        hash = hash_bytes(hash, &batches[index].first_vertex,
                          sizeof(batches[index].first_vertex));
        hash = hash_bytes(hash, &batches[index].vertex_count,
                          sizeof(batches[index].vertex_count));
        hash = hash_bytes(hash, &batches[index].triangle_count,
                          sizeof(batches[index].triangle_count));
        hash = hash_bytes(hash, &batches[index].texture_valid,
                          sizeof(batches[index].texture_valid));
        hash = hash_bytes(hash, &batches[index].material.texture_id,
                          sizeof(batches[index].material.texture_id));
    }
    return hash;
}

int main(int argc, char **argv)
{
    GuardHarness harness;
    GeStanCollisionSurface surface;
    GeOriginalSetupPadProviders providers;
    GeOriginalSetupPadState pad_state;
    GeOriginalDamGuardStats stats;
    ChrRecord *guard;
    PropRecord *prop;
    Mtxf view;
    struct player player;
    unsigned char *collision_bytes;
    unsigned char *model_bytes;
    unsigned char *weapon_model_bytes;
    unsigned char *native_bytes;
    GeOriginalDamGuardScene scene;
    GeOriginalDamGuardSceneCache scene_cache = {0};
    GeOriginalModelSceneInput scene_inputs[
        GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS];
    GeDamRoomSceneStorage scene_storage;
    GeDamRoomWorldVertex *scene_vertices;
    GeDamRoomDrawBatch *scene_batches;
    GeDamRoomWorldVertex *legacy_vertices;
    GeDamRoomDrawBatch *legacy_batches;
    float *reference_world;
    size_t collision_size;
    size_t model_size;
    size_t weapon_model_size;
    size_t scene_input_count;
    size_t native_size;
    size_t index;

    assert(argc == 4);
    memset(&harness, 0, sizeof(harness));
    memset(&guard_player, 0, sizeof(guard_player));
    guard_player.c_lodscalez = 1.0f;
    memset(&player, 0, sizeof(player));
    player.c_lodscalez = 1.0f;
    g_CurrentPlayer = &player;
    collision_bytes = read_file(argv[1], &collision_size);
    model_bytes = read_file(argv[2], &model_size);
    weapon_model_bytes = read_file(argv[3], &weapon_model_size);
    assert(model_size == GE_ORIGINAL_DAM_GUARD_MODEL_BLOB_SIZE);
    assert(weapon_model_size
           == GE_ORIGINAL_DAM_GUARD_WEAPON_MODEL_BLOB_SIZE);
    assert(ge_stan_collision_open(collision_bytes, collision_size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f,
               native_bytes, native_size, &harness.native)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&harness.native)
           == GE_STAN_COLLISION_OK);
    assert(ge_original_prop_state_reset(&harness.props, 137U));

    memset(&providers, 0, sizeof(providers));
    providers.context = &harness;
    providers.load_setup = load_setup;
    providers.get_room_scale_reciprocal = room_scale_reciprocal;
    providers.resolve_stan = resolve_stan;
    ge_original_setup_pad_bind(&providers, &pad_state);
    ge_original_setup_pad_load(LEVELID_DAM);
    assert(pad_state.loaded && pad_state.resolved_pad_count > 0U);

    ge_original_dam_guards_reset();
    assert(ge_original_dam_guards_construct_initial()
           == GE_ORIGINAL_DAM_GUARD_OK);
    assert(ge_original_dam_guards_count()
           == GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY);
    assert(g_ChrSlots != NULL && g_NumChrSlots == 14);
    for (index = 0U; index < ge_original_dam_guards_count(); index++) {
        PropRecord *weapon_prop;
        WeaponObjRecord *weapon;
        ObjectRecord *weapon_object;
        GUNHAND authored_hand = index == 3U ? GUNLEFT : GUNRIGHT;
        prop = ge_original_dam_guard_prop(index);
        guard = ge_original_dam_guard_chr(index);
        assert(prop != NULL && guard != NULL);
        assert(prop->type == PROP_TYPE_CHR && prop->chr == guard);
        assert(prop->stan != NULL && guard->prop == prop);
        assert(guard->model != NULL && guard->model->obj != NULL);
        assert(guard->model->obj->numMatrices == 0x14);
        assert(guard->model->scale == 0.1f);
        assert(guard->bodynum == 37 && guard->headnum == -1);
        assert(guard->hearingscale == 1.0f);
        assert(guard->visionrange == 100.0f);
        assert(guard->chrwidth == 20.0f && guard->chrheight == 185.0f);
        assert(ge_original_prop_state_is_active(prop));
        assert(ge_original_prop_state_is_enabled(prop));
        assert(ge_original_prop_state_room_contains(prop->stan->room, prop));
        weapon_prop = ge_original_dam_guard_weapon_prop(index);
        assert(weapon_prop != NULL && weapon_prop->type == PROP_TYPE_WEAPON);
        assert(weapon_prop->parent == prop && prop->child == weapon_prop);
        assert(guard->weapons_held[authored_hand] == weapon_prop);
        assert(guard->weapons_held[1 - authored_hand] == NULL);
        weapon = weapon_prop->weapon;
        weapon_object = weapon_prop->obj;
        assert(weapon != NULL && weapon->weaponnum == ITEM_AK47);
        assert(weapon->extrascale == 0x0100U
               && weapon->state == 0U
               && weapon->type == PROPDEF_COLLECTABLE);
        assert(weapon->obj == PROP_CHRKALASH);
        assert(weapon->pad == (s16)index);
        assert((weapon->flags & PROPFLAG_ASSIGNEDTOCHR) != 0U);
        assert((weapon->flags & PROPFLAG_WEAPON_LEFTHANDED)
               == (index == 3U ? PROPFLAG_WEAPON_LEFTHANDED : 0U));
        assert(weapon_object->model != NULL
               && weapon_object->model->obj->Skeleton
                  == &skeleton_prop_weapon);
        assert(weapon_object->model->attachedto == guard->model);
        assert(weapon_object->model->attachedto_objinst
               == guard->model->obj->Switches[
                   authored_hand == GUNLEFT ? 5 : 3]);
        assert(fabsf(weapon_object->model->scale - 0.1f) < 0.0001f);
    }
    assert(guard_player.c_lodscalez == 1.0f);
    ge_original_dam_guards_snapshot(&stats);
    assert(stats.authored_weapons == 4U && stats.attached_weapons == 4U);

    /* Exact fresh chrGiveWeapon path used by TRYThrowingGrenade. Guard zero's
     * authored AK occupies the right hand, so the unchanged AI passes the
     * canonical left-handed flag and attaches PchrgrenadeZ to switch 5. */
    {
        GeOriginalGuardGrenadeObjectStats grenade_stats;
        PropRecord *grenade_prop;
        ObjectRecord *grenade_object;
        int32_t model_id;
        int32_t weapon_id;
        int32_t timer;
        uint32_t runtime_bitflags;
        const void *model_header;
        const void *parent;

        guard = ge_original_dam_guard_chr(0U);
        assert(guard != NULL && guard->weapons_held[GUNRIGHT] != NULL
               && guard->weapons_held[GUNLEFT] == NULL);
        ge_original_guard_grenade_object_reset();
        grenade_prop = ge_original_guard_grenade_object_create(
            guard, PROP_CHRGRENADE, ITEM_GRENADE,
            PROPFLAG_WEAPON_LEFTHANDED);
        ge_original_guard_grenade_object_snapshot(&grenade_stats);
        if (grenade_prop == NULL) {
            fprintf(stderr, "grenade constructor failed: calls=%u ok=%u "
                    "model=%u weapon=%u switches=%d left=%p root=%p\n",
                    grenade_stats.construction_calls,
                    grenade_stats.successful_constructions,
                    grenade_stats.model_slot_exhaustions,
                    grenade_stats.weapon_slot_exhaustions,
                    guard->model->obj->numSwitches,
                    (void *)guard->weapons_held[GUNLEFT],
                    ge_original_guard_grenade_model_header());
        }
        assert(grenade_prop != NULL && grenade_prop->weapon != NULL);
        assert(grenade_prop->parent == guard->prop
               && guard->prop->child == grenade_prop
               && guard->weapons_held[GUNLEFT] == grenade_prop);
        grenade_object = grenade_prop->obj;
        assert(grenade_object != NULL && grenade_object->model != NULL);
        assert(grenade_object->model->obj
               == ge_original_guard_grenade_model_header());
        assert(grenade_object->model->attachedto == guard->model);
        assert(grenade_object->model->attachedto_objinst
               == guard->model->obj->Switches[5]);
        assert(grenade_object->extrascale == 0x0100U
               && grenade_object->state == 0U
               && grenade_object->type == PROPDEF_COLLECTABLE);
        assert(grenade_object->obj == PROP_CHRGRENADE
               && grenade_object->pad == guard->chrnum);
        assert(grenade_object->flags
               == (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_WEAPON_LEFTHANDED));
        assert(grenade_prop->weapon->weaponnum == ITEM_GRENADE
               && grenade_prop->weapon->LinkedWeaponType == -1
               && grenade_prop->weapon->timer == -1);
        assert(fabsf(grenade_object->model->scale - 0.1f) < 0.0001f);
        assert(ge_original_guard_grenade_object_inspect(
                   grenade_prop, &model_id, &weapon_id, &timer,
                   &runtime_bitflags, &model_header, &parent));
        assert(model_id == PROP_CHRGRENADE && weapon_id == ITEM_GRENADE
               && timer == -1 && runtime_bitflags == 0U
               && model_header == ge_original_guard_grenade_model_header()
               && parent == guard->prop);
        ge_original_guard_grenade_object_snapshot(&grenade_stats);
        assert(grenade_stats.construction_calls == 1U
               && grenade_stats.successful_constructions == 1U
               && grenade_stats.model_slot_exhaustions == 0U
               && grenade_stats.weapon_slot_exhaustions == 0U);

        /* Keep the subsequent renderer assertion scoped to Dam's four
         * authored AK-47 records. This harness-only unlink does not exercise
         * or replace the production objDrop lifecycle. */
        guard->weapons_held[GUNLEFT] = NULL;
        guard->prop->child = grenade_prop->prev;
        assert(guard->prop->child != NULL);
        guard->prop->child->next = NULL;
        grenade_prop->parent = NULL;
        grenade_prop->prev = NULL;
        grenade_prop->next = NULL;
    }

    memset(&view, 0, sizeof(view));
    view.m[0][0] = view.m[1][1] = view.m[2][2] = view.m[3][3] = 1.0f;
    assert(ge_original_dam_guards_update_matrices(view.m)
           == GE_ORIGINAL_DAM_GUARD_OK);
    for (index = 0U; index < ge_original_dam_guards_count(); index++) {
        guard = ge_original_dam_guard_chr(index);
        assert(guard->model->render_pos != NULL);
        assert(isfinite(guard->model->render_pos[4].pos.m[3][0]));
        assert(isfinite(guard->model->render_pos[4].pos.m[3][1]));
        assert(isfinite(guard->model->render_pos[4].pos.m[3][2]));
        assert(fabsf(guard->model->render_pos[4].pos.m[3][0]
                    - guard->prop->pos.x) < 0.01f);
        assert(fabsf(guard->model->render_pos[4].pos.m[3][1]
                    - guard->prop->pos.y) < 0.01f);
        assert(fabsf(guard->model->render_pos[4].pos.m[3][2]
                    - guard->prop->pos.z) < 0.01f);
        assert(fabsf(guard->prop->zDepth
                    + guard->model->render_pos[4].pos.m[3][2]) < 0.01f);
        assert(guard->prop->flags & PROPFLAG_ONSCREEN);
        prop = ge_original_dam_guard_weapon_prop(index);
        assert(prop != NULL && (prop->flags & PROPFLAG_ONSCREEN) != 0U);
        assert(prop->obj != NULL && prop->obj->model != NULL);
        assert(prop->obj->model->render_pos != NULL);
        assert(prop->obj->model->obj->numMatrices == 1);
        assert(isfinite(prop->obj->model->render_pos[0].pos.m[3][0]));
        assert(isfinite(prop->obj->model->render_pos[0].pos.m[3][1]));
        assert(isfinite(prop->obj->model->render_pos[0].pos.m[3][2]));
    }

    /* chrTick owns the animated pose.  In particular ACT_ATTACK installs
     * chrHandleJointPositioned while calculating the torso/arm/head matrices,
     * then clears the callback before the native renderer publishes them.
     * Verify publication copies those already-positioned transient matrices
     * instead of recalculating a different pose. */
    {
        RenderPosView transient_body[0x14];
        RenderPosView transient_weapon[1];
        RenderPosView expected_body[0x14];
        RenderPosView expected_weapon[1];
        Model *body_model;
        Model *weapon_model;
        PropRecord *body_prop = ge_original_dam_guard_prop(0U);
        PropRecord *weapon_prop = ge_original_dam_guard_weapon_prop(0U);
        size_t matrix;
        size_t row;
        size_t column;

        assert(body_prop != NULL && body_prop->chr != NULL
               && weapon_prop != NULL && weapon_prop->obj != NULL);
        body_model = body_prop->chr->model;
        weapon_model = weapon_prop->obj->model;
        assert(body_model != NULL && body_model->obj != NULL
               && body_model->obj->numMatrices <= 0x14
               && weapon_model != NULL && weapon_model->obj != NULL
               && weapon_model->obj->numMatrices == 1);
        memset(transient_body, 0, sizeof(transient_body));
        memset(transient_weapon, 0, sizeof(transient_weapon));
        for (matrix = 0U;
                matrix < (size_t)body_model->obj->numMatrices; matrix++) {
            for (row = 0U; row < 4U; row++)
                transient_body[matrix].pos.m[row][row] = 1.0f;
            /* Distinct arm/attachment values model the canonical callback's
             * attack-frame result and make any second calculation visible. */
            for (column = 0U; column < 3U; column++)
                transient_body[matrix].pos.m[3][column] =
                    1000.0f + (float)(matrix * 10U + column);
        }
        for (row = 0U; row < 4U; row++)
            transient_weapon[0].pos.m[row][row] = 1.0f;
        transient_weapon[0].pos.m[3][0] = 2001.0f;
        transient_weapon[0].pos.m[3][1] = 2002.0f;
        transient_weapon[0].pos.m[3][2] = 2003.0f;
        memcpy(expected_body, transient_body, sizeof(expected_body));
        memcpy(expected_weapon, transient_weapon, sizeof(expected_weapon));
        body_model->render_pos = transient_body;
        weapon_model->render_pos = transient_weapon;
        body_prop->zDepth = 123.25f;
        body_prop->flags |= PROPFLAG_ONSCREEN;
        weapon_prop->flags |= PROPFLAG_ONSCREEN;

        assert(ge_original_dam_guards_update_visible_matrices(view.m)
               == GE_ORIGINAL_DAM_GUARD_OK);
        assert(body_model->render_pos != transient_body
               && weapon_model->render_pos != transient_weapon);
        assert(memcmp(body_model->render_pos, expected_body,
                      (size_t)body_model->obj->numMatrices
                          * sizeof(expected_body[0])) == 0);
        assert(memcmp(weapon_model->render_pos, expected_weapon,
                      sizeof(expected_weapon)) == 0);
        assert(body_prop->zDepth == 123.25f);

        /* Restore a normal canonical base pose for the scene assertions
         * below; the non-live entry point remains the initialization path. */
        assert(ge_original_dam_guards_update_matrices(view.m)
               == GE_ORIGINAL_DAM_GUARD_OK);
    }

    /* Live publication must preserve the unchanged chrTick visibility bit.
     * The renderer keeps fixed topology, but maps every display-list input
     * for an offscreen guard to the non-authored room 0xff. */
    {
        GeOriginalDamGuardStats before_visible;
        GeOriginalDamGuardStats after_visible;
        size_t hidden_inputs = 0U;
        PropRecord *hidden = ge_original_dam_guard_prop(0U);
        PropRecord *hidden_weapon =
            ge_original_dam_guard_weapon_prop(0U);
        assert(hidden != NULL && hidden_weapon != NULL);
        hidden->flags &= (u8)~PROPFLAG_ONSCREEN;
        hidden_weapon->flags &= (u8)~PROPFLAG_ONSCREEN;
        ge_original_dam_guards_snapshot(&before_visible);
        assert(ge_original_dam_guards_update_visible_matrices(view.m)
               == GE_ORIGINAL_DAM_GUARD_OK);
        ge_original_dam_guards_snapshot(&after_visible);
        assert((hidden->flags & PROPFLAG_ONSCREEN) == 0U);
        assert((hidden_weapon->flags & PROPFLAG_ONSCREEN) == 0U);
        assert(after_visible.matrix_updates
               == before_visible.matrix_updates + 3U);
        assert(ge_original_dam_guard_scene_inputs_with_weapons(
                   model_bytes, model_size,
                   weapon_model_bytes, weapon_model_size, view.m,
                   scene_inputs, GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
                   &scene_input_count)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        for (index = 0U; index < scene_input_count; ++index)
            if (scene_inputs[index].room_id == UINT8_MAX) hidden_inputs++;
        assert(hidden_inputs > 0U && hidden_inputs < scene_input_count);
        hidden->flags |= PROPFLAG_ONSCREEN;
        hidden_weapon->flags |= PROPFLAG_ONSCREEN;
    }

    {
        const GeOriginalDamGuardSceneStatus scene_status =
            ge_original_dam_guard_scene_build(
                model_bytes, model_size, view.m, NULL, &scene);
        if (scene_status != GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED) {
            assert(ge_original_dam_guard_scene_inputs(
                       model_bytes, model_size, view.m, scene_inputs,
                       GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
                       &scene_input_count)
                   == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
            for (index = 0U; index < scene_input_count; ++index) {
                GeOriginalModelScene input_scene;
                const GeOriginalModelSceneStatus input_status =
                    ge_original_model_scene_build(
                        &scene_inputs[index], NULL, &input_scene);
                if (input_status !=
                        GE_ORIGINAL_MODEL_SCENE_CAPACITY_EXCEEDED) {
                    fprintf(stderr, "guard input %zu offset 0x%x vertex "
                            "0x%x: %s (%d)\n", index,
                            scene_inputs[index].primary_offset,
                            scene_inputs[index].segment4_offset,
                            ge_original_model_scene_status_name(input_status),
                            (int)input_status);
                    break;
                }
            }
            fprintf(stderr, "guard scene query: %s (%d), inputs=%zu, "
                    "vertices=%zu, batches=%zu, commands=%zu\n",
                    ge_original_dam_guard_scene_status_name(scene_status),
                    (int)scene_status, scene.input_count,
                    scene.required_vertex_count,
                    scene.required_batch_count, scene.commands_visited);
        }
        assert(scene_status == GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED);
    }
    assert(scene.guard_count == GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY);
    assert(scene.input_count > 0U
           && scene.input_count <= GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS);
    assert(scene.required_vertex_count > 0U);
    assert(scene.required_batch_count > 0U);
    assert(scene.triangle_count * 3U == scene.required_vertex_count);
    scene_vertices = calloc(scene.required_vertex_count,
                            sizeof(*scene_vertices));
    scene_batches = calloc(scene.required_batch_count,
                           sizeof(*scene_batches));
    assert(scene_vertices != NULL && scene_batches != NULL);
    scene_storage.vertices = scene_vertices;
    scene_storage.vertex_capacity = scene.required_vertex_count;
    scene_storage.batches = scene_batches;
    scene_storage.batch_capacity = scene.required_batch_count;
    assert(ge_original_dam_guard_scene_build(
               model_bytes, model_size, view.m, &scene_storage, &scene)
           == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
    assert(scene.vertex_count == scene.required_vertex_count);
    assert(scene.batch_count == scene.required_batch_count);
    legacy_vertices = malloc(scene.vertex_count * sizeof(*legacy_vertices));
    legacy_batches = malloc(scene.batch_count * sizeof(*legacy_batches));
    assert(legacy_vertices != NULL && legacy_batches != NULL);
    memcpy(legacy_vertices, scene_vertices,
           scene.vertex_count * sizeof(*legacy_vertices));
    memcpy(legacy_batches, scene_batches,
           scene.batch_count * sizeof(*legacy_batches));
    assert(ge_original_dam_guard_scene_cache_init(&scene_cache));
    assert(ge_original_dam_guard_scene_build_cached(
               &scene_cache, model_bytes, model_size, view.m,
               NULL, &scene)
           == GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED);
    assert(scene_cache.topology_rebuilds == 1U
           && scene_cache.single_pass_builds == 0U);
    assert(ge_original_dam_guard_scene_build_cached(
               &scene_cache, model_bytes, model_size, view.m,
               &scene_storage, &scene)
           == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
    assert(scene_cache.topology_rebuilds == 1U
           && scene_cache.single_pass_builds == 1U);
    for (index = 0U; index < scene.vertex_count; index++) {
        size_t axis;
        assert(memcmp(&legacy_vertices[index].source,
                      &scene_vertices[index].source,
                      sizeof(legacy_vertices[index].source)) == 0);
        for (axis = 0U; axis < 3U; axis++)
            assert(fabsf(legacy_vertices[index].world[axis]
                         - scene_vertices[index].world[axis]) < 0.001f);
        for (axis = 0U; axis < 2U; axis++)
            assert(fabsf(legacy_vertices[index].processed.texture[axis]
                         - scene_vertices[index].processed.texture[axis])
                   < 0.001f);
        assert(memcmp(legacy_vertices[index].processed.rgba,
                      scene_vertices[index].processed.rgba,
                      sizeof(legacy_vertices[index].processed.rgba)) == 0);
    }
    for (index = 0U; index < scene.batch_count; index++) {
        assert(legacy_batches[index].room_id == scene_batches[index].room_id);
        assert(legacy_batches[index].list_kind
               == scene_batches[index].list_kind);
        assert(legacy_batches[index].first_vertex
               == scene_batches[index].first_vertex);
        assert(legacy_batches[index].vertex_count
               == scene_batches[index].vertex_count);
        assert(legacy_batches[index].triangle_count
               == scene_batches[index].triangle_count);
        assert(legacy_batches[index].texture_valid
               == scene_batches[index].texture_valid);
        assert(legacy_batches[index].material.texture_id
               == scene_batches[index].material.texture_id);
    }
    for (index = 0U; index < scene.vertex_count; ++index) {
        assert(isfinite(scene_vertices[index].world[0]));
        assert(isfinite(scene_vertices[index].world[1]));
        assert(isfinite(scene_vertices[index].world[2]));
    }
    for (index = 0U; index < scene.batch_count; ++index) {
        size_t guard_index;
        int authored_room = 0;
        assert(scene_batches[index].vertex_count > 0U);
        assert(scene_batches[index].triangle_count * 3U
               == scene_batches[index].vertex_count);
        assert(scene_batches[index].first_vertex
                   + scene_batches[index].vertex_count
               <= scene.vertex_count);
        for (guard_index = 0U;
                guard_index < ge_original_dam_guards_count(); ++guard_index) {
            PropRecord *batch_guard =
                ge_original_dam_guard_prop(guard_index);
            if (batch_guard != NULL && batch_guard->stan != NULL
                    && scene_batches[index].room_id
                       == (uint32_t)batch_guard->stan->room) {
                authored_room = 1;
                break;
            }
        }
        assert(authored_room);
    }
    reference_world = malloc(scene.vertex_count * 3U
                             * sizeof(*reference_world));
    assert(reference_world != NULL);
    for (index = 0U; index < scene.vertex_count; ++index)
        memcpy(reference_world + index * 3U, scene_vertices[index].world,
               3U * sizeof(*reference_world));
    view.m[3][0] = 128.0f;
    view.m[3][1] = -64.0f;
    view.m[3][2] = 32.0f;
    assert(ge_original_dam_guards_update_matrices(view.m)
           == GE_ORIGINAL_DAM_GUARD_OK);
    view.m[3][0] = -128.0f;
    view.m[3][1] = 64.0f;
    view.m[3][2] = -32.0f;
    assert(ge_original_dam_guard_scene_build_cached(
               &scene_cache, model_bytes, model_size, view.m,
               &scene_storage, &scene)
           == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
    assert(scene_cache.topology_rebuilds == 1U
           && scene_cache.single_pass_builds == 2U);
    {
        GeDamRoomSceneStorage legacy_storage = {
            legacy_vertices, scene.vertex_count,
            legacy_batches, scene.batch_count,
        };
        GeOriginalDamGuardScene legacy_scene;
        assert(ge_original_dam_guard_scene_build(
                   model_bytes, model_size, view.m,
                   &legacy_storage, &legacy_scene)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        assert(legacy_scene.vertex_count == scene.vertex_count
               && legacy_scene.batch_count == scene.batch_count);
        for (index = 0U; index < scene.vertex_count; index++) {
            size_t axis;
            for (axis = 0U; axis < 3U; axis++)
                assert(fabsf(legacy_vertices[index].world[axis]
                             - scene_vertices[index].world[axis]) < 0.001f);
            for (axis = 0U; axis < 2U; axis++)
                assert(fabsf(legacy_vertices[index].processed.texture[axis]
                             - scene_vertices[index].processed.texture[axis])
                       < 0.001f);
            assert(memcmp(legacy_vertices[index].processed.rgba,
                          scene_vertices[index].processed.rgba,
                          sizeof(legacy_vertices[index].processed.rgba)) == 0);
        }
    }
    for (index = 0U; index < scene.vertex_count; ++index) {
        size_t axis;
        for (axis = 0U; axis < 3U; ++axis)
            assert(fabsf(scene_vertices[index].world[axis]
                         - reference_world[index * 3U + axis]) < 0.01f);
    }

    if (getenv("GE_GUARD_SCENE_BENCH") != NULL) {
        const char *iterations_text = getenv("GE_GUARD_SCENE_BENCH_ITERS");
        size_t iterations = iterations_text != NULL
            ? (size_t)strtoull(iterations_text, NULL, 10) : 5000U;
        uint64_t *legacy_times;
        uint64_t *cached_times;
        GeDamRoomSceneStorage legacy_storage = {
            legacy_vertices, scene.vertex_count,
            legacy_batches, scene.batch_count,
        };
        GeOriginalDamGuardScene legacy_scene;
        GeOriginalDamGuardScene cached_scene;
        uint64_t legacy_hash;
        uint64_t cached_hash;
        uint64_t legacy_semantic_hash;
        uint64_t cached_semantic_hash;
        size_t warmup;
        int semantic_equal = 1;

        assert(iterations > 0U && iterations <= 1000000U);
        legacy_times = malloc(iterations * sizeof(*legacy_times));
        cached_times = malloc(iterations * sizeof(*cached_times));
        assert(legacy_times != NULL && cached_times != NULL);
        for (warmup = 0U; warmup < 100U; warmup++) {
            assert(ge_original_dam_guard_scene_build(
                       model_bytes, model_size, view.m,
                       &legacy_storage, &legacy_scene)
                   == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
            assert(ge_original_dam_guard_scene_build_cached(
                       &scene_cache, model_bytes, model_size, view.m,
                       &scene_storage, &cached_scene)
                   == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        }
        for (index = 0U; index < iterations; index++) {
            uint64_t started;
            if ((index & 1U) == 0U) {
                started = monotonic_ns();
                assert(ge_original_dam_guard_scene_build(
                           model_bytes, model_size, view.m,
                           &legacy_storage, &legacy_scene)
                       == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
                legacy_times[index] = monotonic_ns() - started;
                started = monotonic_ns();
                assert(ge_original_dam_guard_scene_build_cached(
                           &scene_cache, model_bytes, model_size, view.m,
                           &scene_storage, &cached_scene)
                       == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
                cached_times[index] = monotonic_ns() - started;
            } else {
                started = monotonic_ns();
                assert(ge_original_dam_guard_scene_build_cached(
                           &scene_cache, model_bytes, model_size, view.m,
                           &scene_storage, &cached_scene)
                       == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
                cached_times[index] = monotonic_ns() - started;
                started = monotonic_ns();
                assert(ge_original_dam_guard_scene_build(
                           model_bytes, model_size, view.m,
                           &legacy_storage, &legacy_scene)
                       == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
                legacy_times[index] = monotonic_ns() - started;
            }
        }
        assert(legacy_scene.vertex_count == cached_scene.vertex_count);
        assert(legacy_scene.batch_count == cached_scene.batch_count);
        for (index = 0U; index < cached_scene.vertex_count; index++) {
            size_t axis;
            if (memcmp(&legacy_vertices[index].source,
                       &scene_vertices[index].source,
                       sizeof(legacy_vertices[index].source)) != 0
                    || memcmp(legacy_vertices[index].processed.rgba,
                              scene_vertices[index].processed.rgba,
                              sizeof(legacy_vertices[index].processed.rgba))
                       != 0)
                semantic_equal = 0;
            for (axis = 0U; axis < 3U; axis++)
                if (fabsf(legacy_vertices[index].world[axis]
                          - scene_vertices[index].world[axis]) >= 0.001f)
                    semantic_equal = 0;
            for (axis = 0U; axis < 2U; axis++)
                if (fabsf(legacy_vertices[index].processed.texture[axis]
                          - scene_vertices[index].processed.texture[axis])
                    >= 0.001f)
                    semantic_equal = 0;
        }
        for (index = 0U; index < cached_scene.batch_count; index++) {
            if (legacy_batches[index].room_id != scene_batches[index].room_id
                    || legacy_batches[index].list_kind
                       != scene_batches[index].list_kind
                    || legacy_batches[index].first_vertex
                       != scene_batches[index].first_vertex
                    || legacy_batches[index].vertex_count
                       != scene_batches[index].vertex_count
                    || legacy_batches[index].triangle_count
                       != scene_batches[index].triangle_count
                    || legacy_batches[index].texture_valid
                       != scene_batches[index].texture_valid
                    || legacy_batches[index].material.texture_id
                       != scene_batches[index].material.texture_id)
                semantic_equal = 0;
        }
        legacy_hash = scene_output_hash(
            legacy_vertices, legacy_scene.vertex_count,
            legacy_batches, legacy_scene.batch_count);
        cached_hash = scene_output_hash(
            scene_vertices, cached_scene.vertex_count,
            scene_batches, cached_scene.batch_count);
        legacy_semantic_hash = scene_semantic_hash(
            legacy_vertices, legacy_scene.vertex_count,
            legacy_batches, legacy_scene.batch_count);
        cached_semantic_hash = scene_semantic_hash(
            scene_vertices, cached_scene.vertex_count,
            scene_batches, cached_scene.batch_count);
        qsort(legacy_times, iterations, sizeof(*legacy_times), compare_u64);
        qsort(cached_times, iterations, sizeof(*cached_times), compare_u64);
        printf("guard-scene benchmark: n=%zu inputs=%zu triangles=%zu "
               "vertices=%zu batches=%zu commands=%zu\n",
               iterations, cached_scene.input_count,
               cached_scene.triangle_count, cached_scene.vertex_count,
               cached_scene.batch_count, cached_scene.commands_visited);
        printf("legacy: median=%.3f us p95=%.3f us, two GBI traversals "
               "(%zu command events) per frame\n",
               (double)legacy_times[iterations / 2U] / 1000.0,
               (double)legacy_times[((iterations * 95U + 99U) / 100U) - 1U]
                   / 1000.0,
               legacy_scene.commands_visited * 2U);
        printf("cached: median=%.3f us p95=%.3f us, zero GBI traversals, "
               "%zu matrix-transformed vertices per frame\n",
               (double)cached_times[iterations / 2U] / 1000.0,
               (double)cached_times[((iterations * 95U + 99U) / 100U) - 1U]
                   / 1000.0,
               cached_scene.vertex_count);
        printf("speedup: median=%.2fx p95=%.2fx; raw hashes "
               "%016llx/%016llx, raw_equal=%s; semantic hashes "
               "%016llx/%016llx, semantic_equal=%s\n",
               (double)legacy_times[iterations / 2U]
                   / (double)cached_times[iterations / 2U],
               (double)legacy_times[((iterations * 95U + 99U) / 100U) - 1U]
                   / (double)cached_times[((iterations * 95U + 99U) / 100U) - 1U],
               (unsigned long long)legacy_hash,
               (unsigned long long)cached_hash,
               legacy_hash == cached_hash ? "yes" : "no",
               (unsigned long long)legacy_semantic_hash,
               (unsigned long long)cached_semantic_hash,
               semantic_equal ? "yes" : "no");
        assert(semantic_equal
               && legacy_semantic_hash == cached_semantic_hash);
        free(cached_times);
        free(legacy_times);
    }

    /* Extend the same persistent overlay with the exact PchrkalashZ display
     * lists and matrices published by the held-weapon relation. Prove the
     * cached renderer is semantically identical to a fresh GBI traversal. */
    {
        GeOriginalDamGuardScene weapon_query;
        GeOriginalDamGuardScene weapon_legacy;
        GeOriginalDamGuardScene weapon_cached;
        GeOriginalDamGuardSceneCache weapon_cache = {0};
        GeDamRoomWorldVertex *weapon_legacy_vertices;
        GeDamRoomWorldVertex *weapon_cached_vertices;
        GeDamRoomDrawBatch *weapon_legacy_batches;
        GeDamRoomDrawBatch *weapon_cached_batches;
        GeDamRoomSceneStorage weapon_legacy_storage;
        GeDamRoomSceneStorage weapon_cached_storage;
        size_t combined_input_count = 0U;
        size_t weapon_input_count = 0U;

        assert(ge_original_dam_guard_scene_inputs_with_weapons(
                   model_bytes, model_size,
                   weapon_model_bytes, weapon_model_size, view.m,
                   scene_inputs, GE_ORIGINAL_DAM_GUARD_MAX_SCENE_INPUTS,
                   &combined_input_count)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        for (index = 0U; index < combined_input_count; index++) {
            if (scene_inputs[index].blob == weapon_model_bytes) {
                assert(scene_inputs[index].blob_size == weapon_model_size);
                assert(scene_inputs[index].primary_offset == UINT32_C(0x780));
                assert(scene_inputs[index].secondary_offset == UINT32_C(0x8d8));
                assert(scene_inputs[index].segment4_offset
                       == GE_ORIGINAL_MODEL_SCENE_NO_LIST);
                assert(scene_inputs[index].segment3_matrix_count == 1U);
                weapon_input_count++;
            }
        }
        assert(weapon_input_count == GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY);
        assert(combined_input_count == scene.input_count + weapon_input_count);
        assert(ge_original_dam_guard_scene_build_with_weapons(
                   model_bytes, model_size,
                   weapon_model_bytes, weapon_model_size, view.m,
                   NULL, &weapon_query)
               == GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED);
        assert(weapon_query.input_count == combined_input_count);
        assert(weapon_query.required_vertex_count > scene.vertex_count);
        assert(weapon_query.required_batch_count > scene.batch_count);
        assert(weapon_query.triangle_count > scene.triangle_count);

        weapon_legacy_vertices = calloc(
            weapon_query.required_vertex_count,
            sizeof(*weapon_legacy_vertices));
        weapon_cached_vertices = calloc(
            weapon_query.required_vertex_count,
            sizeof(*weapon_cached_vertices));
        weapon_legacy_batches = calloc(
            weapon_query.required_batch_count,
            sizeof(*weapon_legacy_batches));
        weapon_cached_batches = calloc(
            weapon_query.required_batch_count,
            sizeof(*weapon_cached_batches));
        assert(weapon_legacy_vertices != NULL
               && weapon_cached_vertices != NULL
               && weapon_legacy_batches != NULL
               && weapon_cached_batches != NULL);
        weapon_legacy_storage = (GeDamRoomSceneStorage) {
            weapon_legacy_vertices, weapon_query.required_vertex_count,
            weapon_legacy_batches, weapon_query.required_batch_count,
        };
        weapon_cached_storage = (GeDamRoomSceneStorage) {
            weapon_cached_vertices, weapon_query.required_vertex_count,
            weapon_cached_batches, weapon_query.required_batch_count,
        };
        assert(ge_original_dam_guard_scene_build_with_weapons(
                   model_bytes, model_size,
                   weapon_model_bytes, weapon_model_size, view.m,
                   &weapon_legacy_storage, &weapon_legacy)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        assert(ge_original_dam_guard_scene_cache_init(&weapon_cache));
        assert(ge_original_dam_guard_scene_build_cached_with_weapons(
                   &weapon_cache, model_bytes, model_size,
                   weapon_model_bytes, weapon_model_size, view.m,
                   &weapon_cached_storage, &weapon_cached)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        assert(weapon_cache.topology_rebuilds == 1U
               && weapon_cache.single_pass_builds == 1U);
        assert(weapon_legacy.vertex_count == weapon_cached.vertex_count);
        assert(weapon_legacy.batch_count == weapon_cached.batch_count);
        assert(scene_semantic_hash(
                   weapon_legacy_vertices, weapon_legacy.vertex_count,
                   weapon_legacy_batches, weapon_legacy.batch_count)
               == scene_semantic_hash(
                   weapon_cached_vertices, weapon_cached.vertex_count,
                   weapon_cached_batches, weapon_cached.batch_count));
        for (index = 0U; index < weapon_cached.vertex_count; index++) {
            size_t axis;
            assert(memcmp(&weapon_legacy_vertices[index].source,
                          &weapon_cached_vertices[index].source,
                          sizeof(weapon_legacy_vertices[index].source)) == 0);
            for (axis = 0U; axis < 3U; axis++)
                assert(fabsf(weapon_legacy_vertices[index].world[axis]
                             - weapon_cached_vertices[index].world[axis])
                       < 0.001f);
        }
        ge_original_dam_guard_scene_cache_close(&weapon_cache);
        free(weapon_cached_batches);
        free(weapon_legacy_batches);
        free(weapon_cached_vertices);
        free(weapon_legacy_vertices);
    }

    /* Canonical chrpropCleanupForRemoval clears model/chrnum before the
     * scheduler frees the prop. Reproduce those exact durable postconditions,
     * return the prop to the shared pool, and prove presentation never touches
     * that stale slot during later refreshes. */
    {
        PropRecord *removed_prop = ge_original_dam_guard_prop(0U);
        ChrRecord *removed_chr = ge_original_dam_guard_chr(0U);
        size_t refresh;
        extern void chrpropDeregisterRooms(PropRecord *prop);
        extern PropRecord *g_ActivePropsHead;
        extern PropRecord *g_ActivePropsTail;
        extern PropRecord *g_FreeProps;
        assert(removed_prop != NULL && removed_chr != NULL);
        removed_chr->model = NULL;
        removed_chr->chrnum = -1;
        chrpropDeregisterRooms(removed_prop);
        if (removed_prop->prev != NULL)
            removed_prop->prev->next = removed_prop->next;
        else
            g_ActivePropsHead = removed_prop->next;
        if (removed_prop->next != NULL)
            removed_prop->next->prev = removed_prop->prev;
        else
            g_ActivePropsTail = removed_prop->prev;
        removed_prop->flags &= (u8)~PROPFLAG_ENABLED;
        removed_prop->next = NULL;
        removed_prop->prev = g_FreeProps;
        g_FreeProps = removed_prop;
        assert(!ge_original_dam_guard_is_live(0U));
        assert(ge_original_dam_guards_live_count() == 3U);
        for (refresh = 0U; refresh < 4U; refresh++)
            assert(ge_original_dam_guards_update_matrices(view.m)
                   == GE_ORIGINAL_DAM_GUARD_OK);
        assert(ge_original_dam_guard_scene_build_cached(
                   &scene_cache, model_bytes, model_size, view.m,
                   NULL, &scene)
               == GE_ORIGINAL_DAM_GUARD_SCENE_CAPACITY_EXCEEDED);
        assert(scene.guard_count == 3U && scene.input_count > 0U);
        assert(scene_cache.topology_rebuilds == 2U);
        assert(ge_original_dam_guard_scene_build_cached(
                   &scene_cache, model_bytes, model_size, view.m,
                   &scene_storage, &scene)
               == GE_ORIGINAL_DAM_GUARD_SCENE_OK);
        assert(scene.guard_count == 3U);
        if (getenv("GE_GUARD_SCENE_BENCH") == NULL)
            assert(scene_cache.single_pass_builds == 3U);
        else
            assert(scene_cache.single_pass_builds > 3U);
    }
    ge_original_dam_guards_snapshot(&stats);
    assert(stats.constructed_guards == 4U);
    assert(stats.first_command_index == 23U);
    assert(stats.last_command_index == 26U);
    assert(stats.matrix_updates == 31U);
    printf("original Dam guards 23..26: greatguard2 model/prop/chr/matrices "
           "and scene %zu inputs, %zu triangles, %zu batches ok\n",
           scene.input_count, scene.triangle_count, scene.batch_count);

    ge_original_dam_guard_scene_cache_close(&scene_cache);
    free(legacy_batches);
    free(legacy_vertices);
    free(scene_batches);
    free(scene_vertices);
    free(reference_world);
    free(native_bytes);
    free(weapon_model_bytes);
    free(model_bytes);
    free(collision_bytes);
    return 0;
}
