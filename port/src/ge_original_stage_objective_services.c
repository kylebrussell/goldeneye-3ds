#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcomment"
#include "game/bondview.h"
#pragma GCC diagnostic pop
#include "game/model.h"
#include "ge_original_stage_objective_runtime.h"

extern bool chrHasStageFlag(ChrRecord *self, s32 flags);
extern struct player *g_CurrentPlayer;
extern bool objGetOnscreenRenderBounds(
    PropRecord *prop, coord3d *center, coord2d *horizontal, coord2d *vertical);
extern void projectRectCornersTo2D(
    coord3d *center, coord2d *horizontal, coord2d *vertical,
    coord2d *minimum, coord2d *maximum);

bool bondinvHasPropInInv(PropRecord *prop)
{
    InvItem *item = g_CurrentPlayer != NULL
        ? g_CurrentPlayer->ptr_inventory_first_in_cycle : NULL;
    /* Canonical bondinv.c circular-list walk.  The null player guard is only
     * the platform startup boundary; live mission AI always has a player. */
    while (item != NULL) {
        if (item->type == INV_ITEM_PROP
                && item->type_inv_item.type_prop.prop == prop)
            return TRUE;
        item = item->next;
        if (item == g_CurrentPlayer->ptr_inventory_first_in_cycle) break;
    }
    return FALSE;
}

int ge_original_stage_objective_prop_in_inventory_exact(
    void *context, const void *opaque_prop)
{
    struct player *player = context;
    const PropRecord *prop = opaque_prop;
    InvItem *item;
    if (player == NULL || prop == NULL) return -1;
    item = player->ptr_inventory_first_in_cycle;
    while (item != NULL) {
        if (item->type == INV_ITEM_PROP
                && item->type_inv_item.type_prop.prop == prop)
            return 1;
        item = item->next;
        if (item == player->ptr_inventory_first_in_cycle) break;
    }
    return 0;
}

int ge_original_stage_objective_stage_flag_set_exact(
    void *context, uint32_t flags)
{
    (void)context;
    return chrHasStageFlag(NULL, (s32)flags) ? 1 : 0;
}

int ge_original_stage_objective_key_analyzer_complete_exact(void *context)
{
    const struct player *player = context;
    /* Exact get_keyanalyzer_flag field read, guarded only at this adapter. */
    return player != NULL ? (player->copiedgoldeneye != 0) : -1;
}

int ge_original_stage_objective_photograph_binding_ready_exact(
    void *context, const void *opaque_object, const void *opaque_prop)
{
    const ObjectRecord *object = opaque_object;
    const PropRecord *prop = opaque_prop;
    ModelNode *node;
    int bbox_count = 0;
    if (context == NULL || context != g_CurrentPlayer || object == NULL
            || prop == NULL || object->prop != prop || prop->obj != object
            || object->model == NULL || object->model->obj == NULL
            || object->model->obj->RootNode == NULL
            || getsubmatrix(object->model) == NULL)
        return 0;
    node = object->model->obj->RootNode;
    while (node != NULL) {
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_BBOX) {
            if (node->Data == NULL
                    || modelFindNodeMtx(object->model, node, 0) == NULL)
                return 0;
            ++bbox_count;
        }
        if (node->Child != NULL) {
            node = node->Child;
        } else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    return bbox_count != 0;
}

int ge_original_stage_objective_photograph_bounds_inside_view_exact(
    void *context, const void *opaque_object, const void *opaque_prop)
{
    const ObjectRecord *object = opaque_object;
    PropRecord *prop = (PropRecord *)opaque_prop;
    coord3d center;
    coord2d horizontal;
    coord2d vertical;
    struct rectbbox bounds;
    float left, right, top, bottom;
    if (!ge_original_stage_objective_photograph_binding_ready_exact(
            context, object, prop))
        return -1;
    if (!objGetOnscreenRenderBounds(
            prop, &center, &horizontal, &vertical)) return 0;
    projectRectCornersTo2D(
        &center, &horizontal, &vertical,
        (coord2d *)&bounds.right, (coord2d *)&bounds.left);
    left = getPlayer_c_screenleft();
    right = left + getPlayer_c_screenwidth();
    top = getPlayer_c_screentop();
    bottom = top + getPlayer_c_screenheight();
    return left < bounds.right && bounds.right < right
        && left < bounds.left && bounds.left < right
        && top < bounds.down && bounds.down < bottom
        && top < bounds.up && bounds.up < bottom;
}
