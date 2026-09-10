/* Optional private-asset host regression. Route helpers follow the existing
 * ge_dam_route_capacity_probe so the same authored states and budgets apply.
 * This executes original visibility/camera source; it is not an N64 image oracle. */
#include "ge_asset_pack.h"
#include "ge_dam_renderer_visibility.h"
#include "ge_3ds_stereo.h"
#include <assert.h>
#include <math.h>
#include "ge_dam_dynamic_scene.h"
#include "ge_dam_preload_queue.h"
#include "ge_dam_visibility_runtime.h"
#include "ge_dam_world.h"
#include "ge_original_bond_camera.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef GE_ROUTE_VERTEX_CAPACITY
#define GE_ROUTE_VERTEX_CAPACITY 65536U
#endif
#ifndef GE_ROUTE_BATCH_CAPACITY
#define GE_ROUTE_BATCH_CAPACITY 65536U
#endif
#ifndef GE_ROUTE_INITIAL_ROOM_CAPACITY
#define GE_ROUTE_INITIAL_ROOM_CAPACITY 10U
#endif
#define GE_ROUTE_MAX_LABEL 127U

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

typedef struct TourView {
    uint8_t room;
    float position[3];
    float look[3];
    float up[3];
    char label[GE_ROUTE_MAX_LABEL + 1U];
} TourView;

static FileData read_file(const char *path)
{
    FileData result = {NULL, 0U};
    FILE *stream = fopen(path, "rb");
    long length;

    if (stream == NULL || fseek(stream, 0L, SEEK_END) != 0
            || (length = ftell(stream)) <= 0L
            || fseek(stream, 0L, SEEK_SET) != 0) {
        if (stream != NULL) fclose(stream);
        return result;
    }
    result.size = (size_t)length;
    result.bytes = malloc(result.size);
    if (result.bytes == NULL
            || fread(result.bytes, 1U, result.size, stream) != result.size) {
        free(result.bytes);
        result.bytes = NULL;
        result.size = 0U;
    }
    fclose(stream);
    return result;
}

static int read_tour_view(FILE *stream, TourView *view)
{
    char line[1024];

    while (fgets(line, sizeof(line), stream) != NULL) {
        unsigned long frames;
        unsigned long room;
        int parsed;

        if (line[0] == '#' || line[0] == '\n'
                || strncmp(line, "GEVIEW1", 7U) == 0) {
            continue;
        }
        parsed = sscanf(line,
            "%lu %lu %f %f %f %f %f %f %f %f %f %127s",
            &frames, &room,
            &view->position[0], &view->position[1], &view->position[2],
            &view->look[0], &view->look[1], &view->look[2],
            &view->up[0], &view->up[1], &view->up[2], view->label);
        (void)frames;
        if (parsed != 12 || room > UINT8_MAX) return -1;
        view->room = (uint8_t)room;
        return 1;
    }
    return feof(stream) ? 0 : -1;
}

static size_t count_unique_rare_textures(const GeDamDynamicScene *scene)
{
    size_t texture_count = 0U;
    size_t batch_index;

    for (batch_index = 0U; batch_index < scene->scene.batch_count;
            ++batch_index) {
        const GeDamRoomDrawBatch *batch = &scene->batches[batch_index];
        size_t prior_index;

        if (batch->texture_valid == 0U
                || batch->material.texture_enabled == 0U
                || batch->material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) {
            continue;
        }
        for (prior_index = 0U; prior_index < batch_index; ++prior_index) {
            const GeDamRoomDrawBatch *prior = &scene->batches[prior_index];

            if (prior->texture_valid != 0U
                    && prior->material.texture_enabled != 0U
                    && prior->material.texture_source
                        == GE_PICA_TEXTURE_SOURCE_RARE_ID
                    && prior->texture.texture_id
                        == batch->texture.texture_id) {
                break;
            }
        }
        if (prior_index == batch_index) ++texture_count;
    }
    return texture_count;
}

static int collect_initial_rooms(const GeDamWorld *world, uint8_t start,
                                 uint8_t rooms[GE_ROUTE_INITIAL_ROOM_CAPACITY],
                                 size_t *room_count)
{
    uint8_t seen[GE_DAM_WORLD_MAX_ROOMS] = {0};
    size_t cursor = 0U;
    size_t count = 1U;

    if (world == NULL || room_count == NULL || start >= world->room_count) {
        return 0;
    }
    rooms[0] = start;
    seen[start] = 1U;
    while (cursor < count && count < GE_ROUTE_INITIAL_ROOM_CAPACITY) {
        const uint8_t current = rooms[cursor++];
        size_t portal;

        for (portal = 0U; portal < world->portal_count
                && count < GE_ROUTE_INITIAL_ROOM_CAPACITY; ++portal) {
            const GeDamWorldPortal *entry = &world->portals[portal];
            uint8_t connected;

            if (entry->connected_room1 == current) {
                connected = entry->connected_room2;
            } else if (entry->connected_room2 == current) {
                connected = entry->connected_room1;
            } else {
                continue;
            }
            if (seen[connected] == 0U) {
                seen[connected] = 1U;
                rooms[count++] = connected;
            }
        }
    }
    *room_count = count;
    return 1;
}

static int build_camera(const GeDamWorld *world, const TourView *view,
                        GeOriginalBondCameraResult *camera)
{
    static const float level_scale = 0.23363999f;
    static const float visibility_scale = 0.2f;
    GeOriginalBondCameraConfig config;
    const GeDamWorldRoom *room = ge_dam_world_room(world, view->room);
    size_t axis;

    if (room == NULL) return 0;
    memset(&config, 0, sizeof(config));
    for (axis = 0U; axis < 3U; ++axis) {
        config.camera_position[axis] = view->position[axis];
        config.camera_look_direction[axis] = view->look[axis];
        config.camera_up[axis] = view->up[axis];
        config.room_origin[axis] = room->origin[axis] / level_scale;
    }
    config.room_position_scale = level_scale;
    config.camera_local_scale = visibility_scale;
    config.visibility_scale = visibility_scale;
    config.viewport_scale[0] = 640;
    config.viewport_scale[1] = 480;
    config.viewport_scale[2] = 511;
    config.viewport_translation[0] = 800;
    config.viewport_translation[1] = 480;
    config.viewport_translation[2] = 511;
    config.room = view->room;
    return ge_original_bond_camera_set_perspective(
               &config, 60.0f, 4.0f / 3.0f, 5.0f, 15000.0f)
                == GE_ORIGINAL_BOND_CAMERA_OK
        && ge_original_bond_camera_run(&config, camera)
                == GE_ORIGINAL_BOND_CAMERA_OK;
}

static void print_room_set(const uint8_t rooms[GE_DAM_WORLD_MAX_ROOMS])
{
    size_t room;
    int first = 1;

    putchar('[');
    for (room = 0U; room < GE_DAM_WORLD_MAX_ROOMS; ++room) {
        if (rooms[room] == 0U) continue;
        if (!first) putchar(',');
        printf("%lu", (unsigned long)room);
        first = 0;
    }
    putchar(']');
}


static size_t isolation_checks, mono_checks, oracle_checks, capacity_checks;
static size_t left_only_views, right_only_views, closed_reductions;
static uint64_t canonical_digest = UINT64_C(14695981039346656037);

static void room_snapshot(GeOriginalBgRoomVisibilitySnapshot *snapshot,
    size_t room_count)
{
    memset(snapshot, 0, sizeof(*snapshot) * GE_DAM_WORLD_MAX_ROOMS);
    for (size_t room = 0U; room < room_count; ++room)
        assert(ge_original_bg_visibility_room_snapshot((uint8_t)room,
            &snapshot[room]));
}

static void digest_bytes(const void *data, size_t size)
{
    const uint8_t *bytes = data;
    for (size_t i = 0U; i < size; ++i) {
        canonical_digest ^= bytes[i];
        canonical_digest *= UINT64_C(1099511628211);
    }
}

static void check_view(GeDamVisibilityRuntime *runtime,
    const GeOriginalBondCameraResult *camera, const TourView *view,
    GeDamPreloadQueue *queue, const uint8_t *controls, size_t portal_count,
    GeDamRendererVisibility *render)
{
    GeOriginalBgVisibilityResult canonical, reference;
    GeOriginalBgRoomVisibilitySnapshot before[GE_DAM_WORLD_MAX_ROOMS];
    GeOriginalBgRoomVisibilitySnapshot after[GE_DAM_WORLD_MAX_ROOMS];
    ge_dam_preload_queue_begin_visibility(queue);
    GeDamPreloadQueue before_canonical = *queue;
    GeOriginalBgVisibilityProviders providers = ge_dam_preload_queue_providers(
        queue, controls, portal_count);
    assert(ge_dam_visibility_runtime_run_with_providers(runtime, camera,
        view->position, &providers, &canonical) == GE_DAM_VISIBILITY_RUNTIME_OK);
    GeDamPreloadQueue after_canonical = *queue;
    room_snapshot(before, runtime->room_count);
    Ge3dsStereoConfig stereo;
    GeRendererEyeShift shifts[2], zero[2] = {{0}};
    ge_3ds_stereo_config_defaults(&stereo);
    for (size_t eye = 0U; eye < 2U; ++eye)
        ge_3ds_stereo_pixel_shift(&stereo, 1.0f, eye == 0U ? -1 : 1,
            GE_3DS_STEREO_LAYER_WORLD, &shifts[eye].constant_px,
            &shifts[eye].coefficient_px);
    assert(ge_dam_renderer_visibility_run(runtime, camera, view->position,
        &providers, queue, &canonical, zero, render) == GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(!render->stereo && memcmp(&render->visible, &canonical,
        sizeof(canonical)) == 0);
    ++mono_checks;
    GeOriginalBgVisibilityStatus status = ge_dam_renderer_visibility_run(
        runtime, camera, view->position, &providers, queue, &canonical,
        shifts, render);
    if (status != GE_ORIGINAL_BG_VISIBILITY_OK) {
        fprintf(stderr, "renderer failed at %s: %s (basis %.9g %.9g %.9g)\n",
            view->label, ge_original_bg_visibility_status_name(status),
            camera->view[0][0], camera->view[1][0], camera->view[2][0]);
        abort();
    }
    room_snapshot(after, runtime->room_count);
    assert(memcmp(before, after, sizeof(before)) == 0);
    assert(memcmp(queue, &after_canonical, sizeof(*queue)) == 0);
    ++isolation_checks;
    uint8_t expected[GE_DAM_WORLD_MAX_ROOMS] = {0};
    for (size_t i = 0U; i < canonical.room_count; ++i)
        expected[canonical.rooms[i].room] |= 1U;
    /* A separate original-interpreter execution is the authored policy oracle.
     * This deliberately happens only AFTER publication invariance was checked.
     * Its queue is a value snapshot and has no side effect on live streaming. */
    for (size_t eye = 0U; eye < 2U; ++eye) {
        GeOriginalBgVisibilityInput input;
        GeDamPreloadQueue shadow = after_canonical;
        GeOriginalBgVisibilityProviders eye_providers = ge_dam_preload_queue_providers(
            &shadow, controls, portal_count);
        assert(ge_dam_renderer_visibility_eye_input(runtime, camera,
            view->position, &shifts[eye], &input));
        input.providers = &eye_providers;
        assert(ge_original_bg_visibility_run(&input, &reference)
            == GE_ORIGINAL_BG_VISIBILITY_OK);
        for (size_t i = 0U; i < reference.room_count; ++i)
            expected[reference.rooms[i].room] |= (uint8_t)(2U << eye);
        ++oracle_checks;
    }
    assert(memcmp(expected, render->membership, sizeof(expected)) == 0);
    /* Restore the exact canonical invocation, including its entry queue state. */
    GeOriginalBgVisibilityProviders restored = ge_dam_preload_queue_providers(
        &before_canonical, controls, portal_count);
    assert(ge_dam_visibility_runtime_run_with_providers(runtime, camera,
        view->position, &restored, &reference) == GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(memcmp(&canonical, &reference, sizeof(canonical)) == 0);
    room_snapshot(after, runtime->room_count);
    assert(memcmp(before, after, sizeof(before)) == 0);
    digest_bytes(before, sizeof(before));
}

static void print_membership(const GeDamRendererVisibility *render, uint8_t bit)
{
    uint8_t rooms[GE_DAM_WORLD_MAX_ROOMS] = {0};
    for (size_t room = 0U; room < GE_DAM_WORLD_MAX_ROOMS; ++room)
        rooms[room] = (render->membership[room] & bit) != 0U;
    print_room_set(rooms);
}

int main(int argc, char **argv)
{
    if (argc < 6 || argc > 8) {
        fprintf(stderr, "usage: %s ROUTE BACKGROUND GEPACK GEVIEW TEXTURE_CAP [frame-aging|batch-aging|legacy-aging] [mono-stream]\n", argv[0]);
        return 2;
    }
    const int frame_aging = !(argc > 6 && strcmp(argv[6], "batch-aging") == 0);
    const int protect_preloads = !(argc > 6 && strcmp(argv[6], "legacy-aging") == 0);
    const int mono_stream = argc > 7 && strcmp(argv[7], "mono-stream") == 0;
    int exit_status = 0;
    const size_t texture_capacity = strtoul(argv[5], NULL, 10);
    assert(texture_capacity > 0U);
    FileData background = read_file(argv[2]);
    GeAssetPack pack;
    GeDamWorld world;
    GeDamVisibilityRuntime runtime;
    GeDamPreloadQueue queue;
    GeDamDynamicScene scene;
    uint8_t initial[GE_ROUTE_INITIAL_ROOM_CAPACITY], controls[GE_DAM_WORLD_MAX_PORTALS];
    uint8_t closed[GE_DAM_WORLD_MAX_PORTALS];
    size_t initial_count;
    const GeDamDynamicSceneLimits limits = {GE_DAM_WORLD_MAX_ROOMS,
        GE_ROUTE_VERTEX_CAPACITY, GE_ROUTE_BATCH_CAPACITY};
    assert(background.bytes && ge_dam_world_parse(background.bytes,
        background.size, &world) == GE_DAM_WORLD_OK);
    assert(ge_asset_pack_open(&pack, argv[3]) == GE_ASSET_PACK_OK);
    assert(ge_dam_visibility_runtime_load(&pack, &runtime) == GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(runtime.background_size == background.size
        && memcmp(runtime.background, background.bytes, background.size) == 0);
    assert(collect_initial_rooms(&world, 135U, initial, &initial_count));
    assert(ge_dam_preload_queue_init(&queue, world.room_count, GE_DAM_PRELOAD_MAX_ROOMS,
        initial, initial_count) == GE_DAM_PRELOAD_OK);
    assert(ge_dam_dynamic_scene_init(&scene, &pack, &world, initial,
        initial_count, &limits) == GE_DAM_DYNAMIC_SCENE_OK);
    for (size_t portal = 0U; portal < world.portal_count; ++portal) {
        controls[portal] = world.portals[portal].control_bytes1;
        closed[portal] = controls[portal] | 1U;
    }
    size_t view_count = 0U, peak_rooms = scene.room_count;
    size_t peak_vertices = scene.scene.vertex_count, peak_batches = scene.scene.batch_count;
    size_t peak_textures = count_unique_rare_textures(&scene);
    size_t streaming_installs = 0U, eye_only_installs = 0U;
    size_t hold_checks = 0U, released_preloads = 0U;
    uint8_t previously_protected[GE_DAM_WORLD_MAX_ROOMS] = {0};
    if (peak_textures > texture_capacity) {
        fprintf(stderr, "initial scene exceeds texture capacity\n");
        exit_status = 1; goto cleanup;
    }
    FILE *tour = fopen(argv[4], "r");
    assert(tour);
    printf("{\"schema\":1,\"evidence\":\"original-assets-host-visibility\",\"route\":\"%s\",\"views\":[", argv[1]);
    for (;;) {
        TourView view;
        int read_status = read_tour_view(tour, &view);
        if (read_status == 0) break;
        assert(read_status == 1);
        GeOriginalBondCameraResult camera;
        assert(build_camera(&world, &view, &camera));
        (void)ge_dam_preload_queue_request(&queue, view.room);
        GeDamRendererVisibility render;
        size_t pass;
        for (pass = 0U; pass <= world.room_count; ++pass) {
            check_view(&runtime, &camera, &view, &queue, controls, world.portal_count, &render);
            if (mono_stream) for (size_t room = 0U; room < runtime.room_count; ++room) {
                render.membership[room] &= 1U;
                render.preload[room] = 0U;
            }
            for (size_t room = 0U; room < runtime.room_count; ++room)
                if (render.preload[room]) (void)ge_dam_preload_queue_request(&queue, (uint8_t)room);
            uint8_t visible_rooms[GE_DAM_WORLD_MAX_ROOMS];
            size_t visible_count = 0U;
            for (size_t room = 0U; room < runtime.room_count; ++room)
                if (render.membership[room] || (protect_preloads
                    && (render.preload[room] || queue.visibility_requested[room])))
                    visible_rooms[visible_count++] = (uint8_t)room;
            if (pass == 0U || frame_aging) assert(ge_dam_dynamic_scene_age_visibility(&scene, visible_rooms, visible_count)
                == GE_DAM_DYNAMIC_SCENE_OK);
            if (queue.pending_count == 0U) break;
            uint8_t install_room;
            assert(ge_dam_preload_queue_peek(&queue, &install_room) == GE_DAM_PRELOAD_OK);
            if ((render.membership[install_room] & 6U)
                && !(render.membership[install_room] & 1U)) ++eye_only_installs;
            GeDamDynamicSceneStatus status = ge_dam_dynamic_scene_install_next(&scene, &queue);
            if (status != GE_DAM_DYNAMIC_SCENE_OK) {
                fprintf(stderr, "room %u install at %s failed: %s\n", install_room,
                    view.label, ge_dam_dynamic_scene_status_name(status));
                exit_status = 1; fclose(tour); goto cleanup;
            }
            ++streaming_installs;
            if (scene.room_count > peak_rooms) peak_rooms = scene.room_count;
            if (scene.scene.vertex_count > peak_vertices) peak_vertices = scene.scene.vertex_count;
            if (scene.scene.batch_count > peak_batches) peak_batches = scene.scene.batch_count;
            size_t textures = count_unique_rare_textures(&scene);
            if (textures > peak_textures) peak_textures = textures;
            if (textures > texture_capacity) {
                fprintf(stderr, "scene at %s exceeds texture capacity\n", view.label);
                exit_status = 1; fclose(tour); goto cleanup;
            }
        }
        if (pass > world.room_count) {
            fprintf(stderr, "preload closure did not converge at %s; pending=%lu evictions=%llu\n",
                view.label, (unsigned long)queue.pending_count,
                (unsigned long long)scene.rooms_evicted);
            for (size_t room = 0U; room < runtime.room_count; ++room)
                if (queue.states[room]) fprintf(stderr, " %lu:%u:%u", (unsigned long)room,
                    queue.states[room], render.membership[room]);
            fputc('\n', stderr);
            exit_status = 1; fclose(tour); goto cleanup;
        }
        for (size_t room = 0U; room < runtime.room_count; ++room) {
            if (previously_protected[room] && !scene.resident[room]) {
                ++released_preloads;
                previously_protected[room] = 0U;
            }
            if (!render.membership[room] && render.preload[room] && scene.resident[room])
                previously_protected[room] = 1U;
        }
        /* More than the original three-tick unload window at a settled camera
         * must not cycle authored preload-only rooms back into the queue. */
        if (frame_aging && protect_preloads) for (size_t hold = 0U; hold < 6U; ++hold) {
            size_t accepted = queue.accepted_count;
            check_view(&runtime, &camera, &view, &queue, controls, world.portal_count, &render);
            uint8_t protected_rooms[GE_DAM_WORLD_MAX_ROOMS];
            size_t protected_count = 0U;
            for (size_t room = 0U; room < runtime.room_count; ++room) {
                if (render.preload[room]) (void)ge_dam_preload_queue_request(&queue, (uint8_t)room);
                if (render.membership[room] || render.preload[room] || queue.visibility_requested[room])
                    protected_rooms[protected_count++] = (uint8_t)room;
            }
            assert(queue.accepted_count == accepted && queue.pending_count == 0U);
            assert(ge_dam_dynamic_scene_age_visibility(&scene, protected_rooms, protected_count)
                == GE_DAM_DYNAMIC_SCENE_OK);
            for (size_t i = 0U; i < protected_count; ++i)
                if (scene.resident[protected_rooms[i]]) {
                    assert(scene.room_age[protected_rooms[i]] == 1U);
                    assert(!scene.eviction_pending[protected_rooms[i]]);
                }
            ++hold_checks;
        }
        int left_only = 0, right_only = 0;
        for (size_t room = 0U; room < runtime.room_count; ++room) {
            if (render.membership[room] && !mono_stream) assert(queue.states[room] == GE_DAM_PRELOAD_ROOM_RESIDENT);
            left_only |= render.membership[room] == 2U;
            right_only |= render.membership[room] == 4U;
        }
        left_only_views += left_only;
        right_only_views += right_only;
        /* With all portal closed bits set, original global commands can still
         * authorize rooms. Compare exact original eye sets, not an invented
         * graph-only policy that would suppress authored global visibility. */
        GeDamPreloadQueue shadow = queue;
        GeDamRendererVisibility closed_result;
        check_view(&runtime, &camera, &view, &shadow, closed, world.portal_count, &closed_result);
        closed_reductions += closed_result.visible.room_count < render.visible.room_count;
        /* A one-entry pending budget is another real interpreter state; private
         * eye queries must leave it intact even when a request is blocked. */
        GeDamPreloadQueue limited;
        assert(ge_dam_preload_queue_init(&limited, world.room_count, 1U,
            &view.room, 1U) == GE_DAM_PRELOAD_OK);
        GeDamRendererVisibility limited_result;
        check_view(&runtime, &camera, &view, &limited, controls,
            world.portal_count, &limited_result);
        assert(limited.pending_count <= 1U);
        ++capacity_checks;
        if (view_count++) putchar(',');
        printf("{\"label\":\"%s\",\"camera_room\":%u,\"canonical\":", view.label, view.room);
        print_membership(&render, 1U);
        printf(",\"left\":"); print_membership(&render, 2U);
        printf(",\"right\":"); print_membership(&render, 4U);
        printf(",\"closed_union_count\":%lu,\"resident_rooms\":%lu}",
            (unsigned long)closed_result.visible.room_count, (unsigned long)scene.room_count);
    }
    fclose(tour);
    printf("],\"view_count\":%lu,\"isolation_checks\":%lu,\"mono_checks\":%lu,"
        "\"original_eye_oracle_checks\":%lu,\"limited_queue_checks\":%lu,"
        "\"left_only_views\":%lu,\"right_only_views\":%lu,\"closed_reductions\":%lu,"
        "\"streaming_installs\":%lu,\"eye_only_installs\":%lu,\"evicted_rooms\":%llu,"
        "\"peak_resident_rooms\":%lu,\"peak_vertices\":%lu,\"peak_batches\":%lu,"
        "\"peak_textures\":%lu,\"vertex_capacity\":%u,\"batch_capacity\":%u,"
        "\"texture_capacity\":%lu,\"canonical_publication_fnv1a64\":\"%016llx\","
        "\"stationary_hold_checks\":%lu,\"released_preload_rooms\":%lu,"
        "\"frame_aging\":%s,\"protect_preloads\":%s}\n",
        (unsigned long)view_count, (unsigned long)isolation_checks, (unsigned long)mono_checks,
        (unsigned long)oracle_checks, (unsigned long)capacity_checks,
        (unsigned long)left_only_views, (unsigned long)right_only_views,
        (unsigned long)closed_reductions, (unsigned long)streaming_installs,
        (unsigned long)eye_only_installs, (unsigned long long)scene.rooms_evicted,
        (unsigned long)peak_rooms, (unsigned long)peak_vertices, (unsigned long)peak_batches,
        (unsigned long)peak_textures, GE_ROUTE_VERTEX_CAPACITY, GE_ROUTE_BATCH_CAPACITY,
        (unsigned long)texture_capacity, (unsigned long long)canonical_digest,
        (unsigned long)hold_checks, (unsigned long)released_preloads,
        frame_aging ? "true" : "false", protect_preloads ? "true" : "false");
    exit_status = view_count && closed_reductions && released_preloads ? 0 : 1;
cleanup:
    ge_dam_dynamic_scene_close(&scene);
    ge_dam_visibility_runtime_close(&runtime);
    ge_asset_pack_close(&pack);
    free(background.bytes);
    return exit_status;
}
