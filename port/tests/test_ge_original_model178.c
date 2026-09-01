#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "ge_original_model178.h"

int main(int argc,char **argv)
{
    GeOriginalModel178 r; uint8_t blob[GE_ORIGINAL_MODEL178_BLOB_SIZE];
    FILE *f; void *h,*m; float scale;
    assert(argc==2); f=fopen(argv[1],"rb"); assert(f);
    assert(fread(blob,1,sizeof(blob),f)==sizeof(blob)); assert(fgetc(f)==EOF);
    assert(fclose(f)==0);
    assert(ge_original_model178_relocate(&r,blob,sizeof(blob))==GE_ORIGINAL_MODEL178_OK);
    assert(r.nodes[0].Opcode==MODELNODE_OPCODE_GROUP);
    assert(r.nodes[1].Opcode==MODELNODE_OPCODE_BBOX);
    assert(r.nodes[2].Opcode==MODELNODE_OPCODE_DLCOLLISION);
    assert(r.collision_data.DisplayListCollisions.numVertices==44);
    assert(r.collision_data.DisplayListCollisions.numCollisionVertices==20);
    assert(fabsf(r.bbox_data.BoundingBox.Bounds.xmin+253.0f)<0.0001f);
    assert(fabsf(r.bbox_data.BoundingBox.Bounds.xmax-253.0f)<0.0001f);
    assert(ge_original_model178_resolve_instance(&r,178,&h,&m,&scale));
    assert(h==&r.header&&m==&r.model&&scale==1.0f);
    blob[0x24]=0xff;
    assert(ge_original_model178_relocate(&r,blob,sizeof(blob))==GE_ORIGINAL_MODEL178_INVALID_LAYOUT);
    puts("Dam PdamgatedoorZ model 178 relocated into native pointers");
    return 0;
}
