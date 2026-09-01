#ifndef GE_ORIGINAL_RAREWARE_LOGO_H
#define GE_ORIGINAL_RAREWARE_LOGO_H

#include "ge_gbi_pipeline.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_ORIGINAL_RAREWARE_SEGMENT_BYTES 0x67f0U
#define GE_ORIGINAL_RAREWARE_FRONT_TRIANGLES 18U
#define GE_ORIGINAL_RAREWARE_LETTER_TRIANGLES 8U
#define GE_ORIGINAL_RAREWARE_BODY_TRIANGLES 242U

typedef enum GeOriginalRarewarePass {
    GE_ORIGINAL_RAREWARE_PASS_FRONT = 0,
    GE_ORIGINAL_RAREWARE_PASS_LETTERS = 1,
    GE_ORIGINAL_RAREWARE_PASS_BODY = 2,
    GE_ORIGINAL_RAREWARE_PASS_COUNT = 3
} GeOriginalRarewarePass;

typedef enum GeOriginalRarewarePrimitive {
    GE_ORIGINAL_RAREWARE_PRIMITIVE_PRIMARY = 0,
    GE_ORIGINAL_RAREWARE_PRIMITIVE_SECONDARY = 1
} GeOriginalRarewarePrimitive;

typedef struct GeOriginalRarewarePassDescriptor {
    uint32_t display_list_offset;
    uint32_t texture_offset;
    size_t command_count;
    size_t triangle_count;
    uint8_t primitive;
    uint8_t texture_generated;
} GeOriginalRarewarePassDescriptor;

typedef struct GeOriginalRarewareVertex {
    GeGbiVertex source;
    uint8_t texture_index;
} GeOriginalRarewareVertex;

typedef struct GeOriginalRarewareMesh {
    GeOriginalRarewarePass pass;
    size_t vertex_count;
    size_t triangle_count;
    size_t required_vertex_count;
    size_t commands_visited;
} GeOriginalRarewareMesh;

typedef enum GeOriginalRarewareStatus {
    GE_ORIGINAL_RAREWARE_OK = 0,
    GE_ORIGINAL_RAREWARE_INVALID_ARGUMENT,
    GE_ORIGINAL_RAREWARE_INVALID_SEGMENT,
    GE_ORIGINAL_RAREWARE_CAPACITY_EXCEEDED,
    GE_ORIGINAL_RAREWARE_PIPELINE_ERROR,
    GE_ORIGINAL_RAREWARE_UNEXPECTED_GEOMETRY
} GeOriginalRarewareStatus;

const GeOriginalRarewarePassDescriptor *
ge_original_rareware_pass_descriptor(GeOriginalRarewarePass pass);

GeOriginalRarewareStatus ge_original_rareware_mesh_build(
    const uint8_t *segment, size_t segment_size, GeOriginalRarewarePass pass,
    GeOriginalRarewareVertex *vertices, size_t vertex_capacity,
    GeOriginalRarewareMesh *mesh);

#ifdef __cplusplus
}
#endif

#endif
