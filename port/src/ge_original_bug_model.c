#include "ge_original_bug_model.h"

#include <ultra64.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include <gbi_extension.h>

/* AIPARSE intentionally suppresses the IMAGEIDS enum in bondconstants.h, but
 * the generated model display list still names its texture ids. Recreate that
 * enum from the same canonical images.def source instead of baking ids here. */
typedef enum GeBugImageIds {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) IMAGE_##NAME,
#include "assets/images.def"
#undef IMAGE
} GeBugImageIds;

ModelJoint jointlist_standard_object[] = {
    {0x0002, 0x0000, 0x0000}
};
ModelSkeleton skeleton_standard_object = {
    1, 0, jointlist_standard_object, 3, 0
};
#include "assets/obseg/prop/chrbug/ModelFileHeader.inc.c"
#include "assets/obseg/prop/chrbug/Model.c"

int ge_original_bug_model_prepare(void)
{
    chrbug_header.RootNode = &ModelNode_0x048;
    chrbug_header.Skeleton = &SKELETON(standard_object);
    chrbug_header.Textures = proptextures;
    return chrbug_header.RootNode != NULL
        && chrbug_header.numMatrices == 1
        && chrbug_header.numtextures == 6;
}

void *ge_original_bug_model_header(void)
{
    return ge_original_bug_model_prepare() ? &chrbug_header : NULL;
}

int32_t ge_original_bug_model_id(void)
{
    return PROJECTILES_TYPE_BUG;
}
