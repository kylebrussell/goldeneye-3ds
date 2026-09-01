#include "ge_original_dam_guard_weapon_model.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <gbi_extension.h>
#include "game/model.h"

typedef enum GeDamGuardWeaponImageIds {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) IMAGE_##NAME,
#include "assets/images.def"
#undef IMAGE
} GeDamGuardWeaponImageIds;

/* Other exact retained slices also carry this canonical skeleton. Weak
 * ownership lets their strong definition win without changing pointer
 * identity, while the standalone constructor test still has the exact data. */
ModelJoint JOINTLIST(prop_weapon)[2] __attribute__((weak)) = {
    {0x0015, 0x0000, 0x0000},
    {0x0015, 0x0001, 0x0001},
};
ModelSkeleton SKELETON(prop_weapon) __attribute__((weak)) = {
    2, 0, JOINTLIST(prop_weapon), 0, 0
};
#define chrkalash_header ge_chrkalash_header
#include "assets/obseg/prop/chrkalash/ModelFileHeader.inc.c"
#undef chrkalash_header

/* Generated model names are file-offset based and collide across PitemZ
 * translation units. Keep this exact asset native while giving its symbols a
 * model-specific namespace. SwitchNodes is serialized as u32 on N64, so use
 * native pointer width for this one generated compilation, as the guard-body
 * model does for its authored switch table. */
#define SwitchNodes ge_chrkalash_switch_nodes
#define proptextures ge_chrkalash_textures
#define ModelNode_0x06c ge_chrkalash_node_06c
#define ModelNode_0x084 ge_chrkalash_node_084
#define ModelNode_0x09c ge_chrkalash_node_09c
#define ModelNode_0x0b4 ge_chrkalash_node_0b4
#define GroupSimpleRecord_0x0cc ge_chrkalash_group_0cc
#define BoundingBoxRecord_0x0e0 ge_chrkalash_bbox_0e0
#define DisplayListRecord_0x740 ge_chrkalash_dl_740
#define GunfireRecord_0x754 ge_chrkalash_gunfire_754
#define Vertex_0x100 ge_chrkalash_vertices_100
#define GFX_PRIMARY_0x780 ge_chrkalash_primary_780
#define GFX_SECONDARY_0x8d8 ge_chrkalash_secondary_8d8
#define PADDING_0x0fc ge_chrkalash_padding_0fc
#define PADDING_0x77c ge_chrkalash_padding_77c
#define u32 uintptr_t
#include "assets/obseg/prop/chrkalash/Model.c"
#undef u32
#undef PADDING_0x77c
#undef PADDING_0x0fc
#undef GFX_SECONDARY_0x8d8
#undef GFX_PRIMARY_0x780
#undef Vertex_0x100
#undef GunfireRecord_0x754
#undef DisplayListRecord_0x740
#undef BoundingBoxRecord_0x0e0
#undef GroupSimpleRecord_0x0cc
#undef ModelNode_0x0b4
#undef ModelNode_0x09c
#undef ModelNode_0x084
#undef ModelNode_0x06c
#undef proptextures
#undef SwitchNodes

static const GeOriginalDamGuardDisplayList ge_chrkalash_display_list = {
    &ge_chrkalash_node_09c,
    UINT32_C(0x780),
    UINT32_C(0x8d8),
    UINT32_MAX,
};

int ge_original_dam_guard_weapon_model_prepare(void)
{
    ge_chrkalash_header.RootNode = &ge_chrkalash_node_06c;
    ge_chrkalash_header.Skeleton = &SKELETON(prop_weapon);
    ge_chrkalash_header.Switches =
        (ModelNode **)ge_chrkalash_switch_nodes;
    ge_chrkalash_header.Textures = ge_chrkalash_textures;
    modelCalculateRwDataLen(&ge_chrkalash_header);
    return ge_chrkalash_header.RootNode != NULL
        && ge_chrkalash_header.Skeleton == &SKELETON(prop_weapon)
        && ge_chrkalash_header.Switches[0] == &ge_chrkalash_node_0b4
        && ge_chrkalash_header.numMatrices == 1
        && ge_chrkalash_header.numRecords == 1
        && ge_chrkalash_header.numtextures == 8;
}

void *ge_original_dam_guard_weapon_model_header(void)
{
    return ge_original_dam_guard_weapon_model_prepare()
        ? &ge_chrkalash_header : NULL;
}

size_t ge_original_dam_guard_weapon_model_rw_words(void)
{
    return ge_original_dam_guard_weapon_model_prepare()
        ? (size_t)ge_chrkalash_header.numRecords : 0U;
}

const GeOriginalDamGuardDisplayList *
ge_original_dam_guard_weapon_model_display_list(void)
{
    return ge_original_dam_guard_weapon_model_prepare()
        ? &ge_chrkalash_display_list : NULL;
}
