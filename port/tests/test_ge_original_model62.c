#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ge_original_model62.h"
#include "ge_original_default_object.h"

int main(int argc, char **argv)
{
    FILE *file;
    uint8_t *blob;
    GeOriginalModel62 runtime;
    void *header;
    void *model;
    float scale;
    GeOriginalDefaultObjectProviders providers = {0};

    assert(argc == 2);
    file=fopen(argv[1],"rb"); assert(file != NULL);
    blob=malloc(GE_ORIGINAL_MODEL62_BLOB_SIZE); assert(blob != NULL);
    assert(fread(blob,1,GE_ORIGINAL_MODEL62_BLOB_SIZE,file)==GE_ORIGINAL_MODEL62_BLOB_SIZE);
    assert(fgetc(file)==EOF); assert(fclose(file)==0);
    assert(ge_original_model62_relocate(&runtime,blob,GE_ORIGINAL_MODEL62_BLOB_SIZE)==GE_ORIGINAL_MODEL62_OK);
    assert(runtime.header.RootNode==&runtime.nodes[0]);
    assert(runtime.nodes[0].Child==&runtime.nodes[1]);
    assert(runtime.nodes[1].Child==&runtime.nodes[2]);
    assert(runtime.nodes[1].Next==&runtime.nodes[3]);
    assert(runtime.nodes[3].Prev==&runtime.nodes[1]);
    assert(runtime.header.Switches[0]==&runtime.nodes[3]);
    assert(runtime.header.numSwitches==3 && runtime.header.numMatrices==1);
    assert(runtime.header.numRecords==1 && runtime.header.numtextures==6);
    assert(runtime.bbox_data.BoundingBox.ModelNumber==100U);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.xmin-(-216.0f))<0.0001f);
    assert(fabsf(runtime.bbox_data.BoundingBox.Bounds.xmax-119.0f)<0.0001f);
    assert(runtime.display_list_data.DisplayList.numVertices==74U && runtime.display_list_data.DisplayList.ModelType==4);
    assert((const uint8_t *)(const void *)runtime.display_list_data.DisplayList.Primary==blob+0x5c8U);
    assert((const uint8_t *)(const void *)runtime.display_list_data.DisplayList.Secondary==blob+0x6b8U);
    assert(runtime.display_list_data.DisplayList.BaseAddr==blob);
    assert(runtime.display_list_data.DisplayList.Vertices==runtime.vertices);
    assert(runtime.vertices[0].coord.x==94 && runtime.vertices[73].coord.z==-3);
    assert(runtime.gunfire_data.Gunfire.Image==blob+0x3cU);
    assert(runtime.gunfire_data.Gunfire.BaseAddr==0U);
    assert(runtime.model.obj==&runtime.header && runtime.model.render_pos==runtime.render_positions);
    assert(runtime.model.datas==(union ModelRwData **)(void *)runtime.rwdata_words);
    assert(runtime.model.rwdatalen==1);
    providers.context=&runtime;
    providers.model_load=ge_original_model62_model_load;
    providers.resolve_model_instance=ge_original_model62_resolve_instance;
    assert(providers.model_load(providers.context,62)==1);
    assert(ge_original_model62_model_load(&runtime,62)==1);
    assert(ge_original_model62_model_load(&runtime,61)==0);
    assert(ge_original_model62_resolve_instance(&runtime,62,&header,&model,&scale));
    assert(header==&runtime.header && model==&runtime.model && fabsf(scale-0.1f)<0.0001f);
    blob[0]^=1U;
    assert(ge_original_model62_relocate(&runtime,blob,GE_ORIGINAL_MODEL62_BLOB_SIZE)==GE_ORIGINAL_MODEL62_HASH_MISMATCH);
    free(blob);
    puts("PP7 model 62 exact ROM blob relocated into native pointers");
    return 0;
}
