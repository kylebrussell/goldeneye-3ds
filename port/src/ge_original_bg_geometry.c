#include "ge_original_bg_geometry_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

GeOriginalBgGeometry *ge_bg_geometry_current;

typedef struct GeBgScan {
    GeOriginalBgGeometry *geometry;
    uint32_t room;
    unsigned list;
    uint32_t points[16];
    uint16_t valid;
    int ok;
} GeBgScan;

static void *allocate(GeOriginalBgGeometry *geometry, size_t count, size_t size)
{
    void *memory;
    if (count == 0U || size == 0U) return NULL;
    if (count > GE_BG_GEOMETRY_MAX_BYTES / size
            || count * size > GE_BG_GEOMETRY_MAX_BYTES - geometry->allocated_bytes) {
        geometry->status = GE_ORIGINAL_BG_GEOMETRY_CAPACITY;
        return NULL;
    }
    memory = calloc(count, size);
    if (memory == NULL) geometry->status = GE_ORIGINAL_BG_GEOMETRY_NO_MEMORY;
    else geometry->allocated_bytes += count * size;
    return memory;
}

void *ge_bg_bounds_allocate(size_t size)
{ return allocate(ge_bg_geometry_current, 1U, size); }

static int copy_bytes(GeOriginalBgGeometry *geometry, const uint8_t *source,
    size_t size, const uint8_t **destination)
{
    uint8_t *copy;
    *destination = NULL;
    if (size == 0U) return source == NULL;
    if (source == NULL) return 0;
    copy = allocate(geometry, size, 1U);
    if (copy == NULL) return 0;
    memcpy(copy, source, size);
    *destination = copy;
    return 1;
}

static int command_index(const uint8_t *base, size_t count, const void *command,
    size_t *index)
{
    const uintptr_t start = (uintptr_t)base, address = (uintptr_t)command;
    if (base == NULL || command == NULL || address < start
            || address - start >= count * 8U || (address - start) % 8U != 0U)
        return 0;
    *index = (address - start) / 8U;
    return 1;
}

const GeOriginalBgCommandInfo *ge_bg_command_info(int32_t room, const Gfx *gfx)
{
    GeOriginalBgRoomGeometry *geometry;
    size_t index;
    if (ge_bg_geometry_current == NULL || room <= 0
            || (size_t)room >= ge_bg_geometry_current->room_count) return NULL;
    geometry = &ge_bg_geometry_current->room_geometry[room];
    if (command_index(geometry->blobs.primary_gdl, geometry->command_counts[0], gfx, &index))
        return &geometry->commands[0][index];
    if (command_index(geometry->blobs.secondary_gdl, geometry->command_counts[1], gfx, &index))
        return &geometry->commands[1][index];
    return NULL;
}

static int scan_event(const GeGbiPipelineEvent *event, void *user)
{
    GeBgScan *scan = user;
    GeOriginalBgRoomGeometry *room = &scan->geometry->room_geometry[scan->room];
    GeOriginalBgCommandInfo *info;
    if (event->command_address.segment != 13U || event->call_depth != 0U
            || event->command_address.offset % 8U != 0U
            || event->command_address.offset / 8U >= room->command_counts[scan->list])
        return scan->ok = 0;
    if (event->action.kind == GE_GBI_STATE_ACTION_LOAD_VERTICES) {
        const size_t first = event->action.data.vertices.first;
        const size_t count = event->action.data.vertices.count;
        const GeGbiAddress address = event->action.data.vertices.address;
        if (address.segment != 14U || address.offset % 16U != 0U
                || first + count > 16U || address.offset / 16U + count > room->vertex_count)
            return scan->ok = 0;
        for (size_t i = 0U; i < count; ++i) {
            scan->points[first + i] = (uint32_t)(address.offset / 16U + i);
            scan->valid |= (uint16_t)(1U << (first + i));
        }
    }
    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) return 1;
    info = &room->commands[scan->list][event->command_address.offset / 8U];
    info->texture = -1;
    info->tile_format = info->tile_size = -1;
    info->triangle_count = event->action.data.draw.count;
    if (info->triangle_count > 4U) return scan->ok = 0;
    if (event->state->active_texture_binding == GE_GBI_TEXTURE_BINDING_RARE_ID) {
        const uint16_t texture = event->state->rare_texture.texture_id;
        if (texture >= GE_BG_TEXTURE_COUNT) return scan->ok = 0;
        info->texture = (int16_t)texture;
    } else if (event->state->texture_image_valid) {
        /* A raw physical RDP image pointer lacks original allocation-prefix
         * provenance. Authored room streams use Rare IDs; reject other data. */
        return scan->ok = 0;
    }
    for (size_t triangle = 0U; triangle < info->triangle_count; ++triangle) {
        for (size_t vertex = 0U; vertex < 3U; ++vertex) {
            const unsigned slot = event->action.data.draw.triangles[triangle].vertex[vertex];
            if (slot >= 16U || (scan->valid & (1U << slot)) == 0U) return scan->ok = 0;
            info->point_indices[triangle * 3U + vertex] = scan->points[slot];
        }
    }
    return 1;
}

static int execute_list(const GeDamRoomBlobDescriptor *room, unsigned list,
    GeGbiPipelineCallback callback, void *context)
{
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig limits = {8U, 32768U};
    GeGbiPipelineResult result;
    const uint8_t *gdl = list == 0U ? room->primary_gdl : room->secondary_gdl;
    const size_t bytes = list == 0U ? room->primary_gdl_size : room->secondary_gdl_size;
    if (gdl == NULL && bytes == 0U) return 1;
    ge_gbi_memory_map_init(&memory);
    if ((room->point_table_size != 0U && ge_gbi_memory_map_set_segment(&memory, 14U,
                room->point_table, room->point_table_size) != GE_GBI_RESOLVE_OK)
            || ge_gbi_memory_map_set_segment(&memory, 13U, gdl, bytes) != GE_GBI_RESOLVE_OK)
        return 0;
    result = ge_gbi_pipeline_execute(&memory,
        (GeGbiAddress){UINT32_C(0x0d000000), 0U, 13U},
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &limits, callback, context);
    return result.status == GE_GBI_PIPELINE_OK && result.unsupported_commands == 0U;
}

static int validate_list(GeOriginalBgGeometry *geometry, uint32_t id, unsigned list)
{
    GeOriginalBgRoomGeometry *room = &geometry->room_geometry[id];
    const uint8_t *gdl = list == 0U ? room->blobs.primary_gdl : room->blobs.secondary_gdl;
    const size_t bytes = list == 0U ? room->blobs.primary_gdl_size : room->blobs.secondary_gdl_size;
    size_t count = 0U;
    uint32_t base = 0U, first = 0U;
    int have_vertices = 0, ended = 0;
    if (bytes == 0U) return list == 1U && gdl == NULL;
    if (gdl == NULL || bytes % 8U != 0U || bytes / 8U > INT16_MAX) return 0;
    for (count = 0U; count < bytes / 8U; ++count) {
        const Gfx *command = ge_bg_gfx_at(gdl, (ptrdiff_t)count);
        const uint8_t opcode = gdl[count * 8U];
        if (opcode == 0xb8U) { ++count; ended = 1; break; }
        /* Original bgBuildRoomVtxBounds walks this primary list linearly.
         * Never approximate a called/matrix-transformed room as a flat list. */
        if (opcode == 0x06U || opcode == 0x01U) return 0;
        /* Authored room streams carry Rare IDs, not expanded RDP pointer/
         * tile commands. Reject a different format instead of guessing its
         * physical image allocation or final texLoadFromGdl tile state. */
        if (opcode == 0xfdU || opcode == 0xf5U) return 0;
        if (opcode == 0xc0U) {
            const uint32_t word = ge_bg_gfx_word(command, 1U);
            const uint32_t type = ge_bg_gfx_word(command, 0U) & 7U;
            if (type > 4U || (word & 0xfffU) >= GE_BG_TEXTURE_COUNT
                    || (type == 1U && ((word >> 12) & 0xfffU) >= GE_BG_TEXTURE_COUNT))
                return 0;
        }
        if (opcode == 0x04U) {
            const uint32_t address = ge_bg_gfx_word(command, 1U);
            const size_t number = (gdl[count * 8U + 1U] >> 4) + 1U;
            first = gdl[count * 8U + 1U] & 0xfU;
            base = (address & 0xffffffU) / 16U;
            if (address >> 24 != 14U || address % 16U != 0U
                    || first + number > 16U || base + number > room->vertex_count)
                return 0;
            have_vertices = 1;
        } else if (opcode == 0xbfU || opcode == 0xb1U) {
            GeGbiCommand decoded;
            if (!have_vertices || ge_gbi_decode_command((const uint8_t *)command, 8U,
                    GE_GBI_BYTE_ORDER_BIG_ENDIAN, 0U, &decoded) != GE_GBI_STATUS_OK) return 0;
            for (size_t triangle = 0U; triangle < decoded.data.geometry.count; ++triangle) {
                const uint8_t *indices = decoded.data.geometry.triangles[triangle].vertex;
                if (indices[0] == indices[1] || indices[1] == indices[2] || indices[0] == indices[2])
                    continue;
                for (size_t i = 0U; i < 3U; ++i) {
                    const int64_t index = (int64_t)base + indices[i] - first;
                    if (index < 0 || (uint64_t)index >= room->vertex_count) return 0;
                }
            }
        }
    }
    if (!ended) return 0;
    room->command_counts[list] = count;
    room->commands[list] = allocate(geometry, count, sizeof(*room->commands[list]));
    if (room->commands[list] == NULL) return 0;
    GeBgScan scan = {.geometry = geometry, .room = id, .list = list, .ok = 1};
    if (!execute_list(&room->blobs, list, scan_event, &scan) || !scan.ok) return 0;
    ge_bg_texture_emission_begin();
    int8_t format = -1, depth = -1;
    for (size_t i = 0U; i < count; ++i) {
        const Gfx *command = ge_bg_gfx_at(gdl, (ptrdiff_t)i);
        if (((const uint8_t *)command)[0] == 0xc0U
                && !ge_bg_texture_emission_command(ge_bg_gfx_word(command,0U),
                    ge_bg_gfx_word(command,1U),&format,&depth)) return 0;
        room->commands[list][i].tile_format = format;
        room->commands[list][i].tile_size = depth;
    }
    return 1;
}

static int register_fixtures(GeOriginalBgGeometry *geometry, uint32_t id, size_t *total)
{
    GeOriginalBgRoomGeometry *room = &geometry->room_geometry[id];
    ge_bg_light_room_exact((int32_t)id);
    for (unsigned list = 0U; list < 2U; ++list) {
        const uint8_t *gdl = list == 0U ? room->blobs.primary_gdl : room->blobs.secondary_gdl;
        int pending = 0;
        uint32_t vertex_base = 0U;
        int have_vertices = 0;
        for (size_t i = 0U; i < room->command_counts[list]; ++i) {
            Gfx *command = ge_bg_gfx_at(gdl, (ptrdiff_t)i);
            const uint8_t opcode = ((uint8_t *)command)[0];
            if (opcode == 0x04U) {
                vertex_base = (ge_bg_gfx_word(command, 1U) & 0xffffffU) / 16U;
                have_vertices = 1;
            }
            if (pending && (opcode == 0xbfU || opcode == 0xb1U)) {
                /* The original light body scans back to the last VTX and
                 * uses raw cache indices (including TRI4 padding). Validate
                 * that exact access, separately from BG's vtxoff subtraction. */
                if (!have_vertices) return 0;
                for (unsigned type = opcode == 0xbfU ? 0U : 1U;
                        type <= (opcode == 0xbfU ? 0U : 4U); ++type) {
                    s32 indices[3];
                    ge_bg_light_indices_exact(command, type, &indices[0], &indices[1], &indices[2]);
                    for (size_t v = 0U; v < 3U; ++v)
                        if (indices[v] < 0 || vertex_base + (uint32_t)indices[v] >= room->vertex_count)
                            return 0;
                }
            }
            if (((uint8_t *)command)[0] != 0xc0U) continue;
            if (pending) { ge_bg_light_end_exact(command); pending = 0; }
            if (ge_bg_is_light_exact((int32_t)(ge_bg_gfx_word(command, 1U) & 0xfffU))) {
                ge_bg_light_begin_exact(ge_bg_gfx_at(command, 1));
                if (geometry->fixture_slot >= (int16_t)GE_BG_LIGHT_FIXTURES) {
                    geometry->status = GE_ORIGINAL_BG_GEOMETRY_CAPACITY;
                    return 0;
                }
                pending = 1;
                ++*total;
            }
        }
        if (pending) ge_bg_light_end_exact(ge_bg_gfx_at(gdl, (ptrdiff_t)room->command_counts[list]));
    }
    return 1;
}

GeOriginalBgGeometry *ge_original_bg_geometry_open_rooms(
    const GeDamRoomBlobDescriptor *descriptors, size_t descriptor_count,
    const GeOriginalBgRoomBounds *bounds, size_t room_count, float level_scale,
    GeOriginalBgGeometryReport *report)
{
    GeOriginalBgGeometryReport result = {.status = GE_ORIGINAL_BG_GEOMETRY_INVALID_ARGUMENT};
    GeOriginalBgGeometry *geometry = NULL, *previous = ge_bg_geometry_current;
    if (descriptors == NULL || bounds == NULL || room_count < 2U
            || room_count > GE_BG_GEOMETRY_MAX_ROOMS || descriptor_count != room_count - 1U
            || !isfinite(level_scale) || level_scale <= 0.0f || !isfinite(1.0f / level_scale))
        goto finish;
    geometry = calloc(1U, sizeof(*geometry));
    if (geometry == NULL) { result.status = GE_ORIGINAL_BG_GEOMETRY_NO_MEMORY; goto finish; }
    geometry->room_count = room_count;
    geometry->level_scale = level_scale;
    geometry->inverse_scale = 1.0f / level_scale;
    geometry->status = GE_ORIGINAL_BG_GEOMETRY_OK;
    ge_original_bg_geometry_bind(geometry);
    ge_bg_light_init_exact();
    for (size_t i = 0U; i < descriptor_count; ++i) {
        const GeDamRoomBlobDescriptor *source = &descriptors[i];
        const uint32_t id = source->room_id;
        GeOriginalBgRoomGeometry *room;
        result.failed_room = id;
        if (id == 0U || id >= room_count || geometry->room_geometry[id].blobs.primary_gdl != NULL)
            goto invalid;
        room = &geometry->room_geometry[id];
        room->blobs = *source;
        room->blobs.point_table = room->blobs.primary_gdl = room->blobs.secondary_gdl = NULL;
        if (source->point_table_size % 16U != 0U || source->point_table_size / 16U > UINT16_MAX)
            goto invalid;
        for (size_t axis = 0U; axis < 3U; ++axis) {
            if (!isfinite(source->origin[axis]) || fabsf(source->origin[axis]) > 1000000.0f
                    || !isfinite(bounds[id].minimum[axis]) || !isfinite(bounds[id].maximum[axis])
                    || bounds[id].minimum[axis] > bounds[id].maximum[axis]) goto invalid;
            geometry->origins[id].f[axis] = source->origin[axis];
            geometry->rooms[id].minbounds.f[axis] = bounds[id].minimum[axis];
            geometry->rooms[id].maxbounds.f[axis] = bounds[id].maximum[axis];
        }
        if (!copy_bytes(geometry, source->point_table, source->point_table_size, &room->blobs.point_table)
                || !copy_bytes(geometry, source->primary_gdl, source->primary_gdl_size, &room->blobs.primary_gdl)
                || !copy_bytes(geometry, source->secondary_gdl, source->secondary_gdl_size, &room->blobs.secondary_gdl))
            goto invalid;
        room->vertex_count = source->point_table_size / 16U;
        if (room->vertex_count != 0U) {
            geometry->rooms[id].vertices = allocate(geometry, room->vertex_count, sizeof(Vtx));
            room->collision_vertices = allocate(geometry, room->vertex_count, sizeof(Vertex));
            if (geometry->rooms[id].vertices == NULL || room->collision_vertices == NULL) goto invalid;
            for (size_t vertex = 0U; vertex < room->vertex_count; ++vertex) {
                const uint8_t *input = room->blobs.point_table + vertex * 16U;
                Vtx *output = &geometry->rooms[id].vertices[vertex];
                for (size_t axis = 0U; axis < 3U; ++axis)
                    output->v.ob[axis] = (int16_t)(((uint16_t)input[axis * 2U] << 8) | input[axis * 2U + 1U]);
                output->v.flag = (uint16_t)(((uint16_t)input[6] << 8) | input[7]);
                output->v.tc[0] = (int16_t)(((uint16_t)input[8] << 8) | input[9]);
                output->v.tc[1] = (int16_t)(((uint16_t)input[10] << 8) | input[11]);
                memcpy(output->v.cn, input + 12U, 4U);
                for (size_t axis = 0U; axis < 3U; ++axis)
                    room->collision_vertices[vertex].coord.AsArray[axis] = output->v.ob[axis];
                room->collision_vertices[vertex].index = (int16_t)output->v.flag;
                room->collision_vertices[vertex].s = output->v.tc[0];
                room->collision_vertices[vertex].t = output->v.tc[1];
                memcpy(&room->collision_vertices[vertex].r, output->v.cn, 4U);
            }
        }
        geometry->rooms[id].ptr_expanded_mapping_info = (void *)room->blobs.primary_gdl;
        geometry->rooms[id].ptr_secondary_expanded_mapping_info = (void *)room->blobs.secondary_gdl;
        if (!validate_list(geometry, id, 0U) || !validate_list(geometry, id, 1U)) goto invalid;
        ge_bg_build_bounds_exact((int32_t)id);
        if (geometry->status != GE_ORIGINAL_BG_GEOMETRY_OK) goto invalid;
        ++result.rooms;
        result.vertices += room->vertex_count;
        result.commands += room->command_counts[0] + room->command_counts[1];
        result.batches += (size_t)geometry->rooms[id].num_vtx_batch_bounds;
    }
    /* Canonical first-fit table registration uses authored room order, not
     * descriptor order supplied by a caller. */
    for (uint32_t id = 1U; id < room_count; ++id) {
        result.failed_room = id;
        if (!register_fixtures(geometry, id, &result.fixtures)) goto invalid;
    }
    result.status = GE_ORIGINAL_BG_GEOMETRY_OK;
    result.failed_room = 0U;
    goto finish;
invalid:
    result.status = geometry->status == GE_ORIGINAL_BG_GEOMETRY_OK
        ? GE_ORIGINAL_BG_GEOMETRY_INVALID_ASSET : geometry->status;
    ge_original_bg_geometry_close(geometry);
    geometry = NULL;
finish:
    ge_original_bg_geometry_bind(previous);
    if (report != NULL) *report = result;
    return geometry;
}

void ge_original_bg_geometry_bind(GeOriginalBgGeometry *geometry)
{ ge_bg_geometry_current = geometry; }

size_t ge_original_bg_geometry_room_count(void)
{ return ge_bg_geometry_current != NULL ? ge_bg_geometry_current->room_count : 0U; }

s_room_info *ge_original_bg_geometry_native_rooms(void)
{ return ge_bg_geometry_current != NULL ? ge_bg_geometry_current->rooms : NULL; }

GeOriginalBgGeometryStatus ge_original_bg_geometry_status(const GeOriginalBgGeometry *geometry)
{ return geometry != NULL ? geometry->status : GE_ORIGINAL_BG_GEOMETRY_NOT_BOUND; }

void ge_original_bg_geometry_close(GeOriginalBgGeometry *geometry)
{
    if (geometry == NULL) return;
    if (ge_bg_geometry_current == geometry) ge_bg_geometry_current = NULL;
    for (size_t i = 0U; i < GE_BG_GEOMETRY_MAX_ROOMS; ++i) {
        free(geometry->rooms[i].vertices);
        free(geometry->room_geometry[i].collision_vertices);
        free(geometry->rooms[i].vtx_batch_bounds);
        free((void *)geometry->room_geometry[i].blobs.point_table);
        free((void *)geometry->room_geometry[i].blobs.primary_gdl);
        free((void *)geometry->room_geometry[i].blobs.secondary_gdl);
        free(geometry->room_geometry[i].commands[0]);
        free(geometry->room_geometry[i].commands[1]);
    }
    free(geometry);
}

bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 room, HitThing *hit)
{
    GeOriginalBgGeometry *geometry = ge_bg_geometry_current;
    if (geometry == NULL || from == NULL || to == NULL || hit == NULL
            || room <= 0 || (size_t)room >= geometry->room_count
            || geometry->status != GE_ORIGINAL_BG_GEOMETRY_OK) return FALSE;
    for (size_t i = 0U; i < 3U; ++i)
        if (!isfinite(from->f[i]) || !isfinite(to->f[i])
                || fabsf(from->f[i] * geometry->level_scale) > 1000000.0f
                || fabsf(to->f[i] * geometry->level_scale) > 1000000.0f) {
            geometry->status = GE_ORIGINAL_BG_GEOMETRY_INVALID_ARGUMENT;
            return FALSE;
        }
    return ge_bg_bullet_hit_exact(from, to, room, hit);
}

int ge_original_bg_geometry_hit(GeOriginalBgGeometry *geometry,
    const float from[3], const float to[3], int32_t room, GeOriginalBgGeometryHit *hit)
{
    coord3d start, end;
    HitThing source;
    if (geometry == NULL || ge_bg_geometry_current != geometry || from == NULL || to == NULL || hit == NULL)
        return 0;
    memcpy(start.f, from, sizeof(start.f));
    memcpy(end.f, to, sizeof(end.f));
    if (!bgTestBulletHitBackground(&start, &end, room, &source)) return 0;
    memset(hit, 0, sizeof(*hit));
    memcpy(hit->position, source.hitpos.f, sizeof(hit->position));
    memcpy(hit->normal, source.normal.f, sizeof(hit->normal));
    hit->texture = source.texturenum;
    hit->tile_format = source.tileformat;
    hit->tile_size = source.tilesize;
    hit->triangle = source.tricmd;
    hit->triangle_type = (uint32_t)source.unk28;
    return 1;
}

int ge_original_bg_geometry_light_break(void *context, void *triangle,
    uint32_t type, int32_t room_id)
{
    GeOriginalBgGeometry *geometry = context;
    GeOriginalBgRoomGeometry *room;
    size_t command;
    if (geometry == NULL || geometry != ge_bg_geometry_current || room_id <= 0
            || (size_t)room_id >= geometry->room_count || type > 4U
            || geometry->status != GE_ORIGINAL_BG_GEOMETRY_OK) return 0;
    room = &geometry->room_geometry[room_id];
    if (!command_index(room->blobs.primary_gdl, room->command_counts[0], triangle, &command)
            || room->commands[0][command].triangle_count == 0U
            || !ge_bg_is_light_exact(room->commands[0][command].texture)
            || (type == 0U ? ((uint8_t *)triangle)[0] != 0xbfU
                : ((uint8_t *)triangle)[0] != 0xb1U)) return 0;
    s32 indices[3];
    ge_bg_light_indices_exact(triangle, type, &indices[0], &indices[1], &indices[2]);
    if (indices[0] == indices[1] || indices[1] == indices[2] || indices[0] == indices[2]) return 0;
    ge_bg_light_break_exact(triangle, type, room_id);
    for (size_t i = 0U; i < room->vertex_count; ++i) {
        memcpy((uint8_t *)room->blobs.point_table + i * 16U + 12U,
            geometry->rooms[room_id].vertices[i].v.cn, 4U);
        memcpy(&room->collision_vertices[i].r, geometry->rooms[room_id].vertices[i].v.cn, 4U);
    }
    return 1;
}

GeOriginalBgGeometry *ge_original_bg_geometry_load(
    GeAssetPack *pack, const GeStageAssetDescriptor *stage,
    const GeDamWorld *world, const GeOriginalBgRoomBounds *bounds,
    GeOriginalBgGeometryReport *report)
{
    GeDamRoomBlobDescriptor *rooms = NULL;
    GeOriginalBgGeometry *geometry = NULL;
    GeOriginalBgGeometryReport result = {.status = GE_ORIGINAL_BG_GEOMETRY_INVALID_ARGUMENT};
    size_t bytes_allocated = 0U;
    if (pack == NULL || stage == NULL || world == NULL || bounds == NULL
            || world->room_count < 2U || world->room_count > GE_BG_GEOMETRY_MAX_ROOMS
            || world->room_count != stage->expected_room_count) goto finish;
    rooms = calloc(world->room_count - 1U, sizeof(*rooms));
    if (rooms == NULL) { result.status = GE_ORIGINAL_BG_GEOMETRY_NO_MEMORY; goto finish; }
    for (size_t room = 1U; room < world->room_count; ++room) {
        GeDamRoomBlobDescriptor *destination = &rooms[room - 1U];
        destination->room_id = (uint32_t)room;
        memcpy(destination->origin, world->rooms[room].origin, sizeof(destination->origin));
        result.failed_room = (uint32_t)room;
        for (unsigned stream = 0U; stream < 3U; ++stream) {
            char path[GE_STAGE_ASSET_PATH_CAPACITY];
            const GeAssetPackEntry *entry;
            const uint8_t **data = stream == 0U ? &destination->point_table
                : stream == 1U ? &destination->primary_gdl : &destination->secondary_gdl;
            size_t *size = stream == 0U ? &destination->point_table_size
                : stream == 1U ? &destination->primary_gdl_size : &destination->secondary_gdl_size;
            size_t read = 0U;
            if (ge_stage_asset_room_path(stage, (uint8_t)room, (GeStageRoomStream)stream,
                    path, sizeof(path)) != GE_STAGE_ASSET_OK) goto invalid;
            entry = ge_asset_pack_find(pack, path);
            if (entry == NULL || entry->data_size == 0U) {
                if (stream == 1U) goto invalid;
                continue;
            }
            if (entry->data_size > GE_BG_GEOMETRY_MAX_BYTES - bytes_allocated) {
                result.status = GE_ORIGINAL_BG_GEOMETRY_CAPACITY;
                goto finish;
            }
            *size = (size_t)entry->data_size;
            *data = malloc(*size);
            if (*data == NULL) { result.status = GE_ORIGINAL_BG_GEOMETRY_NO_MEMORY; goto finish; }
            bytes_allocated += *size;
            if (ge_asset_pack_read(pack, path, (void *)*data, *size, &read) != GE_ASSET_PACK_OK
                    || read != *size) goto invalid;
        }
    }
    geometry = ge_original_bg_geometry_open_rooms(rooms, world->room_count - 1U,
        bounds, world->room_count, stage->level_scale, &result);
    goto finish;
invalid:
    result.status = GE_ORIGINAL_BG_GEOMETRY_INVALID_ASSET;
finish:
    if (rooms != NULL) {
        for (size_t i = 0U; i + 1U < world->room_count; ++i) {
            free((void *)rooms[i].point_table);
            free((void *)rooms[i].primary_gdl);
            free((void *)rooms[i].secondary_gdl);
        }
        free(rooms);
    }
    if (report != NULL) *report = result;
    return geometry;
}

typedef struct GeBgSceneUpdate {
    GeOriginalBgGeometry *geometry;
    GeDamRoomWorldVertex *vertices;
    const GeDamRoomDrawBatch *batches;
    size_t batch_count;
    size_t changed_vertices;
    uint32_t room;
    unsigned list;
} GeBgSceneUpdate;

static int update_scene_event(const GeGbiPipelineEvent *event, void *context)
{
    GeBgSceneUpdate *update = context;
    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TRIANGLES) return 1;
    for (size_t batch_index = 0U; batch_index < update->batch_count; ++batch_index) {
        const GeDamRoomDrawBatch *batch = &update->batches[batch_index];
        if (batch->room_id != update->room || batch->list_kind != (GeDamRoomListKind)update->list
                || batch->command_address.offset != event->command_address.offset) continue;
        size_t output = batch->first_vertex;
        for (size_t triangle = 0U; triangle < event->action.data.draw.count; ++triangle) {
            for (size_t vertex = 0U; vertex < 3U; ++vertex) {
                const uint8_t slot = event->action.data.draw.triangles[triangle].vertex[vertex];
                GeDamRoomWorldVertex *destination = &update->vertices[output++];
                if (memcmp(&destination->source.red, &event->vertex_cache[slot].red, 4U) != 0)
                    ++update->changed_vertices;
                /* Exact existing GBI shade path, including any authored
                 * lighting/alpha state; no renderer-created brightness rule. */
                destination->source = event->vertex_cache[slot];
                destination->processed = event->processed_vertex_cache[slot];
            }
        }
    }
    return 1;
}

int ge_original_bg_geometry_apply_scene(GeOriginalBgGeometry *geometry,
    GeDamRoomWorldVertex *vertices, size_t vertex_count,
    const GeDamRoomDrawBatch *batches, size_t batch_count,
    size_t *changed_vertices)
{
    uint8_t dirty[GE_BG_GEOMETRY_MAX_ROOMS][2] = {{0}};
    if (changed_vertices != NULL) *changed_vertices = 0U;
    if (geometry == NULL || (vertex_count != 0U && vertices == NULL)
            || (batch_count != 0U && batches == NULL)
            || geometry->status != GE_ORIGINAL_BG_GEOMETRY_OK) return 0;
    /* Validate all borrowed ranges and immutable provenance before writing. */
    for (size_t i = 0U; i < batch_count; ++i) {
        const GeDamRoomDrawBatch *batch = &batches[i];
        const GeOriginalBgCommandInfo *info;
        const GeOriginalBgRoomGeometry *room;
        if (batch->room_id == 0U || batch->room_id >= geometry->room_count
                || (unsigned)batch->list_kind > 1U || batch->command_address.segment != 13U
                || batch->coordinate_space != GE_DAM_ROOM_COORDINATE_AUTHORED
                || batch->first_vertex > vertex_count || batch->vertex_count > vertex_count - batch->first_vertex
                || batch->command_address.offset % 8U != 0U) return 0;
        room = &geometry->room_geometry[batch->room_id];
        if (batch->command_address.offset / 8U >= room->command_counts[batch->list_kind]) return 0;
        info = &room->commands[batch->list_kind][batch->command_address.offset / 8U];
        if (batch->vertex_count != (size_t)info->triangle_count * 3U) return 0;
        for (size_t v = 0U; v < batch->vertex_count; ++v) {
            const Vtx *canonical = &geometry->rooms[batch->room_id].vertices[info->point_indices[v]];
            const GeGbiVertex *source = &vertices[batch->first_vertex + v].source;
            if (source->x != canonical->v.ob[0] || source->y != canonical->v.ob[1]
                    || source->z != canonical->v.ob[2] || source->flag != canonical->v.flag
                    || source->texture_s != canonical->v.tc[0] || source->texture_t != canonical->v.tc[1])
                return 0;
            if (memcmp(&source->red, canonical->v.cn, 4U) != 0)
                dirty[batch->room_id][batch->list_kind] = 1U;
        }
    }
    GeBgSceneUpdate update = {.geometry = geometry, .vertices = vertices,
        .batches = batches, .batch_count = batch_count};
    for (uint32_t room = 1U; room < geometry->room_count; ++room) {
        update.room = room;
        for (unsigned list = 0U; list < 2U; ++list) {
            update.list = list;
            if (dirty[room][list] && !execute_list(&geometry->room_geometry[room].blobs,
                    list, update_scene_event, &update)) return 0;
        }
    }
    if (changed_vertices != NULL) *changed_vertices = update.changed_vertices;
    return 1;
}
