#include "ge_dam_room.h"
#include "ge_dam_world.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PROBE_ROOM_COUNT 10U
#define PROBE_LEVEL_SCALE 0.23363999f

typedef struct ProbeFile {
    uint8_t *bytes;
    size_t size;
} ProbeFile;

typedef struct ProbeHit {
    float distance;
    size_t batch;
    size_t vertex;
    int hit;
} ProbeHit;

typedef struct ProbeConflict {
    size_t nearest_batch;
    size_t pipeline_batch;
    size_t pixels;
} ProbeConflict;

static ProbeFile read_file(const char *path, int required)
{
    ProbeFile result = {NULL, 0U};
    FILE *stream = fopen(path, "rb");
    long length;
    if (stream == NULL) {
        if (required) fprintf(stderr, "cannot open %s: %s\n", path,
                              strerror(errno));
        return result;
    }
    if (fseek(stream, 0L, SEEK_END) != 0 || (length = ftell(stream)) < 0L
            || fseek(stream, 0L, SEEK_SET) != 0) {
        fclose(stream);
        return result;
    }
    result.size = (size_t)length;
    result.bytes = malloc(result.size != 0U ? result.size : 1U);
    if (result.bytes == NULL
            || fread(result.bytes, 1U, result.size, stream) != result.size) {
        free(result.bytes);
        result.bytes = NULL;
        result.size = 0U;
    }
    fclose(stream);
    return result;
}

static float dot3(const float a[3], const float b[3])
{ return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }

static void cross3(const float a[3], const float b[3], float out[3])
{
    out[0] = a[1]*b[2] - a[2]*b[1];
    out[1] = a[2]*b[0] - a[0]*b[2];
    out[2] = a[0]*b[1] - a[1]*b[0];
}

static int normalize3(float value[3])
{
    const float length = sqrtf(dot3(value, value));
    if (!(length > 0.000001f) || !isfinite(length)) return 0;
    value[0] /= length; value[1] /= length; value[2] /= length;
    return 1;
}

static int ray_triangle(const float origin[3], const float ray[3],
        const GeDamRoomWorldVertex *triangle, float *distance,
        float *determinant_out)
{
    const float epsilon = 0.00001f;
    float edge1[3], edge2[3], p[3], from[3], q[3];
    float determinant, inverse, u, v;
    size_t axis;
    for (axis = 0U; axis < 3U; ++axis) {
        edge1[axis] = triangle[1].world[axis] - triangle[0].world[axis];
        edge2[axis] = triangle[2].world[axis] - triangle[0].world[axis];
        from[axis] = origin[axis] - triangle[0].world[axis];
    }
    cross3(ray, edge2, p);
    determinant = dot3(edge1, p);
    if (fabsf(determinant) < epsilon) return 0;
    inverse = 1.0f / determinant;
    u = dot3(from, p) * inverse;
    if (u < 0.0f || u > 1.0f) return 0;
    cross3(from, edge1, q);
    v = dot3(ray, q) * inverse;
    if (v < 0.0f || u + v > 1.0f) return 0;
    *distance = dot3(edge2, q) * inverse;
    *determinant_out = determinant;
    return *distance > epsilon;
}

static int cull_accepts(const GeDamRoomDrawBatch *batch, float determinant)
{
    switch (batch->material.cull_mode) {
    case GE_PICA_CULL_NONE: return 1;
    case GE_PICA_CULL_BACK: return determinant > 0.0f;
    case GE_PICA_CULL_FRONT: return determinant < 0.0f;
    default: return 0;
    }
}

static void probe_ray(const GeDamRoomWorldVertex *vertices,
        const GeDamRoomDrawBatch *batches, size_t batch_count,
        const float origin[3], const float ray[3],
        ProbeHit *nearest, ProbeHit *pipeline)
{
    float depth = INFINITY;
    size_t batch_index;
    *nearest = (ProbeHit){INFINITY, SIZE_MAX, SIZE_MAX, 0};
    *pipeline = (ProbeHit){INFINITY, SIZE_MAX, SIZE_MAX, 0};
    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];
        size_t vertex;
        for (vertex = batch->first_vertex;
                vertex < batch->first_vertex + batch->vertex_count;
                vertex += 3U) {
            float distance, determinant;
            if (!ray_triangle(origin, ray, &vertices[vertex], &distance,
                              &determinant)
                    || !cull_accepts(batch, determinant)) continue;
            if (!nearest->hit || distance < nearest->distance)
                *nearest = (ProbeHit){distance, batch_index, vertex, 1};
            if (batch->material.depth_test_enabled != 0U
                    && distance >= depth) continue;
            *pipeline = (ProbeHit){distance, batch_index, vertex, 1};
            if (batch->material.depth_write_enabled != 0U) depth = distance;
        }
    }
}

static void batch_color(const GeDamRoomDrawBatch *batch, uint8_t rgb[3])
{
    uint32_t value = batch->room_id * UINT32_C(0x9e3779b1)
        ^ batch->texture.texture_id * UINT32_C(0x85ebca6b)
        ^ (uint32_t)batch->list_kind * UINT32_C(0xc2b2ae35);
    value ^= value >> 16;
    rgb[0] = (uint8_t)(48U + (value & 0xafU));
    rgb[1] = (uint8_t)(48U + ((value >> 8) & 0xafU));
    rgb[2] = (uint8_t)(48U + ((value >> 16) & 0xafU));
}

static int write_ppm(const char *path, const uint8_t *pixels,
                     size_t width, size_t height)
{
    FILE *stream = fopen(path, "wb");
    if (stream == NULL) return 0;
    if (fprintf(stream, "P6\n%zu %zu\n255\n", width, height) < 0
            || fwrite(pixels, 3U, width * height, stream) != width * height
            || fclose(stream) != 0) return 0;
    return 1;
}

static int compare_conflicts(const void *left, const void *right)
{
    const ProbeConflict *a = left, *b = right;
    return a->pixels < b->pixels ? 1 : a->pixels > b->pixels ? -1 : 0;
}

static int parse_vec3(const char *text, float value[3])
{ return sscanf(text, "%f,%f,%f", &value[0], &value[1], &value[2]) == 3; }

static void usage(const char *program)
{
    fprintf(stderr,
        "usage: %s --origin x,y,z --forward x,y,z [--up x,y,z] "
        "[--raw] [--fov degrees] [--size width,height] --output prefix\n",
        program);
}

int main(int argc, char **argv)
{
    ProbeFile background = {0};
    ProbeFile points[PROBE_ROOM_COUNT] = {{0}};
    ProbeFile primary[PROBE_ROOM_COUNT] = {{0}};
    ProbeFile secondary[PROBE_ROOM_COUNT] = {{0}};
    GeDamRoomBlobDescriptor descriptors[PROBE_ROOM_COUNT] = {{0}};
    GeDamWorld world;
    GeDamRoomScene scene;
    GeDamRoomSceneStorage storage;
    GeDamRoomWorldVertex *vertices = NULL;
    GeDamRoomDrawBatch *batches = NULL;
    uint8_t rooms[PROBE_ROOM_COUNT];
    size_t room_count = 0U, room_index, batch_index;
    float origin[3] = {0}, forward[3] = {0}, up[3] = {0,1,0};
    float right[3], corrected_up[3];
    float fov = 60.0f;
    size_t width = 160U, height = 90U;
    const char *prefix = NULL;
    int origin_set = 0, forward_set = 0, raw = 0;
    uint8_t *nearest_pixels = NULL, *pipeline_pixels = NULL;
    uint8_t *conflict_pixels = NULL;
    ProbeConflict *conflicts = NULL;
    size_t conflict_count = 0U, conflict_pixels_total = 0U;
    char path[1024];
    int argument;

    for (argument = 1; argument < argc; ++argument) {
        if (strcmp(argv[argument], "--origin") == 0 && argument + 1 < argc)
            origin_set = parse_vec3(argv[++argument], origin);
        else if (strcmp(argv[argument], "--forward") == 0
                && argument + 1 < argc)
            forward_set = parse_vec3(argv[++argument], forward);
        else if (strcmp(argv[argument], "--up") == 0 && argument + 1 < argc) {
            if (!parse_vec3(argv[++argument], up)) return 2;
        } else if (strcmp(argv[argument], "--fov") == 0
                && argument + 1 < argc) fov = strtof(argv[++argument], NULL);
        else if (strcmp(argv[argument], "--size") == 0
                && argument + 1 < argc) {
            unsigned parsed_width, parsed_height;
            if (sscanf(argv[++argument], "%u,%u", &parsed_width,
                       &parsed_height) != 2) return 2;
            width = parsed_width; height = parsed_height;
        } else if (strcmp(argv[argument], "--output") == 0
                && argument + 1 < argc) prefix = argv[++argument];
        else if (strcmp(argv[argument], "--raw") == 0) raw = 1;
        else { usage(argv[0]); return 2; }
    }
    if (!origin_set || !forward_set || prefix == NULL || width == 0U
            || height == 0U || width > 640U || height > 480U
            || !(fov > 1.0f && fov < 179.0f)) {
        usage(argv[0]); return 2;
    }
    if (raw) {
        origin[0] /= PROBE_LEVEL_SCALE;
        origin[1] /= PROBE_LEVEL_SCALE;
        origin[2] /= PROBE_LEVEL_SCALE;
    }
    if (!normalize3(forward) || !normalize3(up)) return 2;
    cross3(forward, up, right);
    if (!normalize3(right)) return 2;
    cross3(right, forward, corrected_up);
    if (!normalize3(corrected_up)) return 2;

    background = read_file("build/u/assets/obseg/bg/bg_dam_all_p.bin", 1);
    if (background.bytes == NULL
            || ge_dam_world_parse(background.bytes, background.size, &world)
                != GE_DAM_WORLD_OK
            || ge_dam_world_collect_connected(&world, 135U, rooms,
                PROBE_ROOM_COUNT, &room_count) != GE_DAM_WORLD_OK
            || room_count != PROBE_ROOM_COUNT) return 1;
    for (room_index = 0U; room_index < room_count; ++room_index) {
        const GeDamWorldRoom *room = ge_dam_world_room(&world,
                                                       rooms[room_index]);
        snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/point_table.bin",
            (unsigned)rooms[room_index]);
        points[room_index] = read_file(path, 1);
        snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/primary_gdl.bin",
            (unsigned)rooms[room_index]);
        primary[room_index] = read_file(path, 1);
        snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/secondary_gdl.bin",
            (unsigned)rooms[room_index]);
        secondary[room_index] = read_file(path, 0);
        if (room == NULL || points[room_index].bytes == NULL
                || primary[room_index].bytes == NULL) return 1;
        descriptors[room_index] = (GeDamRoomBlobDescriptor){
            rooms[room_index], {room->origin[0], room->origin[1],
            room->origin[2]}, points[room_index].bytes,
            points[room_index].size, primary[room_index].bytes,
            primary[room_index].size, secondary[room_index].bytes,
            secondary[room_index].size};
    }
    if (ge_dam_rooms_build(descriptors, room_count, NULL, NULL, &scene)
            != GE_DAM_ROOM_CAPACITY_EXCEEDED) return 1;
    vertices = malloc(scene.required_vertex_count * sizeof(*vertices));
    batches = malloc(scene.required_batch_count * sizeof(*batches));
    nearest_pixels = calloc(width * height, 3U);
    pipeline_pixels = calloc(width * height, 3U);
    conflict_pixels = calloc(width * height, 3U);
    conflicts = calloc(scene.required_batch_count * scene.required_batch_count,
                       sizeof(*conflicts));
    if (!vertices || !batches || !nearest_pixels || !pipeline_pixels
            || !conflict_pixels || !conflicts) return 1;
    storage = (GeDamRoomSceneStorage){vertices, scene.required_vertex_count,
                                     batches, scene.required_batch_count};
    if (ge_dam_rooms_build(descriptors, room_count, NULL, &storage, &scene)
            != GE_DAM_ROOM_OK) return 1;

    {
        const float tangent = tanf(fov * 0.5f * 3.14159265358979323846f
                                   / 180.0f);
        const float aspect = (float)width / (float)height;
        size_t y, x;
        for (y = 0U; y < height; ++y) for (x = 0U; x < width; ++x) {
            const float screen_x = (2.0f * ((float)x + 0.5f)
                / (float)width - 1.0f) * tangent * aspect;
            const float screen_y = (1.0f - 2.0f * ((float)y + 0.5f)
                / (float)height) * tangent;
            float ray[3];
            ProbeHit nearest, pipeline;
            uint8_t *near_color = nearest_pixels + (y*width+x)*3U;
            uint8_t *pipe_color = pipeline_pixels + (y*width+x)*3U;
            uint8_t *conflict_color = conflict_pixels + (y*width+x)*3U;
            size_t axis;
            for (axis = 0U; axis < 3U; ++axis)
                ray[axis] = forward[axis] + right[axis]*screen_x
                    + corrected_up[axis]*screen_y;
            normalize3(ray);
            probe_ray(vertices, batches, scene.batch_count, origin, ray,
                      &nearest, &pipeline);
            if (nearest.hit) batch_color(&batches[nearest.batch], near_color);
            if (pipeline.hit)
                batch_color(&batches[pipeline.batch], pipe_color);
            if (nearest.hit && pipeline.hit
                    && nearest.batch != pipeline.batch) {
                const size_t key = nearest.batch * scene.batch_count
                    + pipeline.batch;
                if (conflicts[key].pixels == 0U) {
                    conflicts[key].nearest_batch = nearest.batch;
                    conflicts[key].pipeline_batch = pipeline.batch;
                    conflict_count++;
                }
                conflicts[key].pixels++;
                conflict_pixels_total++;
                conflict_color[0] = 255U;
                conflict_color[1] = batches[nearest.batch]
                        .material.depth_write_enabled ? 48U : 192U;
                conflict_color[2] = 32U;
            } else if (nearest.hit) {
                conflict_color[0] = conflict_color[1] =
                    conflict_color[2] = 32U;
            }
        }
    }
    snprintf(path, sizeof(path), "%s-nearest.ppm", prefix);
    if (!write_ppm(path, nearest_pixels, width, height)) return 1;
    snprintf(path, sizeof(path), "%s-pipeline.ppm", prefix);
    if (!write_ppm(path, pipeline_pixels, width, height)) return 1;
    snprintf(path, sizeof(path), "%s-conflicts.ppm", prefix);
    if (!write_ppm(path, conflict_pixels, width, height)) return 1;

    {
        ProbeConflict *compact = malloc(conflict_count * sizeof(*compact));
        size_t cursor = 0U, index;
        if (compact == NULL && conflict_count != 0U) return 1;
        for (index = 0U; index < scene.batch_count * scene.batch_count;
                ++index)
            if (conflicts[index].pixels != 0U)
                compact[cursor++] = conflicts[index];
        qsort(compact, conflict_count, sizeof(*compact), compare_conflicts);
        printf("Dam probe: %zux%zu, %zu batches, %zu conflicting pixels "
               "(%zu pairs)\n", width, height, scene.batch_count,
               conflict_pixels_total, conflict_count);
        for (index = 0U; index < conflict_count && index < 20U; ++index) {
            const GeDamRoomDrawBatch *near_batch =
                &batches[compact[index].nearest_batch];
            const GeDamRoomDrawBatch *pipe_batch =
                &batches[compact[index].pipeline_batch];
            printf("%6zu px: nearest batch %zu room %u tex %u list %u "
                   "depth %u/%u -> pipeline batch %zu room %u tex %u "
                   "list %u depth %u/%u\n", compact[index].pixels,
                   compact[index].nearest_batch,
                   (unsigned)near_batch->room_id,
                   (unsigned)near_batch->texture.texture_id,
                   (unsigned)near_batch->list_kind,
                   (unsigned)near_batch->material.depth_test_enabled,
                   (unsigned)near_batch->material.depth_write_enabled,
                   compact[index].pipeline_batch,
                   (unsigned)pipe_batch->room_id,
                   (unsigned)pipe_batch->texture.texture_id,
                   (unsigned)pipe_batch->list_kind,
                   (unsigned)pipe_batch->material.depth_test_enabled,
                   (unsigned)pipe_batch->material.depth_write_enabled);
        }
        free(compact);
    }

    for (batch_index = 0U; batch_index < room_count; ++batch_index) {
        free(secondary[batch_index].bytes); free(primary[batch_index].bytes);
        free(points[batch_index].bytes);
    }
    free(conflicts); free(conflict_pixels); free(pipeline_pixels);
    free(nearest_pixels); free(batches); free(vertices);
    free(background.bytes);
    return 0;
}
