#ifndef GE_ORIGINAL_DAM_OBJECTIVE_MODELS_H
#define GE_ORIGINAL_DAM_OBJECTIVE_MODELS_H

#include <ultra64.h>
#include <bondtypes.h>

#include "ge_original_dam_objective_models_runtime.h"

#define GE_ORIGINAL_MODEMBOX_NODE_COUNT 5U
#define GE_ORIGINAL_MODEMBOX_VERTEX_COUNT 56U
#define GE_ORIGINAL_MODEMBOX_COLLISION_VERTEX_COUNT 16U
#define GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_COUNT 4U
#define GE_ORIGINAL_MODEMBOX_RW_WORD_COUNT 5U
#define GE_ORIGINAL_SATDISH_NODE_COUNT 3U
#define GE_ORIGINAL_SATDISH_VERTEX_COUNT 92U
#define GE_ORIGINAL_SATDISH_COLLISION_VERTEX_COUNT 42U
#define GE_ORIGINAL_SATDISH_RW_WORD_COUNT 2U

typedef struct GeOriginalModemboxModel {
    const uint8_t *source_blob;
    size_t source_size;
    ModelFileTextures textures[4];
    ModelJoint joints[1];
    ModelSkeleton skeleton;
    ModelNode nodes[GE_ORIGINAL_MODEMBOX_NODE_COUNT];
    union ModelRoData group_data;
    union ModelRoData bbox_data;
    union ModelRoData collision_data[2];
    union ModelRoData switch_data;
    ModelNode *switch_nodes[1];
    Vertex vertices[GE_ORIGINAL_MODEMBOX_VERTEX_COUNT];
    Vertex collision_vertices[GE_ORIGINAL_MODEMBOX_COLLISION_VERTEX_COUNT];
    s16 point_usage[GE_ORIGINAL_MODEMBOX_VERTEX_COUNT];
    Vertex screen_vertices[GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_COUNT];
    Vertex screen_collision_vertices[GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_COUNT];
    s16 screen_point_usage[GE_ORIGINAL_MODEMBOX_SCREEN_VERTEX_COUNT];
    ModelFileHeader header;
    Model model;
    RenderPosView render_positions[1];
    uintptr_t rwdata_words[GE_ORIGINAL_MODEMBOX_RW_WORD_COUNT];
} GeOriginalModemboxModel;

typedef struct GeOriginalSatdishModel {
    const uint8_t *source_blob;
    size_t source_size;
    ModelFileTextures textures[2];
    ModelJoint joints[1];
    ModelSkeleton skeleton;
    ModelNode nodes[GE_ORIGINAL_SATDISH_NODE_COUNT];
    union ModelRoData group_data;
    union ModelRoData bbox_data;
    union ModelRoData collision_data;
    Vertex vertices[GE_ORIGINAL_SATDISH_VERTEX_COUNT];
    Vertex collision_vertices[GE_ORIGINAL_SATDISH_COLLISION_VERTEX_COUNT];
    s16 point_usage[GE_ORIGINAL_SATDISH_VERTEX_COUNT];
    ModelFileHeader header;
    Model model;
    RenderPosView render_positions[1];
    uintptr_t rwdata_words[GE_ORIGINAL_SATDISH_RW_WORD_COUNT];
} GeOriginalSatdishModel;

struct GeOriginalDamObjectiveModels {
    GeOriginalModemboxModel modembox;
    GeOriginalSatdishModel satdish;
};

GeOriginalDamObjectiveModelsStatus ge_original_modembox_model_relocate(
    GeOriginalModemboxModel *runtime, const void *source_blob,
    size_t source_size);
GeOriginalDamObjectiveModelsStatus ge_original_satdish_model_relocate(
    GeOriginalSatdishModel *runtime, const void *source_blob,
    size_t source_size);

#endif
