#include "ge_dam_room.h"
#include "ge_dam_camera.h"
#include "ge_dam_world.h"
#include "ge_draw_batch_visibility.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ROOM_LOAD_COUNT 10U
#define SOURCE_VERTEX_CAPACITY 16384U
#define PROJECTED_VERTEX_CAPACITY 65536U
#define TEXTURE_CAPACITY 64U

typedef struct FileData {
    uint8_t *bytes;
    size_t size;
} FileData;

static FileData read_file(const char *path, int required)
{
    FILE *stream = fopen(path, "rb");
    FileData result = {NULL, 0U};
    long length;

    if (stream == NULL) {
        assert(!required);
        return result;
    }
    assert(fseek(stream, 0L, SEEK_END) == 0);
    length = ftell(stream);
    assert(length >= 0L);
    assert(fseek(stream, 0L, SEEK_SET) == 0);
    result.size = (size_t)length;
    result.bytes = malloc(result.size == 0U ? 1U : result.size);
    assert(result.bytes != NULL);
    assert(fread(result.bytes, 1U, result.size, stream) == result.size);
    assert(fclose(stream) == 0);
    return result;
}

static size_t count_unique_textures(const GeDamRoomDrawBatch *batches,
                                    size_t batch_count)
{
    uint16_t textures[TEXTURE_CAPACITY + 1U];
    size_t texture_count = 0U;
    size_t batch_index;

    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];
        size_t texture_index;

        if (batch->texture_valid == 0U
                || batch->material.texture_enabled == 0U
                || batch->material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) {
            continue;
        }
        for (texture_index = 0U; texture_index < texture_count;
                ++texture_index) {
            if (textures[texture_index] == batch->texture.texture_id) break;
        }
        if (texture_index == texture_count) {
            assert(texture_count < sizeof(textures) / sizeof(textures[0]));
            textures[texture_count++] = batch->texture.texture_id;
        }
    }
    return texture_count;
}

static void audit_floor_winding(const GeDamRoomWorldVertex *vertices,
                                const GeDamRoomDrawBatch *batches,
                                size_t batch_count, size_t triangle_count)
{
    size_t cull_batches[4] = {0U};
    size_t horizontal_positive[4] = {0U};
    size_t horizontal_negative[4] = {0U};
    size_t horizontal_by_combine[6] = {0U};
    size_t horizontal_black_by_combine[6] = {0U};
    size_t horizontal_textured = 0U;
    size_t audited_triangles = 0U;
    size_t batch_index;

    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];
        const size_t cull = (size_t)batch->material.cull_mode;
        size_t vertex_index;

        assert(cull < sizeof(cull_batches) / sizeof(cull_batches[0]));
        assert(batch->vertex_count % 3U == 0U);
        cull_batches[cull]++;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                vertex_index += 3U) {
            const float ax = vertices[vertex_index + 1U].world[0]
                - vertices[vertex_index].world[0];
            const float ay = vertices[vertex_index + 1U].world[1]
                - vertices[vertex_index].world[1];
            const float az = vertices[vertex_index + 1U].world[2]
                - vertices[vertex_index].world[2];
            const float bx = vertices[vertex_index + 2U].world[0]
                - vertices[vertex_index].world[0];
            const float by = vertices[vertex_index + 2U].world[1]
                - vertices[vertex_index].world[1];
            const float bz = vertices[vertex_index + 2U].world[2]
                - vertices[vertex_index].world[2];
            const float nx = ay * bz - az * by;
            const float ny = az * bx - ax * bz;
            const float nz = ax * by - ay * bx;
            const size_t combine = (size_t)batch->material.color_combine;

            audited_triangles++;
            if (fabsf(ny) >= fabsf(nx) && fabsf(ny) >= fabsf(nz)) {
                const int all_black =
                    vertices[vertex_index].processed.rgba[0] == 0U
                    && vertices[vertex_index].processed.rgba[1] == 0U
                    && vertices[vertex_index].processed.rgba[2] == 0U
                    && vertices[vertex_index + 1U].processed.rgba[0] == 0U
                    && vertices[vertex_index + 1U].processed.rgba[1] == 0U
                    && vertices[vertex_index + 1U].processed.rgba[2] == 0U
                    && vertices[vertex_index + 2U].processed.rgba[0] == 0U
                    && vertices[vertex_index + 2U].processed.rgba[1] == 0U
                    && vertices[vertex_index + 2U].processed.rgba[2] == 0U;

                assert(combine < sizeof(horizontal_by_combine)
                    / sizeof(horizontal_by_combine[0]));
                horizontal_by_combine[combine]++;
                if (all_black) horizontal_black_by_combine[combine]++;
                if (ny >= 0.0f) horizontal_positive[cull]++;
                else horizontal_negative[cull]++;
                if (batch->texture_valid != 0U
                        && batch->material.texture_enabled != 0U
                        && batch->material.texture_source
                            == GE_PICA_TEXTURE_SOURCE_RARE_ID) {
                    horizontal_textured++;
                }
            }
        }
    }
    assert(audited_triangles == triangle_count);
    printf("Cull batches none/front/back/both: %zu/%zu/%zu/%zu\n",
           cull_batches[GE_PICA_CULL_NONE],
           cull_batches[GE_PICA_CULL_FRONT],
           cull_batches[GE_PICA_CULL_BACK],
           cull_batches[GE_PICA_CULL_BOTH]);
    printf("Horizontal triangles +Y none/front/back/both: %zu/%zu/%zu/%zu; "
           "-Y: %zu/%zu/%zu/%zu; textured %zu\n",
           horizontal_positive[GE_PICA_CULL_NONE],
           horizontal_positive[GE_PICA_CULL_FRONT],
           horizontal_positive[GE_PICA_CULL_BACK],
           horizontal_positive[GE_PICA_CULL_BOTH],
           horizontal_negative[GE_PICA_CULL_NONE],
           horizontal_negative[GE_PICA_CULL_FRONT],
           horizontal_negative[GE_PICA_CULL_BACK],
           horizontal_negative[GE_PICA_CULL_BOTH], horizontal_textured);
    printf("Horizontal combine shade/prim/env/tex/tex*shade/tex*prim: "
           "%zu/%zu/%zu/%zu/%zu/%zu; all-black: "
           "%zu/%zu/%zu/%zu/%zu/%zu\n",
           horizontal_by_combine[GE_PICA_COMBINE_SHADE],
           horizontal_by_combine[GE_PICA_COMBINE_PRIMITIVE],
           horizontal_by_combine[GE_PICA_COMBINE_ENVIRONMENT],
           horizontal_by_combine[GE_PICA_COMBINE_TEXTURE0],
           horizontal_by_combine[GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE],
           horizontal_by_combine[GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE],
           horizontal_black_by_combine[GE_PICA_COMBINE_SHADE],
           horizontal_black_by_combine[GE_PICA_COMBINE_PRIMITIVE],
           horizontal_black_by_combine[GE_PICA_COMBINE_ENVIRONMENT],
           horizontal_black_by_combine[GE_PICA_COMBINE_TEXTURE0],
           horizontal_black_by_combine[
               GE_PICA_COMBINE_TEXTURE0_MODULATE_SHADE],
           horizontal_black_by_combine[
               GE_PICA_COMBINE_TEXTURE0_MODULATE_PRIMITIVE]);
}

static void audit_background_depth_inheritance(
        const GeDamRoomDrawBatch *batches, size_t batch_count)
{
    size_t depth_tested = 0U;
    size_t depth_written = 0U;
    size_t depth_read_only = 0U;
    size_t batch_index;

    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];

        if (batch->texture_valid == 0U
                || batch->material.texture_enabled == 0U
                || batch->material.texture_source
                    != GE_PICA_TEXTURE_SOURCE_RARE_ID) {
            continue;
        }
        /* The extracted room list inherits G_ZBUFFER from GoldenEye's parent
         * background task. Every authored textured surface in this opening
         * set then selects a ZB render mode; secondary decals compare without
         * overwriting while primary terrain compares and updates. */
        assert(batch->material.depth_test_enabled != 0U);
        depth_tested++;
        if (batch->material.depth_write_enabled != 0U) depth_written++;
        else depth_read_only++;
    }
    assert(depth_tested == 865U);
    assert(depth_written == 815U);
    assert(depth_read_only == 50U);
    printf("Dam inherited background Z: %zu tested, %zu write, %zu read-only\n",
           depth_tested, depth_written, depth_read_only);
}

static void audit_texture_coordinates(const GeDamRoomWorldVertex *vertices,
                                      const GeDamRoomDrawBatch *batches,
                                      size_t batch_count)
{
    size_t batch_index;

    if (getenv("GE_DAM_TEXTURE_AUDIT") == NULL) return;
    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];
        int16_t min_s = INT16_MAX;
        int16_t max_s = INT16_MIN;
        int16_t min_t = INT16_MAX;
        int16_t max_t = INT16_MIN;
        float min_xyz[3] = {INFINITY, INFINITY, INFINITY};
        float max_xyz[3] = {-INFINITY, -INFINITY, -INFINITY};
        int all_black = 1;
        size_t vertex_index;

        if (batch->texture_valid == 0U) continue;
        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                ++vertex_index) {
            const GeGbiVertex *vertex = &vertices[vertex_index].source;
            size_t axis;
            if (vertex->texture_s < min_s) min_s = vertex->texture_s;
            if (vertex->texture_s > max_s) max_s = vertex->texture_s;
            if (vertex->texture_t < min_t) min_t = vertex->texture_t;
            if (vertex->texture_t > max_t) max_t = vertex->texture_t;
            if (vertices[vertex_index].processed.rgba[0] != 0U
                    || vertices[vertex_index].processed.rgba[1] != 0U
                    || vertices[vertex_index].processed.rgba[2] != 0U)
                all_black = 0;
            for (axis = 0U; axis < 3U; ++axis) {
                if (vertices[vertex_index].world[axis] < min_xyz[axis])
                    min_xyz[axis] = vertices[vertex_index].world[axis];
                if (vertices[vertex_index].world[axis] > max_xyz[axis])
                    max_xyz[axis] = vertices[vertex_index].world[axis];
            }
        }
        printf("texbatch %zu room %u id %u detail %u type %u tile %u "
               "scale %u/%u shift %u/%u st %d..%d/%d..%d tri %zu "
               "fog %u %02x%02x%02x %d/%d combine %u light %u black %d "
               "depth %u/%u xyz %.0f..%.0f/%.0f..%.0f/%.0f..%.0f "
               "mux %06x/%08x\n",
               batch_index, (unsigned)batch->room_id,
               (unsigned)batch->texture.texture_id,
               (unsigned)batch->texture.detail_texture_id,
               (unsigned)batch->texture.type,
               (unsigned)batch->texture.tile,
               (unsigned)batch->material.texture_scale_s,
               (unsigned)batch->material.texture_scale_t,
               (unsigned)batch->material.texture_shift_s,
               (unsigned)batch->material.texture_shift_t,
               (int)min_s, (int)max_s, (int)min_t, (int)max_t,
               batch->triangle_count,
               (unsigned)batch->material.fog_enabled,
               (unsigned)batch->material.fog_color.red,
               (unsigned)batch->material.fog_color.green,
               (unsigned)batch->material.fog_color.blue,
               (int)batch->material.fog_multiplier,
               (int)batch->material.fog_offset,
               (unsigned)batch->material.color_combine,
               (unsigned)batch->material.lighting_enabled, all_black,
               (unsigned)batch->material.depth_test_enabled,
               (unsigned)batch->material.depth_write_enabled,
               min_xyz[0], max_xyz[0], min_xyz[1], max_xyz[1],
               min_xyz[2], max_xyz[2],
               (unsigned)batch->material.combine_mux0,
               (unsigned)batch->material.combine_mux1);
    }
}

typedef struct GeDamRayHit {
    float distance;
    float determinant;
    size_t batch_index;
    size_t vertex_index;
} GeDamRayHit;

static int compare_ray_hits(const void *left, const void *right)
{
    const GeDamRayHit *a = left;
    const GeDamRayHit *b = right;

    return a->distance < b->distance ? -1 : a->distance > b->distance;
}

static int ray_triangle_distance(const float origin[3], const float ray[3],
        const GeDamRoomWorldVertex *triangle, float *distance,
        float *signed_determinant)
{
    const float epsilon = 0.00001f;
    float edge1[3];
    float edge2[3];
    float cross[3];
    float from_vertex[3];
    float second_cross[3];
    float determinant;
    float inverse;
    float u;
    float v;
    size_t axis;

    for (axis = 0U; axis < 3U; ++axis) {
        edge1[axis] = triangle[1].world[axis] - triangle[0].world[axis];
        edge2[axis] = triangle[2].world[axis] - triangle[0].world[axis];
        from_vertex[axis] = origin[axis] - triangle[0].world[axis];
    }
    cross[0] = ray[1] * edge2[2] - ray[2] * edge2[1];
    cross[1] = ray[2] * edge2[0] - ray[0] * edge2[2];
    cross[2] = ray[0] * edge2[1] - ray[1] * edge2[0];
    determinant = edge1[0] * cross[0] + edge1[1] * cross[1]
        + edge1[2] * cross[2];
    if (fabsf(determinant) < epsilon) return 0;
    *signed_determinant = determinant;
    inverse = 1.0f / determinant;
    u = (from_vertex[0] * cross[0] + from_vertex[1] * cross[1]
        + from_vertex[2] * cross[2]) * inverse;
    if (u < 0.0f || u > 1.0f) return 0;
    second_cross[0] = from_vertex[1] * edge1[2]
        - from_vertex[2] * edge1[1];
    second_cross[1] = from_vertex[2] * edge1[0]
        - from_vertex[0] * edge1[2];
    second_cross[2] = from_vertex[0] * edge1[1]
        - from_vertex[1] * edge1[0];
    v = (ray[0] * second_cross[0] + ray[1] * second_cross[1]
        + ray[2] * second_cross[2]) * inverse;
    if (v < 0.0f || u + v > 1.0f) return 0;
    *distance = (edge2[0] * second_cross[0]
        + edge2[1] * second_cross[1]
        + edge2[2] * second_cross[2]) * inverse;
    return *distance > epsilon;
}

static void audit_camera_ray(const GeDamRoomWorldVertex *vertices,
        const GeDamRoomDrawBatch *batches, size_t batch_count)
{
    const char *specification = getenv("GE_DAM_RAY");
    GeDamRayHit *hits;
    float origin[3];
    float ray[3];
    size_t hit_count = 0U;
    size_t batch_index;

    if (specification == NULL) return;
    assert(sscanf(specification, "%f,%f,%f,%f,%f,%f",
        &origin[0], &origin[1], &origin[2], &ray[0], &ray[1], &ray[2]) == 6);
    hits = malloc(PROJECTED_VERTEX_CAPACITY / 3U * sizeof(*hits));
    assert(hits != NULL);
    for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &batches[batch_index];
        size_t vertex_index;

        for (vertex_index = batch->first_vertex;
                vertex_index < batch->first_vertex + batch->vertex_count;
                vertex_index += 3U) {
            float distance;
            float determinant;

            if (ray_triangle_distance(origin, ray, &vertices[vertex_index],
                                      &distance, &determinant)) {
                hits[hit_count++] = (GeDamRayHit){
                    distance, determinant, batch_index, vertex_index,
                };
            }
        }
    }
    qsort(hits, hit_count, sizeof(*hits), compare_ray_hits);
    printf("Ray %.1f/%.1f/%.1f -> %.2f/%.2f/%.2f: %zu hit\n",
        origin[0], origin[1], origin[2], ray[0], ray[1], ray[2], hit_count);
    for (batch_index = 0U; batch_index < hit_count && batch_index < 16U;
            ++batch_index) {
        const GeDamRayHit *hit = &hits[batch_index];
        const GeDamRoomDrawBatch *batch = &batches[hit->batch_index];

        printf("  d %.2f face %c batch %zu room %u list %u tex %u type %u shift %u/%u "
               "combine %u cull %u depth %u/%u rgba %u/%u/%u\n",
            hit->distance, hit->determinant > 0.0f ? 'F' : 'B',
            hit->batch_index, (unsigned)batch->room_id,
            (unsigned)batch->list_kind, (unsigned)batch->texture.texture_id,
            (unsigned)batch->material.texture_type,
            (unsigned)batch->material.texture_shift_s,
            (unsigned)batch->material.texture_shift_t,
            (unsigned)batch->material.color_combine,
            (unsigned)batch->material.cull_mode,
            (unsigned)batch->material.depth_test_enabled,
            (unsigned)batch->material.depth_write_enabled,
            (unsigned)vertices[hit->vertex_index].processed.rgba[0],
            (unsigned)vertices[hit->vertex_index].processed.rgba[1],
            (unsigned)vertices[hit->vertex_index].processed.rgba[2]);
    }
    free(hits);
}

static void audit_spawn_clip_rejection(
    const GeDamRoomWorldVertex *vertices,size_t vertex_count,
    const GeDamRoomDrawBatch *batches,size_t batch_count)
{
    const float level_scale=0.23363999f;
    GeDamCameraConfig config=ge_dam_camera_default_config();
    GeDamCamera camera;float world_to_clip[4][4]={{0.0f}};
    size_t row,column,inner,batch_index,rejected_batches=0U;
    size_t rejected_vertices=0U;
    config.eye[0]=4719.0f/level_scale;
    config.eye[1]=-18.0f/level_scale;
    config.eye[2]=3949.0f/level_scale;
    config.forward[0]=-1.0f;config.forward[1]=0.0f;
    config.forward[2]=-0.000643f;
    config.up[0]=0.0f;config.up[1]=1.0f;config.up[2]=0.0f;
    config.aspect=4.0f/3.0f;config.near_distance=100.0f;
    config.far_distance=10000.0f;
    assert(ge_dam_camera_prepare(&config,&camera)==GE_DAM_CAMERA_OK);
    for(row=0U;row<4U;++row)for(column=0U;column<4U;++column)
        for(inner=0U;inner<4U;++inner)
            world_to_clip[row][column]+=
                camera.view[row][inner]*camera.projection[inner][column];
    /* Background vertices remain authored; the live camera consumes runtime
     * coordinates, whose exact Dam conversion is authored/level_scale. */
    for(row=0U;row<3U;++row)for(column=0U;column<4U;++column)
        world_to_clip[row][column]/=level_scale;
    for(batch_index=0U;batch_index<batch_count;++batch_index)
        if(!ge_draw_batch_world_may_intersect_clip_frustum(
                vertices,vertex_count,&batches[batch_index],world_to_clip)){
            ++rejected_batches;
            rejected_vertices+=batches[batch_index].vertex_count;
        }
    assert(rejected_batches>0U&&rejected_batches<batch_count
        &&rejected_vertices>0U&&rejected_vertices<vertex_count);
    printf("Dam authored spawn clip rejection: %zu/%zu batch, "
           "%zu/%zu vertex\n",rejected_batches,batch_count,
        rejected_vertices,vertex_count);
    {
        GeDrawBatchWorldBounds *bounds = calloc(batch_count, sizeof(*bounds));
        size_t angle, accelerated = 0U, decisions = 0U, repeat;
        volatile size_t checksum = 0U;
        clock_t started, scalar_ticks, bounded_ticks;
        assert(bounds != NULL);
        for (batch_index = 0U; batch_index < batch_count; ++batch_index)
            if (batches[batch_index].vertex_count >= 12U)
                assert(ge_draw_batch_world_bounds_build(vertices, vertex_count,
                    &batches[batch_index], &bounds[batch_index]));
        for (angle = 0U; angle < 64U; ++angle) {
            config.forward[0] = cosf((float)angle * 6.283185307f / 64.0f);
            config.forward[2] = sinf((float)angle * 6.283185307f / 64.0f);
            assert(ge_dam_camera_prepare(&config, &camera) == GE_DAM_CAMERA_OK);
            memset(world_to_clip, 0, sizeof(world_to_clip));
            for (row = 0U; row < 4U; ++row)
                for (column = 0U; column < 4U; ++column)
                    for (inner = 0U; inner < 4U; ++inner)
                        world_to_clip[row][column] += camera.view[row][inner]
                            * camera.projection[inner][column];
            for (row = 0U; row < 3U; ++row)
                for (column = 0U; column < 4U; ++column)
                    world_to_clip[row][column] /= level_scale;
            for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
                const GeDrawBatchBoundsVisibility classified =
                    ge_draw_batch_world_bounds_classify(
                        &bounds[batch_index], world_to_clip);
                const int exact = ge_draw_batch_world_may_intersect_clip_frustum(
                    vertices, vertex_count, &batches[batch_index], world_to_clip);
                if (classified != GE_DRAW_BATCH_BOUNDS_UNCERTAIN) {
                    ++accelerated;
                    assert(exact == (classified == GE_DRAW_BATCH_BOUNDS_INSIDE));
                }
                ++decisions;
            }
        }
        started = clock();
        for (repeat = 0U; repeat < 256U; ++repeat)
            for (batch_index = 0U; batch_index < batch_count; ++batch_index)
                checksum += (size_t)ge_draw_batch_world_may_intersect_clip_frustum(
                    vertices, vertex_count, &batches[batch_index], world_to_clip);
        scalar_ticks = clock() - started;
        started = clock();
        for (repeat = 0U; repeat < 256U; ++repeat)
            for (batch_index = 0U; batch_index < batch_count; ++batch_index) {
                const GeDrawBatchBoundsVisibility classified =
                    ge_draw_batch_world_bounds_classify(
                        &bounds[batch_index], world_to_clip);
                checksum += (size_t)(classified == GE_DRAW_BATCH_BOUNDS_INSIDE
                    || (classified == GE_DRAW_BATCH_BOUNDS_UNCERTAIN
                        && ge_draw_batch_world_may_intersect_clip_frustum(
                            vertices, vertex_count, &batches[batch_index],
                            world_to_clip)));
            }
        bounded_ticks = clock() - started;
        assert(accelerated > 0U && checksum > 0U);
        printf("Dam bounds: %zu/%zu exact decisions accelerated at 64 angles; "
               "256-frame CPU test scalar %.3fs, bounded %.3fs\n", accelerated,
               decisions, (double)scalar_ticks / CLOCKS_PER_SEC,
               (double)bounded_ticks / CLOCKS_PER_SEC);
        free(bounds);
    }
}

int main(void)
{
    FileData background = read_file(
        "build/u/assets/obseg/bg/bg_dam_all_p.bin", 1);
    FileData points[ROOM_LOAD_COUNT] = {{0}};
    FileData primary[ROOM_LOAD_COUNT] = {{0}};
    FileData secondary[ROOM_LOAD_COUNT] = {{0}};
    GeDamRoomBlobDescriptor descriptors[ROOM_LOAD_COUNT] = {{0}};
    GeDamWorld world;
    GeDamRoomScene scene;
    GeDamRoomSceneStorage storage;
    GeDamRoomWorldVertex *vertices;
    GeDamRoomDrawBatch *batches;
    uint8_t rooms[ROOM_LOAD_COUNT];
    size_t room_count;
    size_t room_index;
    size_t batch_index;
    size_t texture_count;

    assert(ge_dam_world_parse(background.bytes, background.size, &world)
        == GE_DAM_WORLD_OK);
    assert(ge_dam_world_collect_connected(&world, 135U, rooms,
        ROOM_LOAD_COUNT, &room_count) == GE_DAM_WORLD_OK);
    assert(room_count == ROOM_LOAD_COUNT);

    for (room_index = 0U; room_index < room_count; ++room_index) {
        char path[1024];
        const GeDamWorldRoom *room = ge_dam_world_room(&world,
                                                       rooms[room_index]);

        assert(room != NULL);
        assert(snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/point_table.bin",
            (unsigned)rooms[room_index]) > 0);
        points[room_index] = read_file(path, 1);
        assert(snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/primary_gdl.bin",
            (unsigned)rooms[room_index]) > 0);
        primary[room_index] = read_file(path, 1);
        assert(snprintf(path, sizeof(path),
            "build/3ds-levels/dam/rooms/room%03u/secondary_gdl.bin",
            (unsigned)rooms[room_index]) > 0);
        secondary[room_index] = read_file(path, 0);
        descriptors[room_index] = (GeDamRoomBlobDescriptor){
            rooms[room_index],
            {room->origin[0], room->origin[1], room->origin[2]},
            points[room_index].bytes, points[room_index].size,
            primary[room_index].bytes, primary[room_index].size,
            secondary[room_index].bytes, secondary[room_index].size,
        };
    }

    assert(ge_dam_rooms_build(descriptors, room_count, NULL, NULL, &scene)
        == GE_DAM_ROOM_CAPACITY_EXCEEDED);
    assert(scene.required_vertex_count > 0U);
    assert(scene.required_vertex_count <= SOURCE_VERTEX_CAPACITY);
    assert(scene.required_vertex_count <= PROJECTED_VERTEX_CAPACITY);
    vertices = malloc(scene.required_vertex_count * sizeof(*vertices));
    batches = malloc(scene.required_batch_count * sizeof(*batches));
    assert(vertices != NULL && batches != NULL);
    storage = (GeDamRoomSceneStorage){
        vertices, scene.required_vertex_count,
        batches, scene.required_batch_count,
    };
    assert(ge_dam_rooms_build(descriptors, room_count, NULL, &storage,
                              &scene) == GE_DAM_ROOM_OK);
    for (batch_index = 0U; batch_index < scene.batch_count; ++batch_index) {
        assert(batches[batch_index].first_vertex <= scene.vertex_count);
        assert(batches[batch_index].vertex_count
            <= scene.vertex_count - batches[batch_index].first_vertex);
    }
    texture_count = count_unique_textures(batches, scene.batch_count);
    assert(texture_count <= TEXTURE_CAPACITY);
    audit_floor_winding(vertices, batches, scene.batch_count,
                        scene.triangle_count);
    audit_background_depth_inheritance(batches, scene.batch_count);
    audit_texture_coordinates(vertices, batches, scene.batch_count);
    audit_camera_ray(vertices, batches, scene.batch_count);
    audit_spawn_clip_rejection(vertices,scene.vertex_count,
        batches,scene.batch_count);
    printf("Dam portal-selected scene: %zu room, %zu list, %zu command, "
           "%zu batch, %zu triangle, %zu vertex, %zu texture\n",
           scene.room_count, scene.list_count, scene.commands_visited,
           scene.batch_count, scene.triangle_count, scene.vertex_count,
           texture_count);

    free(batches);
    free(vertices);
    free(background.bytes);
    for (room_index = 0U; room_index < room_count; ++room_index) {
        free(secondary[room_index].bytes);
        free(primary[room_index].bytes);
        free(points[room_index].bytes);
    }
    return 0;
}
