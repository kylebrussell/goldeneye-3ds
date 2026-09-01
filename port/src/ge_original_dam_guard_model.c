#include "ge_original_dam_guard_model.h"

#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <gbi_extension.h>

typedef enum GeDamGuardImageIds {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) IMAGE_##NAME,
#include "assets/images.def"
#undef IMAGE
} GeDamGuardImageIds;

#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \
    ModelSkeleton SKELETON(NAME) = { \
        NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0 \
    };
#include "assets/embedded/skeletons/guard.inc.c"
#undef MODELSKELETON
#include "assets/obseg/chr/greatguard2/modelFileHeader.inc.c"

/* Generated model names are file-offset based. Rename the two names shared
 * by every generated asset so this exact body can coexist with Pchrbug. */
#define SwitchNodes ge_greatguard2_switch_nodes
#define proptextures ge_greatguard2_textures
/* Generated SwitchNodes is serialized as u32 on N64. Native model headers
 * require the same authored pointers without truncating them on 64-bit host
 * sanitizer builds. This affects only that table and inert padding globals. */
#define u32 uintptr_t
#include "assets/obseg/chr/greatguard2/Model.c"
#undef u32
#undef proptextures
#undef SwitchNodes

#define GE_GUARD_NO_LIST UINT32_MAX
static const GeOriginalDamGuardDisplayList ge_greatguard2_display_lists[] = {
    {&ModelNode_0x16c, 0x4d50U, GE_GUARD_NO_LIST, 0x08e0U},
    {&ModelNode_0x1b4, 0x4dc0U, GE_GUARD_NO_LIST, 0x0a80U},
    {&ModelNode_0x1e4, 0x4e88U, GE_GUARD_NO_LIST, 0x0db8U},
    {&ModelNode_0x22c, 0x4f18U, GE_GUARD_NO_LIST, 0x0fa8U},
    {&ModelNode_0x25c, 0x5048U, GE_GUARD_NO_LIST, 0x1488U},
    {&ModelNode_0x2d4, 0x50b0U, GE_GUARD_NO_LIST, 0x1698U},
    {&ModelNode_0x31c, 0x5120U, GE_GUARD_NO_LIST, 0x1838U},
    {&ModelNode_0x34c, 0x51e8U, GE_GUARD_NO_LIST, 0x1b70U},
    {&ModelNode_0x394, 0x5278U, GE_GUARD_NO_LIST, 0x1d60U},
    {&ModelNode_0x3c4, 0x53a0U, GE_GUARD_NO_LIST, 0x2268U},
    {&ModelNode_0x43c, 0x5408U, GE_GUARD_NO_LIST, 0x2480U},
    {&ModelNode_0x4cc, 0x5488U, GE_GUARD_NO_LIST, 0x2668U},
    {&ModelNode_0x514, 0x5528U, GE_GUARD_NO_LIST, 0x28a0U},
    {&ModelNode_0x544, 0x55d8U, GE_GUARD_NO_LIST, 0x2b50U},
    {&ModelNode_0x58c, 0x5640U, GE_GUARD_NO_LIST, 0x2cb0U},
    {&ModelNode_0x5bc, 0x5710U, GE_GUARD_NO_LIST, 0x30c0U},
    {&ModelNode_0x634, 0x5778U, GE_GUARD_NO_LIST, 0x3260U},
    {&ModelNode_0x67c, 0x5818U, GE_GUARD_NO_LIST, 0x3498U},
    {&ModelNode_0x6ac, 0x58c8U, GE_GUARD_NO_LIST, 0x3748U},
    {&ModelNode_0x6f4, 0x5930U, GE_GUARD_NO_LIST, 0x38a8U},
    {&ModelNode_0x724, 0x5a00U, GE_GUARD_NO_LIST, 0x3c90U},
    {&ModelNode_0x76c, 0x5a68U, GE_GUARD_NO_LIST, 0x3df0U},
    {&ModelNode_0x79c, 0x5b90U, GE_GUARD_NO_LIST, 0x4578U},
    {&ModelNode_0x7e4, 0x5c18U, GE_GUARD_NO_LIST, 0x4818U},
    {&ModelNode_0x814, 0x5ca0U, GE_GUARD_NO_LIST, 0x4af0U},
};

/* The original model stores collision links as segment-5 addresses in the
 * Vertex UV union.  Generated native C preserves those authored halfwords,
 * but code in chrCreateBloodStain reads the same union as a ModelNode pointer.
 * Relocate the serialized addresses once, just as the N64 model loader does.
 * This is also required on a 32-bit little-endian target: constructing the
 * segmented value by aliasing the two halfwords would give byte-swapped data. */
static const uint16_t ge_greatguard2_node_offsets[] = {
    0x016cU, 0x01b4U, 0x01e4U, 0x022cU, 0x025cU,
    0x02d4U, 0x031cU, 0x034cU, 0x0394U, 0x03c4U,
    0x043cU, 0x04ccU, 0x0514U, 0x0544U, 0x058cU,
    0x05bcU, 0x0634U, 0x067cU, 0x06acU, 0x06f4U,
    0x0724U, 0x076cU, 0x079cU, 0x07e4U, 0x0814U,
};

static int ge_greatguard2_collision_links_relocated;

static int ge_greatguard2_relocate_collision_links(void)
{
    const size_t node_count = sizeof(ge_greatguard2_display_lists)
        / sizeof(ge_greatguard2_display_lists[0]);
    size_t source_index;

    if (ge_greatguard2_collision_links_relocated) {
        return 1;
    }

    for (source_index = 0; source_index < node_count; source_index++) {
        ModelNode *source = (ModelNode *)ge_greatguard2_display_lists[source_index].node;
        ModelRoData_DisplayList_CollisionRecord *rodata =
            &source->Data->DisplayListCollisions;
        int vertex_index;

        for (vertex_index = 0;
                vertex_index < rodata->numCollisionVertices;
                vertex_index++) {
            Vertex *vertex = &rodata->CollisionVertices[vertex_index];
            const uint16_t encoded_segment = (uint16_t)vertex->s;
            const uint16_t encoded_offset = (uint16_t)vertex->t;
            const uint8_t index_high = vertex->r;
            const uint8_t index_low = vertex->g;
            size_t target_index;

            if (encoded_segment == 0U && encoded_offset == 0U) {
                continue;
            }

            if (encoded_segment != 0x0500U) {
                return 0;
            }

            for (target_index = 0; target_index < node_count; target_index++) {
                if (ge_greatguard2_node_offsets[target_index] == encoded_offset) {
                    break;
                }
            }

            if (target_index == node_count) {
                return 0;
            }

            vertex->CollisionRelatedNode =
                (void *)ge_greatguard2_display_lists[target_index].node;
            vertex->CollisionRelatedIndex =
                (int16_t)(((uint16_t)index_high << 8) | index_low);
        }
    }

    ge_greatguard2_collision_links_relocated = 1;
    return 1;
}

int ge_original_dam_guard_model_prepare(void)
{
    greatguard2_header.RootNode = &ModelNode_0x0dc;
    greatguard2_header.Skeleton = &SKELETON(guard);
    greatguard2_header.Switches = (ModelNode **)ge_greatguard2_switch_nodes;
    greatguard2_header.Textures = ge_greatguard2_textures;
    return ge_greatguard2_relocate_collision_links()
        && greatguard2_header.RootNode != NULL
        && greatguard2_header.numMatrices == 0x14
        && greatguard2_header.numtextures == 0x10;
}

void *ge_original_dam_guard_model_header(void)
{
    return ge_original_dam_guard_model_prepare() ? &greatguard2_header : NULL;
}

size_t ge_original_dam_guard_model_matrix_count(void)
{
    return ge_original_dam_guard_model_prepare()
        ? (size_t)greatguard2_header.numMatrices : 0U;
}

const GeOriginalDamGuardDisplayList *
ge_original_dam_guard_model_display_lists(size_t *count)
{
    if (count != NULL) {
        *count = sizeof(ge_greatguard2_display_lists)
            / sizeof(ge_greatguard2_display_lists[0]);
    }
    return ge_greatguard2_display_lists;
}
