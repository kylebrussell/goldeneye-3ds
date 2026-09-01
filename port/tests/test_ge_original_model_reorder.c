#include <assert.h>
#include <string.h>

#include "ge_original_player_gait_internal.h"

int main(void)
{
    Model model;
    ModelNode base;
    ModelNode left;
    ModelNode right;
    union ModelRoData rodata;
    _Alignas(8) u32 words[32];
    _Alignas(8) u32 head_words[8];
    union ModelRwData *rwdata;
    struct ModelRwData_BSPRecord *bsp_rwdata;
    ModelNode header_node;
    ModelNode head_node;
    ModelNode dl_node;
    ModelNode head_child;
    union ModelRoData header_rodata;
    union ModelRoData head_rodata;
    union ModelRoData dl_rodata;
    union ModelRoData child_rodata;
    struct ModelRwData_HeadPlaceholderRecord *head_rwdata;
    s32 rw_word_count;

    memset(&model, 0, sizeof(model));
    memset(&base, 0, sizeof(base));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    memset(&rodata, 0, sizeof(rodata));
    memset(words, 0, sizeof(words));
    memset(head_words, 0, sizeof(head_words));
    model.datas = (union ModelRwData **)(void *)words;
    base.Opcode = MODELNODE_OPCODE_BSP;
    base.Data = &rodata;
    rodata.BSP.RwDataIndex = 5;
    rodata.BSP.leftChild = &left;
    rodata.BSP.rightChild = &right;
    rwdata = modelGetNodeRwData(&model, &base);
    assert(rwdata == (union ModelRwData *)(void *)&words[5]);

    left.Next = &right;
    right.Next = &left;
    bsp_rwdata = (struct ModelRwData_BSPRecord *)(void *)rwdata;
    bsp_rwdata->visible = TRUE;
    modelApplyReorderRelations(&model, &base);
    assert(base.Child == &left);
    assert(left.Prev == NULL && left.Next == &right);
    assert(right.Prev == &left && right.Next == NULL);

    left.Next = &right;
    right.Next = &left;
    modelApplyReorderRelationsByArg(&base, FALSE);
    assert(base.Child == &right);
    assert(right.Prev == NULL && right.Next == &left);
    assert(left.Prev == &right && left.Next == NULL);

    memset(&header_node, 0, sizeof(header_node));
    memset(&head_node, 0, sizeof(head_node));
    memset(&dl_node, 0, sizeof(dl_node));
    memset(&head_child, 0, sizeof(head_child));
    memset(&header_rodata, 0, sizeof(header_rodata));
    memset(&head_rodata, 0, sizeof(head_rodata));
    memset(&dl_rodata, 0, sizeof(dl_rodata));
    memset(&child_rodata, 0, sizeof(child_rodata));
    header_node.Opcode = MODELNODE_OPCODE_HEADER;
    header_node.Data = &header_rodata;
    header_node.Next = &head_node;
    head_node.Opcode = MODELNODE_OPCODE_HEAD;
    head_node.Data = &head_rodata;
    head_node.Next = &dl_node;
    dl_node.Opcode = MODELNODE_OPCODE_DLCOLLISION;
    dl_node.Data = &dl_rodata;
    rw_word_count = modelCalculateRwDataIndexes(&header_node);
    assert(rw_word_count > 0);
    assert((head_rodata.HeadPlaceholder.RwDataIndex * sizeof(u32)) %
           _Alignof(struct ModelRwData_HeadPlaceholderRecord) == 0);
    assert((dl_rodata.DisplayListCollisions.RwDataIndex * sizeof(u32)) %
           _Alignof(struct ModelRwData_DisplayList_CollisionRecord) == 0);

    head_node.Parent = NULL;
    head_child.Opcode = MODELNODE_OPCODE_HEADER;
    head_child.Data = &child_rodata;
    head_child.Parent = &head_node;
    child_rodata.Header.RwDataIndex = 1;
    head_rwdata = (struct ModelRwData_HeadPlaceholderRecord *)(void *)
        &words[head_rodata.HeadPlaceholder.RwDataIndex];
    head_rwdata->RwDatas = head_words;
    assert(modelGetNodeRwData(&model, &head_child) ==
           (union ModelRwData *)(void *)&head_words[1]);
    return 0;
}
