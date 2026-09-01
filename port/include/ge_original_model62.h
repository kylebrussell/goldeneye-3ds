#ifndef GE_ORIGINAL_MODEL62_H
#define GE_ORIGINAL_MODEL62_H

#include <stddef.h>
#include <stdint.h>

#include <ultra64.h>
#include <bondtypes.h>

#include "ge_original_model62_runtime.h"

#define GE_ORIGINAL_MODEL62_NODE_COUNT 4U
#define GE_ORIGINAL_MODEL62_TEXTURE_COUNT 6U
#define GE_ORIGINAL_MODEL62_VERTEX_COUNT 74U

/* Pointer-safe native relocation of the exact decompressed PchrwppksilZ
 * resource. Geometry and display-list bytes remain owned by source_blob. */
struct GeOriginalModel62 {
    const uint8_t *source_blob;
    size_t source_size;
    ModelFileTextures textures[GE_ORIGINAL_MODEL62_TEXTURE_COUNT];
    ModelJoint joints[2];
    ModelSkeleton skeleton;
    ModelNode *switches[3];
    ModelNode nodes[GE_ORIGINAL_MODEL62_NODE_COUNT];
    union ModelRoData group_data;
    union ModelRoData bbox_data;
    union ModelRoData display_list_data;
    union ModelRoData gunfire_data;
    Vertex vertices[GE_ORIGINAL_MODEL62_VERTEX_COUNT];
    ModelFileHeader header;
    Model model;
    RenderPosView render_positions[1];
    uintptr_t rwdata_words[1];
};

GeOriginalModel62Status ge_original_model62_relocate(
    GeOriginalModel62 *runtime, const void *source_blob, size_t source_size);

#endif
