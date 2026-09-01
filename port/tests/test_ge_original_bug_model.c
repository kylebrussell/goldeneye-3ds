#include "ge_original_bug_model.h"

#include <assert.h>
#include <stdio.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

int main(void)
{
    ModelFileHeader *header;

    assert(ge_original_bug_model_id() == PROJECTILES_TYPE_BUG);
    assert(ge_original_bug_model_prepare());
    header = ge_original_bug_model_header();
    assert(header != NULL);
    assert(header->RootNode != NULL);
    assert(header->RootNode->Opcode == MODELNODE_OPCODE_GROUP);
    assert(header->RootNode->Child != NULL);
    assert(header->RootNode->Child->Opcode == MODELNODE_OPCODE_BBOX);
    assert(header->RootNode->Child->Child != NULL);
    assert(header->RootNode->Child->Child->Opcode
           == MODELNODE_OPCODE_DLCOLLISION);
    assert(header->Skeleton != NULL);
    assert(header->Skeleton->numjoints == 1);
    assert(header->numMatrices == 1);
    assert(header->numtextures == 6);
    assert(header->Textures != NULL);
    assert(header->Textures[0].Width == 64);
    assert(header->Textures[0].Height == 64);
    assert(header->Textures[5].Width == 1);
    assert(header->Textures[5].Height == 1);

    puts("Dam covert-modem Pchrbug model linked from decompiled asset");
    return 0;
}
