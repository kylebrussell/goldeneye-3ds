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

extern struct player *g_CurrentPlayer;

/* Unchanged decompiled bodies required by objectiveTakePictureHandler. */
void modelGetAxisExtents(Model* model, f32* max, f32* min, s32 axis)
{
    ModelNode *node = model->obj->RootNode;
    bool first = TRUE;

    while (node)
    {
        u32 type = node->Opcode & 0xFF;

        if (type == MODELNODE_OPCODE_BBOX)
        {
            struct ModelRoData_BoundingBoxRecord *bbox = &node->Data->BoundingBox;
            Mtxf *mtx = modelFindNodeMtx(model, node, 0);
            f32 dist1;
            f32 dist2;

            if (axis == 0)
            {
                dist1 = chrpropSumMatrixNegX(bbox, mtx) + mtx->m[3][0];
                dist2 = chrpropSumMatrixPosX(bbox, mtx) + mtx->m[3][0];
            }
            else if (axis == 1)
            {
                dist1 = chrpropSumMatrixNegY(bbox, mtx) + mtx->m[3][1];
                dist2 = chrpropSumMatrixPosY(bbox, mtx) + mtx->m[3][1];
            }
            else
            {
                dist1 = chrpropSumMatrixNegZ(bbox, mtx) + mtx->m[3][2];
                dist2 = chrpropSumMatrixPosZ(bbox, mtx) + mtx->m[3][2];
            }

            if (first || dist1 > *max)
            {
                *max = dist1;
            }

            if (first || dist2 < *min)
            {
                *min = dist2;
            }

            first = FALSE;
        }
        else
        {
            // empty
        }

        if (node->Child)
        {
            node = node->Child;
        }
        else
        {
            while (node)
            {
                if (node->Next)
                {
                    node = node->Next;
                    break;
                }

                node = node->Parent;
            }
        }
    }
}

void modelGetXYExtents(Model *model, f32 *arg1, f32 *arg2, f32 *arg3, f32 *arg4)
{
    modelGetAxisExtents(model, arg1, arg2, 0);
    modelGetAxisExtents(model, arg3, arg4, 1);
}

bool objGetOnscreenRenderBounds(PropRecord *prop, coord3d *arg1, struct coord2d *arg2, struct coord2d *arg3)
{
    if (prop->flags & PROPFLAG_ONSCREEN)
    {
        ObjectRecord *obj = prop->obj;
        Mtxf *matrix = getsubmatrix(obj->model);

        arg1->z = matrix->m[3][2];

        if (arg1->z < 0)
        {
            arg1->x = matrix->m[3][0];
            arg1->y = matrix->m[3][1];

            arg3->f[0] = 0;
            arg3->f[1] = 0;

            arg2->f[0] = 0;
            arg2->f[1] = 0;

            modelGetXYExtents(obj->model, &arg2->f[1], &arg2->f[0], &arg3->f[1], &arg3->f[0]);

            return TRUE;
        }
    }

    return FALSE;
}

void transform3Dto2DCoords(coord3d *in, coord2d *out)
{
    f32 inv_z = (1.0f / in->z);
    out->y = (in->y * inv_z * g_CurrentPlayer->c_recipscaley) + (g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight);
    out->x = (g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth) - (in->x * inv_z * g_CurrentPlayer->c_recipscalex);
}

void projectRectCornersTo2D(struct coord3d *center, struct coord2d *arg1, struct coord2d *arg2, struct coord2d *arg3, struct coord2d *arg4)
{
    struct coord3d sp24;
    struct coord2d tout;

    sp24.f[0] = arg1->f[0];
    sp24.f[1] = center->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg3->f[0] = tout.f[0];

    sp24.f[0] = arg1->f[1];
    sp24.f[1] = center->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg4->f[0] = tout.f[0];

    sp24.f[0] = center->f[0];
    sp24.f[1] = arg2->f[1];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg3->f[1] = tout.f[1];

    sp24.f[0] = center->f[0];
    sp24.f[1] = arg2->f[0];
    sp24.f[2] = center->f[2];
    transform3Dto2DCoords(&sp24, &tout);
    arg4->f[1] = tout.f[1];
}
