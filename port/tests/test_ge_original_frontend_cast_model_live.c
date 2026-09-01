#include "ge_original_frontend_cast.h"
#include "ge_original_frontend_cast_model.h"
#include "ge_3ds_original_frontend_cast.h"
#include "ge_original_player_gait_internal.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
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

void instcalcmatrices(ModelRenderData *render_data, Model *model)
{
    ge_port_player_gait_instcalcmatrices(render_data, model);
}

typedef struct Harness {
    uint32_t random_state;
} Harness;

static uint32_t random_next(void *context)
{
    Harness *harness = context;
    uint32_t x = harness->random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    harness->random_state = x != 0U ? x : UINT32_C(0x9e3779b9);
    return harness->random_state;
}

static int choose_random_head(void *context, int32_t body, int32_t *head)
{
    (void)context;
    if (head == NULL) return 0;
    switch (body) {
    case BODY_Scientist_2_Female:
    case BODY_Civilian_1_Female:
    case BODY_Moonraker_Elite_2_Female:
        *head = HEAD_Female_Sally;
        break;
    default:
        *head = HEAD_Male_Karl;
        break;
    }
    return 1;
}

static int unlocked(void *context)
{
    (void)context;
    return 1;
}

static void audit_3ds_projection_orientation(void)
{
    const float below[3] = {0.0f, -20.0f, -100.0f};
    const float centre[3] = {0.0f, 0.0f, -100.0f};
    const float above[3] = {0.0f, 20.0f, -100.0f};
    const float left[3] = {-20.0f, 0.0f, -100.0f};
    const float right[3] = {20.0f, 0.0f, -100.0f};
    const float near_point[3] = {0.0f, 0.0f, -20.0f};
    float projected_below[3];
    float projected_centre[3];
    float projected_above[3];
    float projected_left[3];
    float projected_right[3];
    float projected_near[3];
    assert(ge_3ds_original_frontend_cast_project(
        below, projected_below));
    assert(ge_3ds_original_frontend_cast_project(
        centre, projected_centre));
    assert(ge_3ds_original_frontend_cast_project(
        above, projected_above));
    assert(ge_3ds_original_frontend_cast_project(
        left, projected_left));
    assert(ge_3ds_original_frontend_cast_project(
        right, projected_right));
    assert(ge_3ds_original_frontend_cast_project(
        near_point, projected_near));
    /* PICA's screen-space Y is bottom-up: camera-up must remain visually up,
     * not place the actor's head below its feet. */
    assert(projected_above[1] > projected_centre[1]);
    assert(projected_centre[1] > projected_below[1]);
    assert(projected_left[0] < projected_centre[0]);
    assert(projected_centre[0] < projected_right[0]);
    assert(projected_near[2] > projected_centre[2]);
    {
        const float authored_feet[3] = {0.0f, -90.0f, -70.0f};
        const float authored_head[3] = {0.0f, 90.0f, -70.0f};
        float projected_feet[3];
        float projected_head[3];
        assert(ge_3ds_original_frontend_cast_project(
            authored_feet, projected_feet));
        assert(ge_3ds_original_frontend_cast_project(
            authored_head, projected_head));
        /* front.c deliberately permits a 70-unit camera distance with a
         * 46-degree FOV and 0.1-scale actors. A character-height segment at
         * that authored nearest distance is much taller than the 240-pixel
         * viewport: cast shots are close-ups, not full-body framing. */
        assert(projected_head[1] - projected_feet[1] > 240.0f);
    }
    assert(!ge_3ds_original_frontend_cast_project(
        (const float[3]){0.0f, 0.0f, 1.0f}, projected_centre));
    {
        Ge3dsOriginalFrontendCastClipVertex input[3] = {
            {{-2.0f, -2.0f, -20.0f}, {0.0f, 0.0f}, {1, 0, 0, 1}},
            {{ 2.0f, -2.0f, -20.0f}, {1.0f, 0.0f}, {0, 1, 0, 1}},
            {{ 0.0f,  2.0f,  -5.0f}, {0.5f, 1.0f}, {0, 0, 1, 1}},
        };
        Ge3dsOriginalFrontendCastProjectedVertex output[6];
        size_t count = ge_3ds_original_frontend_cast_clip_project_triangle(
            input, output);
        size_t index;
        /* One vertex behind near=10 retains a quad, emitted as two triangles. */
        assert(count == 6U);
        for (index = 0U; index < count; ++index) {
            assert(isfinite(output[index].projected[0]));
            assert(isfinite(output[index].projected[1]));
            assert(output[index].projected[2] >= 0.001f
                && output[index].projected[2] <= 0.999f);
            assert(output[index].texture[0] >= 0.0f
                && output[index].texture[0] <= 1.0f);
            assert(output[index].texture[1] >= 0.0f
                && output[index].texture[1] <= 1.0f);
        }
        input[0].camera_space[2] = -5.0f;
        input[1].camera_space[2] = -5.0f;
        input[2].camera_space[2] = -20.0f;
        /* Two vertices behind retain the clipped triangle. */
        assert(ge_3ds_original_frontend_cast_clip_project_triangle(
            input, output) == 3U);
        input[2].camera_space[2] = -5.0f;
        assert(ge_3ds_original_frontend_cast_clip_project_triangle(
            input, output) == 0U);
    }
}

static void audit_real_scene_near_clipping(
    const GeOriginalFrontendCastModelScene *scene,
    size_t *one_behind, size_t *two_behind,
    float *maximum_projected_magnitude)
{
    size_t batch_index;
    for (batch_index = 0U; batch_index < scene->batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        size_t first;
        assert(batch->vertex_count % 3U == 0U);
        for (first = batch->first_vertex;
                first < batch->first_vertex + batch->vertex_count;
                first += 3U) {
            Ge3dsOriginalFrontendCastClipVertex input[3];
            Ge3dsOriginalFrontendCastProjectedVertex output[6];
            size_t behind = 0U;
            size_t output_count;
            size_t corner;
            for (corner = 0U; corner < 3U; ++corner) {
                const GeDamRoomWorldVertex *source =
                    &scene->vertices[first + corner];
                size_t channel;
                memcpy(input[corner].camera_space, source->world,
                    sizeof(input[corner].camera_space));
                memcpy(input[corner].texture, source->processed.texture,
                    sizeof(input[corner].texture));
                for (channel = 0U; channel < 4U; ++channel)
                    input[corner].rgba[channel] =
                        (float)source->processed.rgba[channel] / 255.0f;
                behind += source->world[2] > -10.0f;
            }
            output_count =
                ge_3ds_original_frontend_cast_clip_project_triangle(
                    input, output);
            assert(output_count == (behind == 0U ? 3U
                : behind == 1U ? 6U : behind == 2U ? 3U : 0U));
            *one_behind += behind == 1U;
            *two_behind += behind == 2U;
            for (corner = 0U; corner < output_count; ++corner) {
                size_t axis;
                for (axis = 0U; axis < 3U; ++axis) {
                    const float magnitude =
                        fabsf(output[corner].projected[axis]);
                    assert(isfinite(magnitude));
                    if (magnitude > *maximum_projected_magnitude)
                        *maximum_projected_magnitude = magnitude;
                }
            }
        }
    }
}

static void audit_scene(const GeOriginalFrontendCastModelScene *scene,
                        const GeOriginalFrontendCastSelection *selection)
{
    size_t index;
    assert(scene != NULL && selection != NULL);
    assert(scene->vertices != NULL && scene->batches != NULL
        && scene->batch_model_types != NULL);
    assert(scene->vertex_count > 0U && scene->batch_count > 0U
        && scene->triangle_count > 0U);
    assert(scene->part_count == scene->character_part_count
        + scene->weapon_part_count);
    assert(scene->allocated_part_capacity >= scene->part_count);
    assert(scene->selection.body == selection->body
        && scene->selection.head == selection->head
        && scene->selection.weapon_prop == selection->weapon_prop
        && scene->selection.animation_record_offset
            == selection->animation_record_offset);
    assert(scene->weapon_attached == (selection->weapon_prop >= 0));
    if (selection->weapon_prop >= 0) {
        assert(scene->weapon_part_count > 0U);
        assert(scene->weapon_attachment_switch
            == (selection->animation_flip ? 5U : 3U));
        assert(scene->weapon_left_hand_rotation
            == (selection->animation_flip != 0U));
    } else {
        assert(scene->weapon_part_count == 0U);
    }
    assert(scene->render_prop_type == (uint8_t)PROP_TYPE_EXPLOSION);
    assert(scene->render_zbuffer_enabled != 0U);
    assert(scene->render_cull_mode == 3U && scene->render_flags == 3U);
    assert(scene->render_lighting_enabled != 0U
        && scene->render_texture_gen_enabled != 0U);
    assert(scene->reflection_camera_eye_z == 4000.0f);
    assert(isfinite(scene->fade));
    for (index = 0U; index < 3U; ++index) {
        assert(isfinite(scene->camera_eye[index]));
        assert(isfinite(scene->camera_target[index]));
        assert(isfinite(scene->root_position[index]));
        assert(isfinite(scene->transformed_target[index]));
    }
    for (index = 0U; index < scene->vertex_count; ++index) {
        assert(isfinite(scene->vertices[index].world[0]));
        assert(isfinite(scene->vertices[index].world[1]));
        assert(isfinite(scene->vertices[index].world[2]));
    }
}

static void reset_pose(GeOriginalFrontendCast *cast,
                       const GeOriginalFrontendCastSelection *selection)
{
    cast->selection = *selection;
    cast->timer = 0U;
    memset(cast->root_position_smoothed, 0,
           sizeof(cast->root_position_smoothed));
    memset(cast->root_velocity_accumulator, 0,
           sizeof(cast->root_velocity_accumulator));
    memset(cast->target_smoothed, 0, sizeof(cast->target_smoothed));
    memset(cast->target_accumulator, 0, sizeof(cast->target_accumulator));
    cast->camera_reset = 1U;
    cast->pose_applied = 0U;
    cast->initialized = 1U;
}

int main(int argc, char **argv)
{
    static const uint32_t animation_offsets[] = {
        UINT32_C(0x5d10), UINT32_C(0x6254), UINT32_C(0x637c),
        UINT32_C(0x6808), UINT32_C(0x6d50), UINT32_C(0x777c),
        UINT32_C(0x7aa8), UINT32_C(0x7c4c), UINT32_C(0x7d04),
        UINT32_C(0x7f0c), UINT32_C(0x7fb4), UINT32_C(0xb174),
        UINT32_C(0xc410), UINT32_C(0x6644), UINT32_C(0x6a18),
        UINT32_C(0x7304), UINT32_C(0x78c8), UINT32_C(0xa94c),
        UINT32_C(0xa9dc), UINT32_C(0xacac), UINT32_C(0xbf80),
        UINT32_C(0xbc40),
    };
    static const int32_t weapon_models[] = {
        PROP_CHRKALASH, PROP_CHRM16, PROP_CHRFNP90, PROP_CHRAUTOSHOT,
        PROP_CHRGRENADELAUNCH, PROP_CHRSNIPERRIFLE, PROP_CHRWPPK,
        PROP_CHRWPPKSIL, PROP_CHRSKORPION, PROP_CHRUZI, PROP_CHRTT33,
        PROP_CHRRUGER, PROP_CHRLASER, PROP_CHRGOLDEN,
    };
    GeAssetPack pack;
    GeOriginalFrontendCastModel *owner;
    GeOriginalFrontendCastModelStatus status;
    GeOriginalFrontendCastServices services;
    GeOriginalFrontendCast cast;
    GeOriginalFrontendCastFrame frame;
    GeOriginalFrontendCastModelScene scene;
    GeOriginalFrontendCastEvent event;
    GeOriginalFrontendCastSelection selection;
    Harness harness = { UINT32_C(0x007f5a31) };
    size_t actor_count = 0U;
    size_t armed_actor_count = 0U;
    size_t animation_index;
    size_t weapon_index;
    size_t maximum_vertices = 0U;
    size_t maximum_batches = 0U;
    uint32_t scheduler_tick;
    int saw_positive_z_camera = 0;
    int saw_negative_z_camera = 0;
    int audited_valentin_closeup = 0;
    size_t one_behind_triangles = 0U;
    size_t two_behind_triangles = 0U;
    float maximum_projected_magnitude = 0.0f;
    int complete = 0;

    assert(argc == 2);
    audit_3ds_projection_orientation();
    memset(&harness_player, 0, sizeof(harness_player));
    harness_player.c_lodscalez = 1.0f;
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    owner = ge_original_frontend_cast_model_create(&pack, &status);
    if (owner == NULL) {
        fprintf(stderr, "cast owner create: %s\n",
            ge_original_frontend_cast_model_status_name(status));
        return 2;
    }
    memset(&services, 0, sizeof(services));
    services.context = &harness;
    services.random_next = random_next;
    services.choose_random_head = choose_random_head;
    services.cradle_complete = unlocked;
    services.aztec_secret_or_00_complete = unlocked;
    services.egypt_00_complete = unlocked;
    assert(ge_original_frontend_cast_reset(&cast, &services, 1)
        == GE_ORIGINAL_FRONTEND_CAST_OK);

    while (!complete) {
        assert(ge_original_frontend_cast_snapshot(&cast, &frame)
            == GE_ORIGINAL_FRONTEND_CAST_OK);
        {
            const float horizontal = sqrtf(
                frame.camera_eye[0] * frame.camera_eye[0]
                + frame.camera_eye[2] * frame.camera_eye[2]);
            const float side_factor = sqrtf(1.04f);
            assert(horizontal >= 70.0f * side_factor - 0.001f);
            assert(horizontal <= 150.0f * side_factor + 0.001f);
            assert(frame.camera_eye[1] >= -100.0f
                && frame.camera_eye[1] <= 100.0f);
            saw_positive_z_camera |= frame.camera_eye[2] > 0.0f;
            saw_negative_z_camera |= frame.camera_eye[2] < 0.0f;
        }
        status = ge_original_frontend_cast_model_begin_selection(
            owner, &frame.selection);
        if (status != GE_ORIGINAL_FRONTEND_CAST_MODEL_OK) {
            fprintf(stderr, "cast actor %d body/head/weapon %d/%d/%d: %s\n",
                frame.selection.character_index, frame.selection.body,
                frame.selection.head, frame.selection.weapon_prop,
                ge_original_frontend_cast_model_status_name(status));
            return 3;
        }
        memset(&scene, 0, sizeof(scene));
        assert(ge_original_frontend_cast_model_tick(
            owner, &cast, 1U, 1.0f, &scene)
            == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        audit_scene(&scene, &frame.selection);
        audit_real_scene_near_clipping(&scene,
            &one_behind_triangles, &two_behind_triangles,
            &maximum_projected_magnitude);
        if (frame.selection.character_index == 3) {
            float min_x = INFINITY, max_x = -INFINITY;
            float min_y = INFINITY, max_y = -INFINITY;
            size_t vertex;
            size_t before_near = 0U;
            for (vertex = 0U; vertex < scene.vertex_count; ++vertex) {
                float projected[3];
                const float camera_space[3] = {
                    scene.vertices[vertex].world[0],
                    scene.vertices[vertex].world[1],
                    scene.vertices[vertex].world[2],
                };
                const float depth = -camera_space[2];
                if (depth < 10.0f) ++before_near;
                if (!ge_3ds_original_frontend_cast_project(
                        camera_space, projected)) continue;
                if (projected[0] < min_x) min_x = projected[0];
                if (projected[0] > max_x) max_x = projected[0];
                if (projected[1] < min_y) min_y = projected[1];
                if (projected[1] > max_y) max_y = projected[1];
            }
            /* Deterministic Valentin coverage proves this close-up is not a
             * missing near-plane conversion: all vertices remain beyond the
             * exact near=10 plane, while the authored camera still frames a
             * greater-than-screen-height model. */
            assert(before_near == 0U);
            assert(isfinite(min_x) && isfinite(max_x));
            assert(max_y - min_y > 240.0f);
            audited_valentin_closeup = 1;
        }
        if (scene.vertex_count > maximum_vertices)
            maximum_vertices = scene.vertex_count;
        if (scene.batch_count > maximum_batches)
            maximum_batches = scene.batch_count;
        assert(scene.animation_ticks == 1U);
        assert(ge_original_frontend_cast_model_tick(
            owner, &cast, 1U, 1.0f, &scene)
            == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        audit_scene(&scene, &frame.selection);
        assert(scene.animation_ticks == 2U && scene.fade == 0.0f);
        ++actor_count;
        armed_actor_count += frame.selection.weapon_prop >= 0;
        event = GE_ORIGINAL_FRONTEND_CAST_EVENT_NONE;
        for (scheduler_tick = 0U; scheduler_tick < 181U; ++scheduler_tick)
            assert(ge_original_frontend_cast_tick(&cast, 0, &event)
                == GE_ORIGINAL_FRONTEND_CAST_OK);
        if (event == GE_ORIGINAL_FRONTEND_CAST_EVENT_MISSION_SELECT) {
            complete = 1;
        } else {
            assert(event == GE_ORIGINAL_FRONTEND_CAST_EVENT_RELOAD);
            assert(ge_original_frontend_cast_begin_current(&cast)
                == GE_ORIGINAL_FRONTEND_CAST_OK);
        }
    }
    assert(actor_count == 34U && armed_actor_count > 0U);
    /* The start angle is authored across a full turn. Both camera hemispheres
     * therefore occur; a rear/side character view must not be normalized into
     * a replacement front-facing shot by the port. */
    assert(saw_positive_z_camera && saw_negative_z_camera);
    assert(audited_valentin_closeup);
    assert(one_behind_triangles > 0U && two_behind_triangles > 0U);
    /* Exact near clipping removes the unbounded divide by sub-near depths;
     * side-frustum clipping remains PICA's responsibility. */
    assert(maximum_projected_magnitude < 10000.0f);

    /* Prove every animation record and every authored cast weapon through
     * the same native owner, including both attachment switches. */
    assert(ge_original_frontend_cast_reset(&cast, &services, 0)
        == GE_ORIGINAL_FRONTEND_CAST_OK);
    for (animation_index = 0U;
         animation_index < sizeof(animation_offsets)/sizeof(animation_offsets[0]);
         ++animation_index) {
        selection = cast.selection;
        selection.body = BODY_Special_Operations_Uniform;
        selection.head = HEAD_Male_Brosnan_Boiler;
        selection.weapon_prop = animation_index < 2U
            ? -1 : weapon_models[animation_index
                % (sizeof(weapon_models)/sizeof(weapon_models[0]))];
        selection.animation_record_offset = animation_offsets[animation_index];
        selection.animation_start_frame = 0.0f;
        selection.animation_playback_speed = 1.0f;
        selection.animation_flip = (uint8_t)(animation_index & 1U);
        reset_pose(&cast, &selection);
        assert(ge_original_frontend_cast_model_begin_selection(
            owner, &selection) == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        assert(ge_original_frontend_cast_model_tick(
            owner, &cast, 1U, 1.0f, &scene)
            == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        audit_scene(&scene, &selection);
    }
    for (weapon_index = 0U;
         weapon_index < sizeof(weapon_models)/sizeof(weapon_models[0]);
         ++weapon_index) {
        selection = cast.selection;
        selection.body = BODY_Special_Operations_Uniform;
        selection.head = HEAD_Male_Brosnan_Boiler;
        selection.weapon_prop = weapon_models[weapon_index];
        selection.animation_record_offset = UINT32_C(0x6254);
        selection.animation_start_frame = 21.0f;
        selection.animation_playback_speed = 1.0f;
        selection.animation_flip = (uint8_t)(weapon_index & 1U);
        reset_pose(&cast, &selection);
        assert(ge_original_frontend_cast_model_begin_selection(
            owner, &selection) == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        assert(ge_original_frontend_cast_model_tick(
            owner, &cast, 1U, 1.0f, &scene)
            == GE_ORIGINAL_FRONTEND_CAST_MODEL_OK);
        audit_scene(&scene, &selection);
    }

    printf("frontend cast owner live sequence passed: %lu actors, "
           "%lu armed, %lu animation records, %lu weapon models, "
           "max %lu vertices/%lu batches, clipped %lu one-behind/"
           "%lu two-behind triangles (max projected %.1f)\n",
        (unsigned long)actor_count, (unsigned long)armed_actor_count,
        (unsigned long)(sizeof(animation_offsets)/sizeof(animation_offsets[0])),
        (unsigned long)(sizeof(weapon_models)/sizeof(weapon_models[0])),
        (unsigned long)maximum_vertices, (unsigned long)maximum_batches,
        (unsigned long)one_behind_triangles,
        (unsigned long)two_behind_triangles,
        maximum_projected_magnitude);
    ge_original_frontend_cast_model_destroy(owner);
    ge_asset_pack_close(&pack);
    return 0;
}
