#ifndef GE_ORIGINAL_MODEL178_H
#define GE_ORIGINAL_MODEL178_H

#include <ultra64.h>
#include <bondtypes.h>
#include "ge_original_model178_runtime.h"

#define GE_ORIGINAL_MODEL178_NODE_COUNT 3U
#define GE_ORIGINAL_MODEL178_VERTEX_COUNT 44U
#define GE_ORIGINAL_MODEL178_COLLISION_VERTEX_COUNT 20U

struct GeOriginalModel178 {
    const uint8_t *source_blob;
    size_t source_size;
    ModelFileTextures textures[2];
    ModelJoint joints[2];
    ModelSkeleton skeleton;
    ModelNode nodes[GE_ORIGINAL_MODEL178_NODE_COUNT];
    union ModelRoData group_data;
    union ModelRoData bbox_data;
    union ModelRoData collision_data;
    Vertex vertices[GE_ORIGINAL_MODEL178_VERTEX_COUNT];
    Vertex collision_vertices[GE_ORIGINAL_MODEL178_COLLISION_VERTEX_COUNT];
    s16 point_usage[GE_ORIGINAL_MODEL178_VERTEX_COUNT];
    ModelFileHeader header;
    Model model;
    RenderPosView render_positions[1];
    uintptr_t rwdata_words[1];
};

GeOriginalModel178Status ge_original_model178_relocate(
    GeOriginalModel178 *runtime, const void *source_blob,
    size_t source_size);

#endif
