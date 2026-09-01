#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "ge_original_model104.h"

int main(int argc, char **argv)
{
    GeOriginalModel104 runtime;
    GeOriginalModel104Status status;
    FILE *file;
    uint8_t blob[GE_ORIGINAL_MODEL104_BLOB_SIZE];
    void *header;
    void *model;
    float scale;
    assert(argc==2);
    file=fopen(argv[1],"rb"); assert(file!=NULL);
    assert(fread(blob,1,sizeof(blob),file)==sizeof(blob));
    assert(fgetc(file)==EOF); fclose(file);
    status=ge_original_model104_relocate(&runtime,blob,sizeof(blob));
    assert(status==GE_ORIGINAL_MODEL104_OK);
    assert(runtime.header.RootNode==&runtime.nodes[0]);
    assert(runtime.nodes[0].Child==&runtime.nodes[1]);
    assert(runtime.nodes[1].Child==&runtime.nodes[2]);
    assert(runtime.nodes[0].Opcode==MODELNODE_OPCODE_GROUP);
    assert(runtime.nodes[1].Opcode==MODELNODE_OPCODE_BBOX);
    assert(runtime.nodes[2].Opcode==MODELNODE_OPCODE_DLCOLLISION);
    assert(runtime.bbox_data.BoundingBox.ModelNumber==1U);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.xmin+315.0f)<0.0001f);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.xmax-315.0f)<0.0001f);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.ymin+284.0f)<0.0001f);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.ymax-284.0f)<0.0001f);
    assert(runtime.collision_data.DisplayListCollisions.numVertices==4);
    assert(runtime.collision_data.DisplayListCollisions.numCollisionVertices==4);
    assert(runtime.collision_data.DisplayListCollisions.ModelType==4);
    assert(runtime.vertices[0].coord.x==315 && runtime.vertices[0].coord.y==284);
    assert(runtime.collision_vertices[3].index==3);
    assert(ge_original_model104_resolve_instance(
        &runtime,104,&header,&model,&scale));
    assert(header==&runtime.header && model==&runtime.model);
    assert(fabsf(scale-0.1f)<0.0001f);
    blob[0x0cU]=0xffU;
    assert(ge_original_model104_relocate(&runtime,blob,sizeof(blob))
           ==GE_ORIGINAL_MODEL104_INVALID_LAYOUT);
    puts("Dam PwindowZ model 104 relocated into native pointers");
    return 0;
}
