#include "ge_original_gunbarrel.h"
#include "ge_original_gunbarrel_bond.h"
#include "ge_original_player_gait_internal.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct player harness_player;

struct player *ge_original_spawn_player_get(void)
{
    return &harness_player;
}

/* extract_player_gait_model_slice keeps the unchanged instcalcmatrices body
 * behind its native pointer-safe port name.  The production build links that
 * body under the original symbol; expose the same ABI to this host harness. */
void instcalcmatrices(ModelRenderData *render_data, Model *model)
{
    ge_port_player_gait_instcalcmatrices(render_data, model);
}

typedef struct BloodState {
    unsigned advances;
    unsigned resets;
} BloodState;

static int blood_tick(void *context, int mode)
{
    BloodState *state = context;
    if (mode == 0) {
        ++state->resets;
        state->advances = 0U;
        return 0;
    }
    ++state->advances;
    return state->advances >= 42U;
}

static uint32_t audit_scene(const GeOriginalGunbarrelBondScene *scene)
{
    size_t index;
    uint32_t model_type_mask = 0U;
    assert(scene->vertices != NULL && scene->batches != NULL
        && scene->batch_model_types != NULL);
    assert(scene->vertex_count > 0U && scene->batch_count > 0U);
    assert(scene->triangle_count > 0U);
    assert(scene->render_prop_type == 7U);
    assert(scene->render_zbuffer_enabled == 0U);
    assert(scene->render_cull_mode == 3U);
    assert(scene->render_primary_flags == 1U
        && scene->render_secondary_flags == 2U);
    assert(scene->shadow_alpha == 80U);
    assert(scene->viewer_uses_vertex_alpha_lighting != 0U);
    assert(scene->render_environment_rgba == 0U
        && scene->render_fog_rgba == 0U);
    for (index = 0U; index < scene->batch_count; ++index) {
        assert(scene->batch_model_types[index] >= 0
            && scene->batch_model_types[index] <= 4);
        model_type_mask |= UINT32_C(1)
            << (uint32_t)scene->batch_model_types[index];
    }
    for (index = 0U; index < scene->vertex_count; ++index) {
        assert(isfinite(scene->vertices[index].world[0]));
        assert(isfinite(scene->vertices[index].world[1]));
        assert(isfinite(scene->vertices[index].world[2]));
    }
    return model_type_mask;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalGunbarrelBondStatus bond_status;
    GeOriginalGunbarrelBond *bond;
    GeOriginalGunbarrelState timeline;
    GeOriginalGunbarrelFrame frame;
    GeOriginalGunbarrelBondScene scene;
    BloodState blood = {0U, 0U};
    GeOriginalGunbarrelTickResult tick_result;
    size_t rendered_bond_frames = 0U;
    size_t fire_frames = 0U;
    size_t start_frames = 0U;
    size_t speedup_frames = 0U;
    size_t pre_fire_vertices = 0U;
    size_t pre_fire_batches = 0U;
    size_t pre_fire_parts = 0U;
    size_t fire_vertices = 0U;
    size_t fire_batches = 0U;
    size_t fire_parts = 0U;
    size_t post_fire_vertices = 0U;
    size_t post_fire_batches = 0U;
    size_t post_fire_parts = 0U;
    size_t initial_part_capacity = 0U;
    size_t initial_character_parts = 0U;
    size_t initial_gun_parts = 0U;
    size_t maximum_part_count = 0U;
    size_t maximum_character_parts = 0U;
    size_t maximum_gun_parts = 0U;
    size_t maximum_part_frame = 0U;
    uint32_t previous_timer = 0U;
    int awaiting_post_fire = 0;
    size_t host_frames;
    uint32_t model_type_mask = 0U;

    assert(argc == 2);
    memset(&harness_player, 0, sizeof(harness_player));
    harness_player.c_lodscalez = 1.0f;
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    bond = ge_original_gunbarrel_bond_create(&pack, &bond_status);
    if (bond == NULL) {
        fprintf(stderr, "gunbarrel Bond create: %s\n",
            ge_original_gunbarrel_bond_status_name(bond_status));
        return 2;
    }
    assert(ge_original_gunbarrel_bond_reset(bond)
        == GE_ORIGINAL_GUNBARREL_BOND_OK);
    ge_original_gunbarrel_reset(&timeline);

    for (host_frames = 0U; host_frames < 4096U; ++host_frames) {
        tick_result = ge_original_gunbarrel_tick(
            &timeline, blood_tick, &blood, &frame);
        assert(tick_result != GE_ORIGINAL_GUNBARREL_TICK_INVALID);
        assert(tick_result != GE_ORIGINAL_GUNBARREL_TICK_NEEDS_BLOOD_DECODER);
        if ((frame.layers & GE_ORIGINAL_GUNBARREL_LAYER_BOND) != 0U) {
            memset(&scene, 0, sizeof(scene));
            bond_status = ge_original_gunbarrel_bond_tick(
                bond, &frame, &scene);
            if (bond_status != GE_ORIGINAL_GUNBARREL_BOND_OK) {
                fprintf(stderr, "gunbarrel Bond frame %lu: %s\n",
                    (unsigned long)host_frames,
                    ge_original_gunbarrel_bond_status_name(bond_status));
                return 3;
            }
            model_type_mask |= audit_scene(&scene);
            assert(scene.part_count
                == scene.character_part_count + scene.gun_part_count);
            assert(scene.allocated_part_capacity >= scene.part_count);
            if (initial_part_capacity == 0U) {
                initial_part_capacity = scene.initial_part_capacity;
                initial_character_parts =
                    scene.initial_character_part_count;
                initial_gun_parts = scene.initial_gun_part_count;
            }
            assert(scene.initial_part_capacity == initial_part_capacity);
            if (scene.part_count > maximum_part_count) {
                maximum_part_count = scene.part_count;
                maximum_character_parts = scene.character_part_count;
                maximum_gun_parts = scene.gun_part_count;
                maximum_part_frame = host_frames;
            }
            assert(scene.animation_timer
                == previous_timer + frame.bond_animation_ticks);
            previous_timer = scene.animation_timer;
            ++rendered_bond_frames;
            start_frames += frame.animation_start != 0U;
            speedup_frames += frame.animation_speedup != 0U;
            if (frame.fire_shot != 0U) {
                assert(scene.muzzle_flash_visible != 0U);
                assert(fire_frames == 0U);
                fire_vertices = scene.vertex_count;
                fire_batches = scene.batch_count;
                fire_parts = scene.part_count;
                ++fire_frames;
                awaiting_post_fire = 1;
            } else {
                assert(scene.muzzle_flash_visible == 0U);
                if (fire_frames == 0U) {
                    pre_fire_vertices = scene.vertex_count;
                    pre_fire_batches = scene.batch_count;
                    pre_fire_parts = scene.part_count;
                } else if (awaiting_post_fire) {
                    post_fire_vertices = scene.vertex_count;
                    post_fire_batches = scene.batch_count;
                    post_fire_parts = scene.part_count;
                    awaiting_post_fire = 0;
                }
            }
        }
        if (tick_result == GE_ORIGINAL_GUNBARREL_TICK_COMPLETE) break;
    }

    assert(host_frames < 4096U);
    assert(rendered_bond_frames > 100U);
    assert(start_frames == 1U && speedup_frames == 1U && fire_frames == 1U);
    assert(pre_fire_vertices > 0U && pre_fire_batches > 0U);
    fprintf(stderr, "gunbarrel topology audit: %lu/%lu/%lu -> "
        "%lu/%lu/%lu -> %lu/%lu/%lu\n",
        (unsigned long)pre_fire_vertices, (unsigned long)pre_fire_batches,
        (unsigned long)pre_fire_parts,
        (unsigned long)fire_vertices, (unsigned long)fire_batches,
        (unsigned long)fire_parts,
        (unsigned long)post_fire_vertices, (unsigned long)post_fire_batches,
        (unsigned long)post_fire_parts);
    assert(maximum_part_count > initial_part_capacity);
    assert(maximum_character_parts > initial_character_parts);
    assert(maximum_gun_parts == initial_gun_parts);
    assert(fire_vertices >= pre_fire_vertices);
    assert(fire_batches >= pre_fire_batches);
    assert(post_fire_vertices == pre_fire_vertices);
    assert(post_fire_batches == pre_fire_batches);
    assert(post_fire_parts == pre_fire_parts);
    assert(blood.resets == 1U && blood.advances == 42U);
    /* The exact live pair requires both viewer-alpha-lighted wrapper types;
     * type 3 covers the reduced secondary gun/character material and type 4
     * covers the full primary/secondary viewer material. */
    assert((model_type_mask & (UINT32_C(1) << 3U)) != 0U);
    assert((model_type_mask & (UINT32_C(1) << 4U)) != 0U);

    fprintf(stderr, "gunbarrel allocation audit: initial=%lu "
        "(character=%lu gun=%lu), max=%lu (character=%lu gun=%lu) "
        "at frame=%lu\n",
        (unsigned long)initial_part_capacity,
        (unsigned long)initial_character_parts,
        (unsigned long)initial_gun_parts,
        (unsigned long)maximum_part_count,
        (unsigned long)maximum_character_parts,
        (unsigned long)maximum_gun_parts,
        (unsigned long)maximum_part_frame);

    printf("gunbarrel live sequence pass: %lu Bond frames; model types "
           "0x%lx; topology "
           "%lu/%lu/%lu -> %lu/%lu/%lu -> %lu/%lu/%lu\n",
        (unsigned long)rendered_bond_frames,
        (unsigned long)model_type_mask,
        (unsigned long)pre_fire_vertices, (unsigned long)pre_fire_batches,
        (unsigned long)pre_fire_parts,
        (unsigned long)fire_vertices, (unsigned long)fire_batches,
        (unsigned long)fire_parts,
        (unsigned long)post_fire_vertices, (unsigned long)post_fire_batches,
        (unsigned long)post_fire_parts);
    ge_original_gunbarrel_bond_destroy(bond);
    ge_asset_pack_close(&pack);
    return 0;
}
