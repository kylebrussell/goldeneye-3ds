#include "ge_asset_pack.h"
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

int main(int argc, char **argv)
{
    static const GeDamDynamicSceneLimits limits = {
        GE_DAM_WORLD_MAX_ROOMS,
        GE_ROUTE_VERTEX_CAPACITY,
        GE_ROUTE_BATCH_CAPACITY,
    };
    const char *route_name;
    const char *background_path;
    const char *pack_path;
    const char *tour_path;
    char *capacity_end = NULL;
    unsigned long texture_capacity;
    FileData background;
    GeAssetPack pack;
    GeDamWorld world;
    GeDamVisibilityRuntime visibility;
    GeDamPreloadQueue queue;
    GeDamDynamicScene scene;
    GeOriginalBgVisibilityProviders providers;
    uint8_t initial[GE_ROUTE_INITIAL_ROOM_CAPACITY];
    uint8_t portal_controls[GE_DAM_WORLD_MAX_PORTALS];
    uint8_t route_rooms[GE_DAM_WORLD_MAX_ROOMS] = {0};
    uint8_t explicit_rooms[GE_DAM_WORLD_MAX_ROOMS] = {0};
    uint8_t visibility_rooms[GE_DAM_WORLD_MAX_ROOMS] = {0};
    uint8_t initial_room_set[GE_DAM_WORLD_MAX_ROOMS] = {0};
    uint8_t installed_rooms[GE_DAM_WORLD_MAX_ROOMS] = {0};
    size_t initial_count = 0U;
    size_t view_count = 0U;
    size_t explicit_requests = 0U;
    size_t visibility_attempts = 0U;
    size_t visibility_accepted = 0U;
    size_t peak_visible_rooms = 0U;
    size_t peak_resident_rooms = 0U;
    size_t peak_textures = 0U;
    size_t peak_vertices = 0U;
    size_t peak_batches = 0U;
    size_t portal;
    int capacity_ok = 1;
    int result = 0;
    FILE *tour = NULL;

    if (argc != 6) {
        fprintf(stderr, "usage: %s ROUTE BACKGROUND GEPACK GEVIEW TEXTURE_CAP\n",
                argv[0]);
        return 2;
    }
    route_name = argv[1];
    background_path = argv[2];
    pack_path = argv[3];
    tour_path = argv[4];
    errno = 0;
    texture_capacity = strtoul(argv[5], &capacity_end, 0);
    if (errno != 0 || capacity_end == argv[5] || *capacity_end != '\0'
            || texture_capacity == 0U) {
        fprintf(stderr, "invalid texture capacity: %s\n", argv[5]);
        return 2;
    }
    background = read_file(background_path);
    if (background.bytes == NULL
            || ge_dam_world_parse(background.bytes, background.size, &world)
                != GE_DAM_WORLD_OK
            || ge_asset_pack_open(&pack, pack_path) != GE_ASSET_PACK_OK
            || ge_dam_visibility_runtime_load(&pack, &visibility)
                != GE_DAM_VISIBILITY_RUNTIME_OK
            || !collect_initial_rooms(&world, 135U, initial, &initial_count)
            || ge_dam_preload_queue_init(&queue, world.room_count,
                GE_DAM_PRELOAD_MAX_ROOMS, initial, initial_count)
                != GE_DAM_PRELOAD_OK
            || ge_dam_dynamic_scene_init(&scene, &pack, &world,
                initial, initial_count, &limits)
                != GE_DAM_DYNAMIC_SCENE_OK) {
        fprintf(stderr, "could not initialize authored Dam route capacity probe\n");
        free(background.bytes);
        return 2;
    }
    for (portal = 0U; portal < world.portal_count; ++portal) {
        portal_controls[portal] = world.portals[portal].control_bytes1;
    }
    providers = ge_dam_preload_queue_providers(
        &queue, portal_controls, world.portal_count);
    peak_resident_rooms = scene.room_count;
    peak_textures = count_unique_rare_textures(&scene);
    peak_vertices = scene.scene.vertex_count;
    peak_batches = scene.scene.batch_count;
    if (peak_textures > texture_capacity) capacity_ok = 0;
    for (portal = 0U; portal < initial_count; ++portal) {
        initial_room_set[initial[portal]] = 1U;
        installed_rooms[initial[portal]] = 1U;
    }
    tour = fopen(tour_path, "r");
    if (tour == NULL) {
        fprintf(stderr, "could not open route tour: %s\n", tour_path);
        result = 2;
    }
    while (result == 0 && tour != NULL) {
        TourView view;
        GeOriginalBondCameraResult camera;
        int read_status = read_tour_view(tour, &view);
        size_t closure_pass;

        if (read_status == 0) break;
        if (read_status < 0 || view.room >= world.room_count
                || !build_camera(&world, &view, &camera)) {
            fprintf(stderr, "invalid route view at index %lu\n",
                    (unsigned long)view_count);
            result = 2;
            break;
        }
        ++view_count;
        route_rooms[view.room] = 1U;
        if (ge_dam_preload_queue_room_state(&queue, view.room)
                == GE_DAM_PRELOAD_ROOM_UNLOADED) {
            const size_t before = queue.accepted_count;
            (void)ge_dam_preload_queue_request(&queue, view.room);
            if (queue.accepted_count != before) {
                ++explicit_requests;
                explicit_rooms[view.room] = 1U;
            }
        }
        for (closure_pass = 0U;
                closure_pass <= GE_DAM_WORLD_MAX_ROOMS; ++closure_pass) {
            GeOriginalBgVisibilityResult visible;
            const size_t accepted_before = queue.accepted_count;
            GeDamDynamicSceneStatus install_status;
            uint8_t pending_room;
            uint8_t states_before[GE_DAM_PRELOAD_MAX_ROOMS];
            size_t room_index;

            memcpy(states_before, queue.states, sizeof(states_before));
            if (ge_dam_visibility_runtime_run_with_providers(
                    &visibility, &camera, view.position, &providers, &visible)
                        != GE_DAM_VISIBILITY_RUNTIME_OK) {
                fprintf(stderr, "visibility failed for %s\n", view.label);
                result = 2;
                break;
            }
            visibility_attempts += visible.preload_request_count;
            visibility_accepted += queue.accepted_count - accepted_before;
            for (room_index = 0U; room_index < world.room_count; ++room_index) {
                if (states_before[room_index]
                            == GE_DAM_PRELOAD_ROOM_UNLOADED
                        && queue.states[room_index]
                            == GE_DAM_PRELOAD_ROOM_QUEUED) {
                    visibility_rooms[room_index] = 1U;
                }
            }
            if (visible.room_count > peak_visible_rooms) {
                peak_visible_rooms = visible.room_count;
            }
            if (queue.pending_count == 0U) break;
            if (ge_dam_preload_queue_peek(&queue, &pending_room)
                    != GE_DAM_PRELOAD_OK) {
                result = 2;
                break;
            }
            install_status = ge_dam_dynamic_scene_install_next(&scene, &queue);
            if (install_status != GE_DAM_DYNAMIC_SCENE_OK) {
                fprintf(stderr, "room %u install failed at %s: %s\n",
                        pending_room, view.label,
                        ge_dam_dynamic_scene_status_name(install_status));
                capacity_ok = 0;
                result = 1;
                break;
            }
            installed_rooms[pending_room] = 1U;
            if (scene.room_count > peak_resident_rooms) {
                peak_resident_rooms = scene.room_count;
            }
            if (scene.scene.vertex_count > peak_vertices) {
                peak_vertices = scene.scene.vertex_count;
            }
            if (scene.scene.batch_count > peak_batches) {
                peak_batches = scene.scene.batch_count;
            }
            {
                const size_t textures = count_unique_rare_textures(&scene);
                if (textures > peak_textures) peak_textures = textures;
                if (textures > texture_capacity) capacity_ok = 0;
            }
        }
        if (closure_pass > GE_DAM_WORLD_MAX_ROOMS) {
            fprintf(stderr, "visibility preload closure did not converge\n");
            result = 2;
        }
    }
    if (tour != NULL) fclose(tour);
    if (peak_resident_rooms > GE_DAM_WORLD_MAX_ROOMS
            || peak_vertices > GE_ROUTE_VERTEX_CAPACITY
            || peak_batches > GE_ROUTE_BATCH_CAPACITY
            || peak_textures > texture_capacity) {
        capacity_ok = 0;
    }
    printf("{\"route\":\"%s\",\"views\":%lu,"
           "\"explicit_requests\":%lu,\"visibility_preload_attempts\":%lu,"
           "\"visibility_preloads_accepted\":%lu,"
           "\"peak_visible_rooms\":%lu,\"peak_resident_rooms\":%lu,"
           "\"peak_unique_rare_textures\":%lu,\"peak_vertices\":%lu,"
           "\"peak_batches\":%lu,\"room_capacity\":%u,"
           "\"texture_capacity\":%lu,\"vertex_capacity\":%u,"
           "\"batch_capacity\":%u,\"route_rooms\":",
           route_name, (unsigned long)view_count,
           (unsigned long)explicit_requests,
           (unsigned long)visibility_attempts,
           (unsigned long)visibility_accepted,
           (unsigned long)peak_visible_rooms,
           (unsigned long)peak_resident_rooms,
           (unsigned long)peak_textures,
           (unsigned long)peak_vertices,
           (unsigned long)peak_batches,
           GE_DAM_WORLD_MAX_ROOMS, texture_capacity,
           GE_ROUTE_VERTEX_CAPACITY, GE_ROUTE_BATCH_CAPACITY);
    print_room_set(route_rooms);
    printf(",\"initial_rooms\":");
    print_room_set(initial_room_set);
    printf(",\"explicit_rooms\":");
    print_room_set(explicit_rooms);
    printf(",\"visibility_preload_rooms\":");
    print_room_set(visibility_rooms);
    printf(",\"resident_rooms\":");
    print_room_set(installed_rooms);
    printf(",\"capacity_ok\":%s}\n", capacity_ok ? "true" : "false");
    ge_dam_dynamic_scene_close(&scene);
    ge_dam_visibility_runtime_close(&visibility);
    ge_asset_pack_close(&pack);
    free(background.bytes);
    if (!capacity_ok && result == 0) result = 1;
    return result;
}
