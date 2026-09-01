#ifndef GE_DAM_ROOM_H
#define GE_DAM_ROOM_H

#include "ge_gbi_pipeline.h"
#include "ge_pica_material.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_DAM_ROOM1_POINT_TABLE_PATH \
    "converted/levels/dam/room1/point_table.bin"
#define GE_DAM_ROOM1_PRIMARY_GDL_PATH \
    "converted/levels/dam/room1/primary_gdl.bin"

#define GE_DAM_ROOM1_POINT_TABLE_BYTES 576U
#define GE_DAM_ROOM1_PRIMARY_GDL_BYTES 176U
#define GE_DAM_ROOM1_SOURCE_VERTEX_COUNT 36U
#define GE_DAM_ROOM1_COMMAND_COUNT 22U
#define GE_DAM_ROOM1_DRAW_COUNT 7U
#define GE_DAM_ROOM1_TRIANGLE_COUNT 25U
#define GE_DAM_ROOM1_RENDER_VERTEX_COUNT (GE_DAM_ROOM1_TRIANGLE_COUNT * 3U)
#define GE_DAM_ROOM1_TEXTURE_ID 949U

#define GE_DAM_ROOM_DEFAULT_MAX_CALL_DEPTH 8U
#define GE_DAM_ROOM_DEFAULT_MAX_COMMANDS_PER_LIST 4096U

typedef struct GeDamRoomBlobs {
    const uint8_t *point_table;
    size_t point_table_size;
    const uint8_t *primary_gdl;
    size_t primary_gdl_size;
} GeDamRoomBlobs;

typedef struct GeDamRoomVertex {
    GeGbiVertex source;
    GeGbiProcessedVertex processed;
} GeDamRoomVertex;

typedef enum GeDamRoomStatus {
    GE_DAM_ROOM_OK = 0,
    GE_DAM_ROOM_INVALID_ARGUMENT,
    GE_DAM_ROOM_INVALID_BLOB_LAYOUT,
    GE_DAM_ROOM_PIPELINE_ERROR,
    GE_DAM_ROOM_UNEXPECTED_GEOMETRY,
    GE_DAM_ROOM_UNEXPECTED_MATERIAL,
    GE_DAM_ROOM_CAPACITY_EXCEEDED
} GeDamRoomStatus;

typedef enum GeDamRoomListKind {
    GE_DAM_ROOM_LIST_PRIMARY = 0,
    GE_DAM_ROOM_LIST_SECONDARY = 1
} GeDamRoomListKind;

typedef enum GeDamRoomCoordinateSpace {
    GE_DAM_ROOM_COORDINATE_AUTHORED = 0,
    GE_DAM_ROOM_COORDINATE_RUNTIME = 1,
    /* Canonical model matrices whose camera transform has already been
     * applied. Platform projection-only passes consume these coordinates
     * directly without an inverse view round trip. */
    GE_DAM_ROOM_COORDINATE_EYE = 2
} GeDamRoomCoordinateSpace;

typedef struct GeDamRoomBlobDescriptor {
    uint32_t room_id;
    float origin[3];
    const uint8_t *point_table;
    size_t point_table_size;
    const uint8_t *primary_gdl;
    size_t primary_gdl_size;
    const uint8_t *secondary_gdl;
    size_t secondary_gdl_size;
} GeDamRoomBlobDescriptor;

typedef struct GeDamRoomBuildLimits {
    size_t max_call_depth;
    size_t max_commands_per_list;
} GeDamRoomBuildLimits;

typedef struct GeDamRoomWorldVertex {
    GeGbiVertex source;
    GeGbiProcessedVertex processed;
    float world[3];
} GeDamRoomWorldVertex;

typedef struct GeDamRoomDrawBatch {
    uint32_t room_id;
    GeDamRoomListKind list_kind;
    GeGbiAddress command_address;
    GeGbiRareTextureState texture;
    GePicaMaterial material;
    size_t first_vertex;
    size_t vertex_count;
    size_t triangle_count;
    uint8_t texture_valid;
    uint8_t coordinate_space;
} GeDamRoomDrawBatch;

typedef struct GeDamRoomSceneStorage {
    GeDamRoomWorldVertex *vertices;
    size_t vertex_capacity;
    GeDamRoomDrawBatch *batches;
    size_t batch_capacity;
} GeDamRoomSceneStorage;

typedef struct GeDamRoomScene {
    GeDamRoomStatus status;
    size_t room_count;
    size_t list_count;
    size_t vertex_count;
    size_t batch_count;
    size_t triangle_count;
    size_t required_vertex_count;
    size_t required_batch_count;
    size_t commands_visited;
    size_t unsupported_commands;
} GeDamRoomScene;

typedef struct GeDamRoomModel {
    GeDamRoomStatus status;
    GeGbiPipelineResult pipeline;
    GeGbiRareTextureState texture;
    GePicaMaterial material;
    size_t vertex_count;
    GeDamRoomVertex vertices[GE_DAM_ROOM1_RENDER_VERTEX_COUNT];
} GeDamRoomModel;

/* Builds the first Dam background room using its original segment-0x0e
 * vertices and primary Fast3D display list. The result owns no blob pointers. */
GeDamRoomStatus ge_dam_room1_build(const GeDamRoomBlobs *blobs,
                                   GeDamRoomModel *model);

/*
 * Flattens all primary and optional secondary lists in descriptor order.
 * Each GBI draw action becomes one material batch and three world-space
 * vertices per triangle. Output buffers remain unpublished (counts stay zero)
 * on validation or capacity failure. Passing zero-capacity/null buffers is a
 * supported capacity query; required_* fields are still returned.
 */
GeDamRoomStatus ge_dam_rooms_build(
    const GeDamRoomBlobDescriptor *rooms,
    size_t room_count,
    const GeDamRoomBuildLimits *limits,
    const GeDamRoomSceneStorage *storage,
    GeDamRoomScene *scene);

const char *ge_dam_room_status_name(GeDamRoomStatus status);

#ifdef __cplusplus
}
#endif

#endif
