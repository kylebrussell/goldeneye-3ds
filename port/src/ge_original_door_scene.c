#include "ge_original_door_scene.h"

#include <string.h>

#include <bondtypes.h>
#include "ge_original_model178.h"

enum {
    GE_DOOR_MODEL_VERTEX_OFFSET = 0x0a8,
    GE_DOOR_MODEL_PRIMARY_LIST_OFFSET = 0x520,
    GE_DOOR_MODEL_COLLISION_RECORD_OFFSET = 0x500
};

static uint16_t read_be16(const uint8_t *bytes)
{
    return (uint16_t)(((uint16_t)bytes[0] << 8) | bytes[1]);
}

static uint32_t read_be32(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16)
        | ((uint32_t)bytes[2] << 8) | bytes[3];
}

static void write_be16(uint8_t *bytes, int16_t value)
{
    const uint16_t bits = (uint16_t)value;
    bytes[0] = (uint8_t)(bits >> 8);
    bytes[1] = (uint8_t)bits;
}

static void write_vertex(uint8_t *bytes, const Vertex *vertex)
{
    write_be16(bytes + 0U, vertex->coord.x);
    write_be16(bytes + 2U, vertex->coord.y);
    write_be16(bytes + 4U, vertex->coord.z);
    write_be16(bytes + 6U, vertex->index);
    write_be16(bytes + 8U, vertex->s);
    write_be16(bytes + 10U, vertex->t);
    bytes[12] = vertex->r;
    bytes[13] = vertex->g;
    bytes[14] = vertex->b;
    bytes[15] = vertex->a;
}

static int valid_model178_layout(const uint8_t *blob, size_t size)
{
    return blob != NULL && size == GE_ORIGINAL_MODEL178_BLOB_SIZE
        && read_be32(blob + GE_DOOR_MODEL_COLLISION_RECORD_OFFSET)
            == UINT32_C(0x05000520)
        && read_be16(blob + GE_DOOR_MODEL_COLLISION_RECORD_OFFSET + 12U)
            == GE_ORIGINAL_MODEL178_VERTEX_COUNT
        && GE_DOOR_MODEL_VERTEX_OFFSET
                + GE_ORIGINAL_MODEL178_VERTEX_COUNT * 16U <= size;
}

GeOriginalDoorSceneStatus ge_original_door_scene_prepare(
    const void *door_definition, const void *model_blob,
    size_t model_blob_size, GeOriginalDoorScenePublication *publication)
{
    const uint8_t *source = model_blob;
    size_t index;

    if (publication == NULL) return GE_ORIGINAL_DOOR_SCENE_INVALID_ARGUMENT;
    memset(publication, 0, sizeof(*publication));
    publication->status = GE_ORIGINAL_DOOR_SCENE_INVALID_ARGUMENT;
    if (door_definition == NULL || source == NULL)
        return publication->status;
    if (!valid_model178_layout(source, model_blob_size)) {
        publication->status = GE_ORIGINAL_DOOR_SCENE_INVALID_MODEL_LAYOUT;
        return publication->status;
    }
    if (!ge_original_door_runtime_snapshot(
            door_definition, &publication->runtime)) {
        publication->status = GE_ORIGINAL_DOOR_SCENE_SNAPSHOT_UNAVAILABLE;
        return publication->status;
    }
    memcpy(publication->blob, source, sizeof(publication->blob));
    if (publication->runtime.clipped_vertices != NULL) {
        const Vertex *vertices = publication->runtime.clipped_vertices;
        if (publication->runtime.clipped_vertex_count
                    != GE_ORIGINAL_MODEL178_VERTEX_COUNT
                || publication->runtime.clipped_vertex_stride
                    != sizeof(Vertex)) {
            publication->status =
                GE_ORIGINAL_DOOR_SCENE_INVALID_VERTEX_PUBLICATION;
            return publication->status;
        }
        for (index = 0U; index < GE_ORIGINAL_MODEL178_VERTEX_COUNT; ++index)
            write_vertex(publication->blob + GE_DOOR_MODEL_VERTEX_OFFSET
                         + index * 16U, &vertices[index]);
        publication->uses_clipped_vertices = UINT8_C(1);
    } else if (publication->runtime.clipped_vertex_count != 0U) {
        publication->status =
            GE_ORIGINAL_DOOR_SCENE_INVALID_VERTEX_PUBLICATION;
        return publication->status;
    }
    publication->input.blob = publication->blob;
    publication->input.blob_size = sizeof(publication->blob);
    publication->input.primary_offset = GE_DOOR_MODEL_PRIMARY_LIST_OFFSET;
    publication->input.secondary_offset = GE_ORIGINAL_MODEL_SCENE_NO_LIST;
    publication->input.segment4_offset = GE_DOOR_MODEL_VERTEX_OFFSET;
    publication->input.room_id = (uint32_t)(uint16_t)publication->runtime.room;
    memcpy(publication->input.matrix, publication->runtime.matrix,
           sizeof(publication->input.matrix));
    memcpy(publication->input.position, publication->runtime.position,
           sizeof(publication->input.position));
    publication->status = GE_ORIGINAL_DOOR_SCENE_OK;
    return publication->status;
}

const char *ge_original_door_scene_status_name(
    GeOriginalDoorSceneStatus status)
{
    switch (status) {
    case GE_ORIGINAL_DOOR_SCENE_OK: return "ok";
    case GE_ORIGINAL_DOOR_SCENE_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_DOOR_SCENE_SNAPSHOT_UNAVAILABLE:
        return "snapshot unavailable";
    case GE_ORIGINAL_DOOR_SCENE_INVALID_MODEL_LAYOUT:
        return "invalid model layout";
    case GE_ORIGINAL_DOOR_SCENE_INVALID_VERTEX_PUBLICATION:
        return "invalid vertex publication";
    default: return "unknown";
    }
}
