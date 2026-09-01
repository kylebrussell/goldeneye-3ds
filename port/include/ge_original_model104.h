#ifndef GE_ORIGINAL_MODEL104_H
#define GE_ORIGINAL_MODEL104_H

#include <stddef.h>
#include <stdint.h>

#include <ultra64.h>
#include <bondtypes.h>

#include "ge_original_model104_runtime.h"

#define GE_ORIGINAL_MODEL104_NODE_COUNT 3U
#define GE_ORIGINAL_MODEL104_VERTEX_COUNT 4U

struct GeOriginalModel104 {
    const uint8_t *source_blob;
    size_t source_size;
    ModelFileTextures textures[1];
    ModelJoint joints[2];
    ModelSkeleton skeleton;
    ModelNode nodes[GE_ORIGINAL_MODEL104_NODE_COUNT];
    union ModelRoData group_data;
    union ModelRoData bbox_data;
    union ModelRoData collision_data;
    Vertex vertices[GE_ORIGINAL_MODEL104_VERTEX_COUNT];
    Vertex collision_vertices[GE_ORIGINAL_MODEL104_VERTEX_COUNT];
    s16 point_usage[GE_ORIGINAL_MODEL104_VERTEX_COUNT];
    ModelFileHeader header;
    Model model;
    RenderPosView render_positions[1];
    uintptr_t rwdata_words[1];
};

GeOriginalModel104Status ge_original_model104_relocate(
    GeOriginalModel104 *runtime, const void *source_blob,
    size_t source_size);

#endif
