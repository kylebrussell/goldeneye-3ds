#include "ge_original_guard_grenade_model.h"

#include <stdint.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <gbi_extension.h>
#include "game/model.h"

typedef enum GeGuardGrenadeImageIds {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) IMAGE_##NAME,
#include "assets/images.def"
#undef IMAGE
} GeGuardGrenadeImageIds;

/* These canonical symbols are also supplied by the native AK-47 model. Weak
 * definitions preserve one pointer identity when either or both models are
 * linked, while keeping the grenade model independently testable. */
ModelJoint JOINTLIST(prop_weapon)[2] __attribute__((weak)) = {
    {0x0015, 0x0000, 0x0000},
    {0x0015, 0x0001, 0x0001},
};
ModelSkeleton SKELETON(prop_weapon) __attribute__((weak)) = {
    2, 0, JOINTLIST(prop_weapon), 0, 0
};

#define chrgrenade_header ge_chrgrenade_header
#include "assets/obseg/prop/chrgrenade/ModelFileHeader.inc.c"
#undef chrgrenade_header

/* Generated names are file-offset based and collide with other PitemZ model
 * translation units. Namespace every exact generated symbol. SwitchNodes is
 * serialized as u32 on N64, but is a native pointer array after relocation. */
#define SwitchNodes ge_chrgrenade_switch_nodes
#define proptextures ge_chrgrenade_textures
#define ModelNode_0x024 ge_chrgrenade_node_024
#define ModelNode_0x03c ge_chrgrenade_node_03c
#define ModelNode_0x054 ge_chrgrenade_node_054
#define GroupSimpleRecord_0x06c ge_chrgrenade_group_06c
#define BoundingBoxRecord_0x080 ge_chrgrenade_bbox_080
#define DisplayListRecord_0x660 ge_chrgrenade_dl_660
#define Vertex_0x0a0 ge_chrgrenade_vertices_0a0
#define GFX_PRIMARY_0x678 ge_chrgrenade_primary_678
#define PADDING_0x09c ge_chrgrenade_padding_09c
#define PADDING_0x674 ge_chrgrenade_padding_674
#define u32 uintptr_t
#include "assets/obseg/prop/chrgrenade/Model.c"
#undef u32
#undef PADDING_0x674
#undef PADDING_0x09c
#undef GFX_PRIMARY_0x678
#undef Vertex_0x0a0
#undef DisplayListRecord_0x660
#undef BoundingBoxRecord_0x080
#undef GroupSimpleRecord_0x06c
#undef ModelNode_0x054
#undef ModelNode_0x03c
#undef ModelNode_0x024
#undef proptextures
#undef SwitchNodes

int ge_original_guard_grenade_model_prepare(void)
{
    ge_chrgrenade_header.RootNode = &ge_chrgrenade_node_024;
    ge_chrgrenade_header.Skeleton = &SKELETON(prop_weapon);
    ge_chrgrenade_header.Switches =
        (ModelNode **)ge_chrgrenade_switch_nodes;
    ge_chrgrenade_header.Textures = ge_chrgrenade_textures;
    modelCalculateRwDataLen(&ge_chrgrenade_header);
    return ge_chrgrenade_header.RootNode == &ge_chrgrenade_node_024
        && ge_chrgrenade_header.Skeleton == &SKELETON(prop_weapon)
        && ge_chrgrenade_header.numMatrices == 1
        /* GROUPSIMPLE/BBOX/DL carry no mutable rwdata records. */
        && ge_chrgrenade_header.numRecords == 0
        && ge_chrgrenade_header.numtextures == 2;
}

void *ge_original_guard_grenade_model_header(void)
{
    return ge_original_guard_grenade_model_prepare()
        ? &ge_chrgrenade_header : NULL;
}

size_t ge_original_guard_grenade_model_rw_words(void)
{
    return ge_original_guard_grenade_model_prepare()
        ? (size_t)ge_chrgrenade_header.numRecords : 0U;
}
