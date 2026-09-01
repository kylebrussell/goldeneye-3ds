#include "ge_original_guard_bullet_hit.h"
#include "ge_original_guard_animation_table.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_pp7_fire.h"
#include "ge_original_dam_guards.h"
#include "ge_original_prop_state.h"
#include "ge_original_dam_intro.h"
#include "ge_original_dam_guard_ai_tick.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/model.h"
#include "game/player.h"
#include "game/vtxstore.h"
#include "game/chr.h"
#include "memp.h"

extern stagesetup UsetupdamZ;
extern PropRecord *chrpropAllocate(void);
extern void chrpropActivate(PropRecord *prop);
extern void chrpropEnable(PropRecord *prop);
extern void chrpropDeregisterRooms(PropRecord *prop);
extern void chrpropFree(PropRecord *prop);
extern PropRecord *g_ActivePropsHead;
extern PropRecord *g_ActivePropsTail;
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
extern struct player_data *g_playerPerm;
extern struct player *g_playerPointers[4];
extern s32 player_num;
extern MemoryPool g_mempPools[MEMPOOL_COUNT];
extern struct animation_table_data *ptr_animation_table;
extern void initWeaponAnimGroups(void);
#ifdef GE_TEST_GUARD_DEATH_TICK
s32 g_ClockTimer;
#endif
#endif
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
struct player *g_CurrentPlayer;

struct player *ge_original_spawn_player_get(void)
{
    return g_CurrentPlayer;
}

typedef struct GuardHitHarness {
    GeStanNativeMap native;
    GeOriginalPropState props;
} GuardHitHarness;

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
    GuardHitHarness *harness = context;
    return ge_original_stan_match_tile_name(&harness->native, name);
}

static size_t stage_guard_count(void *context)
{
    assert(context != NULL);
    return ge_original_dam_guards_count();
}

static int stage_guard_actor(void *context, size_t index,
                             void **prop_record, void **chr_record)
{
    assert(context != NULL);
    if (prop_record == NULL || chr_record == NULL
            || index >= ge_original_dam_guards_count()) return 0;
    *prop_record = ge_original_dam_guard_prop(index);
    *chr_record = ge_original_dam_guard_chr(index);
    return *prop_record != NULL && *chr_record != NULL;
}

static ModelNode *find_bbox(ModelNode *node)
{
    while (node != NULL) {
        ModelNode *found;
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_BBOX
                && *(s32 *)node->Data > 0)
            return node;
        found = find_bbox(node->Child);
        if (found != NULL) return found;
        node = node->Next;
    }
    return NULL;
}

static void direction_to_guard(ChrRecord *guard, float direction[3])
{
    ModelNode *bbox_node;
    ModelRoData_BoundingBoxRecord *bbox;
    Mtxf *bbox_mtx;
    coord3d center;
    float length;

    assert(guard != NULL && guard->model != NULL);
    bbox_node = find_bbox(guard->model->obj->RootNode);
    assert(bbox_node != NULL);
    bbox = &bbox_node->Data->BoundingBox;
    bbox_mtx = modelFindNodeMtx(guard->model, bbox_node, 0);
    assert(bbox_mtx != NULL);
    center.x = 0.5f * (bbox->Bounds.xmin + bbox->Bounds.xmax);
    center.y = 0.5f * (bbox->Bounds.ymin + bbox->Bounds.ymax);
    center.z = 0.5f * (bbox->Bounds.zmin + bbox->Bounds.zmax);
    mtx4TransformVecInPlace(bbox_mtx, &center);
    length = sqrtf(center.x * center.x + center.y * center.y
                   + center.z * center.z);
    assert(length > 0.0f);
    direction[0] = center.x / length;
    direction[1] = center.y / length;
    direction[2] = center.z / length;
}

#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
static size_t count_deformed_collision_nodes(Model *model, ModelNode *node)
{
    size_t count = 0U;

    while (node != NULL) {
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_DLCOLLISION) {
            ModelRoData_DisplayList_CollisionRecord *rodata =
                &node->Data->DisplayListCollisions;
            ModelRwData_DisplayList_CollisionRecord *rwdata =
                &modelGetNodeRwData(model, node)->DisplayListCollisions;
            if (rwdata->Vertices != rodata->Vertices) {
                count++;
            }
        }
        if (node->Child != NULL) {
            count += count_deformed_collision_nodes(model, node->Child);
        }
        node = node->Next;
    }

    return count;
}
#endif

int main(int argc, char **argv)
{
    GuardHitHarness harness;
    GeStanCollisionSurface surface;
    GeOriginalSetupPadProviders providers;
    GeOriginalSetupPadState pad_state;
    GeOriginalGuardBulletHitStats stats;
    struct player player;
    struct player_data permissions;
    Mtxf identity;
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
    Mtxf world_to_view;
    unsigned char *animation_bytes;
#ifdef GE_TEST_GUARD_DEATH_TICK
    unsigned char *animation_entry_bytes;
#endif
#endif
    ChrRecord *guard;
    PropRecord *player_prop;
    ModelNode *bbox_node;
    ModelRoData_BoundingBoxRecord *bbox;
    Mtxf *bbox_mtx;
    coord3d center;
    float origin[3] = {0.0f, 0.0f, 0.0f};
    float direction[3];
    float length;
    unsigned char *collision_bytes;
    unsigned char *native_bytes;
    unsigned char *vtx_arena;
    size_t collision_size;
    size_t native_size;

#if defined(GE_TEST_GUARD_DEATH_TICK)
    assert(argc == 4);
#elif defined(GE_TEST_GUARD_DAMAGE_CONSEQUENCE)
    assert(argc == 3);
#else
    assert(argc == 2);
#endif
    memset(&harness, 0, sizeof(harness));
    memset(&player, 0, sizeof(player));
    memset(&permissions, 0, sizeof(permissions));
    memset(&identity, 0, sizeof(identity));
    identity.m[0][0] = identity.m[1][1] = identity.m[2][2]
        = identity.m[3][3] = 1.0f;
    player.c_lodscalez = 1.0f;
    player.viewtoworldmtxf = &identity;
    g_CurrentPlayer = &player;
    ge_original_bond_input_bind_player(&player, &permissions);
    ge_original_bond_input_provider_reset_normal_dam();
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
    g_playerPointers[0] = &player;
    g_playerPerm = &permissions;
    player_num = 0;
#endif

    collision_bytes = read_file(argv[1], &collision_size);
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
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
    {
        size_t animation_size;
        animation_bytes = read_file(argv[2], &animation_size);
        assert(ge_original_guard_animation_table_bind(
                   animation_bytes, animation_size));
#ifdef GE_TEST_GUARD_DEATH_TICK
        animation_entry_bytes = read_file(argv[3], &animation_size);
        assert(ge_original_guard_animation_entries_bind(
                   animation_entry_bytes, animation_size));
#endif
    }
    vtx_arena = malloc(65536U);
    assert(vtx_arena != NULL);
    memset(&g_mempPools[MEMPOOL_STAGE], 0,
           sizeof(g_mempPools[MEMPOOL_STAGE]));
    g_mempPools[MEMPOOL_STAGE].start = vtx_arena;
    g_mempPools[MEMPOOL_STAGE].pos = vtx_arena;
    g_mempPools[MEMPOOL_STAGE].end = vtx_arena + 65536U;
    sub_GAME_7F09B820();
    initWeaponAnimGroups();
    assert(g_HitReactionTable[8].hitpart == 8);
    assert(g_HitReactionTable[8].deathAnimCount > 0);
    assert(g_HitReactionTable[8].flinchAnimCount > 0);
#else
    vtx_arena = NULL;
#endif
    player_prop = ge_original_prop_state_allocate_player(&harness.props);
    assert(player_prop != NULL);
    player_prop->type = PROP_TYPE_PLAYER;
    player_prop->stan = (StandTile *)harness.native.spawn_tile;
    player_prop->pos.x = 0.0f;
    player_prop->pos.y = 0.0f;
    player_prop->pos.z = 0.0f;
    player.prop = player_prop;
    memset(&providers, 0, sizeof(providers));
    providers.context = &harness;
    providers.load_setup = load_setup;
    providers.get_room_scale_reciprocal = room_scale_reciprocal;
    providers.resolve_stan = resolve_stan;
    ge_original_setup_pad_bind(&providers, &pad_state);
    ge_original_setup_pad_load(LEVELID_DAM);

    ge_original_dam_guards_reset();
    assert(ge_original_dam_guards_construct_initial()
           == GE_ORIGINAL_DAM_GUARD_OK);
    assert(ge_original_dam_guards_update_matrices(identity.m)
           == GE_ORIGINAL_DAM_GUARD_OK);
    guard = ge_original_dam_guard_chr(0U);
    assert(guard != NULL && guard->model != NULL);
    bbox_node = find_bbox(guard->model->obj->RootNode);
    assert(bbox_node != NULL);
    bbox = &bbox_node->Data->BoundingBox;
    bbox_mtx = modelFindNodeMtx(guard->model, bbox_node, 0);
    assert(bbox_mtx != NULL);
    center.x = 0.5f * (bbox->Bounds.xmin + bbox->Bounds.xmax);
    center.y = 0.5f * (bbox->Bounds.ymin + bbox->Bounds.ymax);
    center.z = 0.5f * (bbox->Bounds.zmin + bbox->Bounds.zmax);
    mtx4TransformVecInPlace(bbox_mtx, &center);
    length = sqrtf(center.x * center.x + center.y * center.y
                   + center.z * center.z);
    assert(length > 0.0f);
    direction[0] = center.x / length;
    direction[1] = center.y / length;
    direction[2] = center.z / length;

    ge_original_guard_bullet_hit_reset();
    ge_original_guard_bullet_hit_observe_guard(0);
    assert(ge_original_guard_bullet_hit_test(origin, direction, direction,
               ITEM_WPPKSIL, 4294967296.0f)
           == GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED);
    ge_original_guard_bullet_hit_snapshot(&stats);
    assert(stats.pool_resets == 1U);
    assert(stats.model_lists_built == 4U);
    assert(stats.rays_tested == 1U);
    assert(stats.guard_candidates == 4U);
    assert(stats.valid_room_lists == 4U);
    assert(stats.onscreen_gate_passes == 4U);
    assert(stats.depth_gate_passes >= 1U
           && stats.depth_gate_passes <= stats.onscreen_gate_passes);
    assert(stats.sphere_gate_passes >= 1U);
    assert(stats.observed_guard_index == 0);
    assert(stats.observed_guard_samples == 1U);
    assert(stats.observed_guard_onscreen_samples == 1U);
    assert(stats.observed_prop_rooms[0]
           == ((PropRecord *)ge_original_dam_guard_prop(0U))->rooms[0]);
    assert(stats.observed_chrai_rooms[0] == -1);
    assert(stats.registered_hits == 1U);
    assert(stats.bounding_sphere_hits >= 1U);
    assert(stats.last_guard_index >= 0 && stats.last_guard_index < 4);
    assert(stats.last_hitpart > 0);
    assert(isfinite(stats.last_distance));
    {
        /* A live campaign binding must consume the hit lists already owned by
         * the preceding canonical chrTick/render pass.  Rebinding must not
         * clear those lists, and shot dispatch must not eagerly allocate one
         * list for every authored guard. */
        const uint32_t model_lists_before = stats.model_lists_built;
        const uint32_t rays_before = stats.rays_tested;
        ge_original_guard_bullet_hit_bind_stage_guards(
            &harness, stage_guard_count, stage_guard_actor);
        assert(ge_original_guard_bullet_hit_test(origin, direction, direction,
                   ITEM_WPPKSIL, 4294967296.0f)
               == GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED);
        ge_original_guard_bullet_hit_snapshot(&stats);
        assert(stats.rays_tested == rays_before + 1U);
        assert(stats.model_lists_built == model_lists_before);
        ge_original_guard_bullet_hit_bind_stage_guards(NULL, NULL, NULL);
    }
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
    guard = ge_original_dam_guard_chr((size_t)stats.last_guard_index);
    assert(guard != NULL);
    {
        const float damage_before = guard->damage;
        const size_t deformed_before =
            count_deformed_collision_nodes(guard->model,
                guard->model->obj->RootNode);
        guard->maxdamage = 100.0f;
        assert(ge_original_guard_bullet_hit_apply_pending()
               == GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
        ge_original_guard_bullet_hit_snapshot(&stats);
        assert(stats.damage_attempts == 1U);
        assert(stats.damage_applied == 1U);
        assert(guard->damage > damage_before);
        assert((guard->chrflags & CHRFLAG_WAS_HIT) != 0U);
        assert((guard->chrflags & CHRFLAG_WAS_DAMAGED) != 0U);
        assert(guard->actiontype == ACT_ARGH);
        assert(guard->model->anim != NULL);
        assert((uintptr_t)guard->model->anim
               >= (uintptr_t)ptr_animation_table);
        assert(count_deformed_collision_nodes(guard->model,
                   guard->model->obj->RootNode) > deformed_before);
    }
    {
        GeOriginalPp7FireStats fire_stats;
        GeOriginalPp7FireStatus fire_status;
        const s32 kill_count_before = permissions.kill_count;
        const s32 life_kills_before = player.kills_this_life;
        guard->maxdamage = guard->damage + 0.01f;
        matrix_4x4_set_basis_and_position_target(&identity,
            0.0f, 0.0f, 0.0f, center.x, center.y, center.z,
            0.0f, 1.0f, 0.0f);
        matrix_4x4_set_lookat(&world_to_view,
            0.0f, 0.0f, 0.0f, center.x, center.y, center.z,
            0.0f, 1.0f, 0.0f);
        assert(ge_original_dam_guards_update_matrices(world_to_view.m)
               == GE_ORIGINAL_DAM_GUARD_OK);
        player.c_screenleft = 0.0f;
        player.c_screentop = 0.0f;
        player.c_screenwidth = 320.0f;
        player.c_screenheight = 240.0f;
        player.c_halfwidth = 160.0f;
        player.c_halfheight = 120.0f;
        player.c_scalex = 0.00001f;
        player.c_scaley = 0.00001f;
        player.c_perspaspect = 4.0f / 3.0f;
        player.crosshair_angle.x = 160.0f;
        player.crosshair_angle.y = 120.0f;
        player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
        player.hands[GUNRIGHT].weapon_firing_status = 1;
        player.hands[GUNRIGHT].volley = 1;
        player.hands[GUNLEFT].weaponnum = ITEM_UNARMED;
        player.hands[GUNLEFT].weapon_firing_status = 0;
        ge_original_pp7_fire_reset();
        fire_status = ge_original_pp7_fire_tick();
        ge_original_pp7_fire_snapshot(&fire_stats);
        assert(fire_status == GE_ORIGINAL_PP7_FIRE_GUARD_DAMAGE_APPLIED);
        assert(fire_stats.guard_hits_registered == 1U);
        assert(fire_stats.guard_damage_applied == 1U);
        assert(fire_stats.guard_damage_frontiers == 0U);
        assert(guard->actiontype == ACT_DIE);
        assert(guard->model->anim != NULL);
        assert((uintptr_t)guard->model->anim
               >= (uintptr_t)ptr_animation_table);
        assert(permissions.kill_count == kill_count_before + 1);
        assert(player.kills_this_life == life_kills_before + 1);
#ifdef GE_TEST_GUARD_DEATH_TICK
        {
            ModelAnimation *death_animation = guard->model->anim;
            float previous_frame = guard->model->animframe1;
            int32_t previous_handle = 0;
            unsigned distinct_streamed_frames = 0U;
            unsigned die_ticks = 0U;
            unsigned dead_ticks = 0U;
            uint8_t fade_before;

            assert(ge_port_guard_animation_owns(death_animation));
            g_ClockTimer = 4;

            /* This is chrTick's canonical order for an ordinary ACT_DIE:
             * run the unchanged action handler, then advance its unchanged
             * model clock.  There is deliberately no port-side switch or
             * replacement death dispatcher in this harness. */
            while (guard->actiontype == ACT_DIE && die_ticks < 1024U) {
                int32_t handle = ge_port_guard_animation_load_frame(
                    guard->model->anim, guard->model->framea);
                assert(handle < 0);
                assert(ge_port_guard_animation_frame_data(handle) != NULL);
                if (handle != previous_handle) {
                    previous_handle = handle;
                    distinct_streamed_frames++;
                }
                ge_original_dam_guard_tick_die_exact(guard);
                if (guard->actiontype == ACT_DIE) {
                    modelTickAnim(guard->model, g_ClockTimer, 1);
                    assert(guard->model->animframe1 >= previous_frame);
                    previous_frame = guard->model->animframe1;
                }
                die_ticks++;
            }
            assert(die_ticks > 1U && die_ticks < 1024U);
            assert(distinct_streamed_frames > 2U);
            assert(guard->actiontype == ACT_DEAD);
            assert(guard->act_dead.allowfade < 0);

            ge_original_dam_guard_tick_dead_exact(guard);
            assert(guard->act_init.padding[0] == 0);
            fade_before = guard->fadealpha;
            while ((guard->hidden & CHRHIDDEN_REMOVE) == 0U
                    && dead_ticks < 1024U) {
                ge_original_dam_guard_tick_dead_exact(guard);
                dead_ticks++;
            }
            assert(dead_ticks > 1U && dead_ticks < 1024U);
            assert((guard->hidden & CHRHIDDEN_REMOVE) != 0U);
            assert(guard->fadealpha <= fade_before);
            printf("canonical lethal PP7 ACT_DIE -> ACT_DEAD/remove: "
                   "%u die ticks, %u streamed frames, %u dead ticks\n",
                   die_ticks, distinct_streamed_frames, dead_ticks);
        }
#endif
    }

    {
        /* Canonical removal leaves the ChrRecord slot in place, nulls its
         * model, delists/disables the prop, and returns that PropRecord to the
         * shared pool. Reuse the exact address as a non-character object to
         * prove PP7 traversal neither dereferences it nor lets one dead slot
         * suppress hits against the remaining authored guards. */
        size_t removed_index = 0U;
        size_t target_index = 0U;
        ChrRecord *removed_chr;
        PropRecord *removed_prop;
        Model *removed_model;
        ChrRecord *target_chr;
        PropRecord *reused_prop;
        ObjectRecord reused_object;
        float removed_direction[3];
        float target_direction[3];

        while (removed_index < ge_original_dam_guards_count()
                && ((ChrRecord *)ge_original_dam_guard_chr(removed_index))
                    ->actiontype
                    == ACT_DEAD)
            removed_index++;
        assert(removed_index < ge_original_dam_guards_count());
        while (target_index < ge_original_dam_guards_count()
                && (target_index == removed_index
                    || ((ChrRecord *)ge_original_dam_guard_chr(target_index))
                        ->actiontype
                        == ACT_DEAD))
            target_index++;
        assert(target_index < ge_original_dam_guards_count());
        removed_chr = ge_original_dam_guard_chr(removed_index);
        removed_prop = ge_original_dam_guard_prop(removed_index);
        target_chr = ge_original_dam_guard_chr(target_index);

        assert(removed_chr != NULL && removed_prop != NULL
               && removed_chr->model != NULL);
        assert(target_chr != NULL && target_chr->model != NULL);
        direction_to_guard(removed_chr, removed_direction);
        ge_original_guard_bullet_hit_reset();
        assert(ge_original_guard_bullet_hit_test(
                   origin, removed_direction, removed_direction,
                   ITEM_WPPKSIL, 4294967296.0f)
               == GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED);
        ge_original_guard_bullet_hit_snapshot(&stats);
        assert(stats.last_guard_index == (int32_t)removed_index);
        removed_model = removed_chr->model;
        chrpropDeregisterRooms(removed_prop);
        /* The focused damage binary does not retain the public delist/disable
         * bodies. Apply their exact list/flag postconditions in the fixture;
         * production removal still runs unchanged propsTick. */
        if (removed_prop->prev != NULL)
            removed_prop->prev->next = removed_prop->next;
        else
            g_ActivePropsHead = removed_prop->next;
        if (removed_prop->next != NULL)
            removed_prop->next->prev = removed_prop->prev;
        else
            g_ActivePropsTail = removed_prop->prev;
        removed_prop->prev = NULL;
        removed_prop->next = NULL;
        removed_prop->flags &= (u8)~PROPFLAG_ENABLED;
        removed_model->obj = NULL;
        removed_chr->model = NULL;
        removed_chr->chrnum = -1;
        chrpropFree(removed_prop);
        assert(!ge_original_dam_guard_is_live(removed_index));
        assert(ge_original_dam_guards_live_count() == 3U);

        reused_prop = chrpropAllocate();
        assert(reused_prop == removed_prop);
        memset(&reused_object, 0, sizeof(reused_object));
        reused_object.prop = reused_prop;
        reused_prop->type = PROP_TYPE_OBJ;
        reused_prop->obj = &reused_object;
        reused_prop->pos.x = 100000.0f;
        reused_prop->pos.y = 100000.0f;
        reused_prop->pos.z = 100000.0f;
        chrpropActivate(reused_prop);
        chrpropEnable(reused_prop);
        assert(ge_original_guard_bullet_hit_apply_pending()
               == GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER);

        direction_to_guard(target_chr, target_direction);
        ge_original_guard_bullet_hit_reset();
        assert(ge_original_guard_bullet_hit_test(
                   origin, target_direction, target_direction,
                   ITEM_WPPKSIL, 4294967296.0f)
               == GE_ORIGINAL_GUARD_BULLET_HIT_REGISTERED);
        ge_original_guard_bullet_hit_snapshot(&stats);
        assert(stats.model_lists_built == 3U);
        assert(stats.last_guard_index >= 0);
        assert((size_t)stats.last_guard_index != removed_index);
        assert(ge_original_dam_guard_is_live(
                   (size_t)stats.last_guard_index));
    }
    puts("original live PP7 guard damage and blood deformation applied");
#else
    puts("original guard bullet ModelHitEntry/BBOX/ShotData hit registered");
#endif

    free(native_bytes);
    free(collision_bytes);
    free(vtx_arena);
#ifdef GE_TEST_GUARD_DAMAGE_CONSEQUENCE
    ge_original_guard_animation_table_reset();
#ifdef GE_TEST_GUARD_DEATH_TICK
    free(animation_entry_bytes);
#endif
    free(animation_bytes);
#endif
    return 0;
}
