#include <ultra64.h>
#include <bondtypes.h>
#include "game/model.h"

/* Exact small model.c bodies that production already receives through the
 * guard-AI support slice.  Keep the focused host link bounded instead of
 * pulling that unrelated full dependency graph into this regression. */
void setsubroty(Model *model, f32 angle)
{
    ModelNode *node = model->obj->RootNode;
    if ((node->Opcode & 0xff) == MODELNODE_OPCODE_HEADER) {
        ModelRwData_HeaderRecord *rwdata = modelGetNodeRwData(model, node);
        f32 diff = angle - rwdata->unk14;
        if (diff < 0) diff += M_TAU_F;
        rwdata->unk30 += diff;
        if (rwdata->unk30 >= M_TAU_F) rwdata->unk30 -= M_TAU_F;
        rwdata->unk20 += diff;
        if (rwdata->unk20 >= M_TAU_F) rwdata->unk20 -= M_TAU_F;
        rwdata->unk14 = angle;
    }
}

void modelSetAnimTranslationScale(Model *model, f32 scale)
{
    model->anim_translation_scale = scale;
}
