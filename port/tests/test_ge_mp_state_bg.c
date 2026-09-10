#define main ge_bg_geometry_test_main
#include "test_ge_original_bg_geometry.c"
#undef main
#include "ge_mp_state_bg.h"

static GeOriginalBgGeometry *state_owner(int reversed)
{
    uint8_t points[48]={0},gdl[32]={0};
    const int16_t xy[3][2]={{0,0},{40,0},{0,40}};
    GeOriginalBgRoomBounds bounds[3]={0};
    GeDamRoomBlobDescriptor rooms[2]={{0}};
    for (size_t point=0;point<3;++point) {
        be16(points+point*16,(uint16_t)xy[point][0]);
        be16(points+point*16+2,(uint16_t)xy[point][1]);
        memset(points+point*16+12,240,4);
    }
    command(gdl,0xc0000000,201); command(gdl+8,0x04200030,0x0e000000);
    command(gdl+16,0xbf000000,0x00000a14); command(gdl+24,0xb8000000,0);
    for (size_t slot=0;slot<2;++slot) {
        size_t room=reversed?2-slot:slot+1;
        rooms[slot]=(GeDamRoomBlobDescriptor){.room_id=(uint32_t)room,.origin={(float)(room*100),200,300},
            .point_table=points,.point_table_size=48,.primary_gdl=gdl,.primary_gdl_size=32};
        bounds[room].minimum[0]=(float)(room*100); bounds[room].maximum[0]=(float)(room*100+40);
        bounds[room].minimum[1]=200; bounds[room].maximum[1]=240;
        bounds[room].minimum[2]=bounds[room].maximum[2]=300;
    }
    GeOriginalBgGeometryReport report;
    GeOriginalBgGeometry *owner=ge_original_bg_geometry_open_rooms(rooms,2,bounds,3,0.5f,&report);
    assert(owner != NULL); return owner;
}

static void state_digest(const GeMpStateBgSnapshot *bg, const GeMpRegistry *registry, uint8_t hash[32])
{
    uint32_t global_value=1;
    GeMpFieldSchema global_schema={256,GE_MP_STATE_U32,0,1,1};
    GeMpStateField global_field={256,GE_MP_STATE_U32,1,&global_value};
    GeMpRecordSchema record_schemas[2]={{0,GE_MP_SYSTEM_SESSION,&global_schema,1},*ge_mp_state_bg_schema()};
    GeMpStateSchema schema={1,GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_OBJECTS,1,record_schemas,2};
    GeMpStateRecord records[2]={{0,0,&global_field,1},bg->record};
    GeMpCanonicalSnapshot snapshot={.session=17,.tick=1,.enabled_systems=schema.registered_systems,
        .schema=&schema,.registry=registry,.records=records,.record_count=2,
        .coverage={schema.registered_systems,0,0}};
    memset(snapshot.build_sha256,1,32); memset(snapshot.content_sha256,2,32); memset(snapshot.rules_sha256,3,32);
    assert(bg->record_count==1);
    assert(ge_mp_state_sha256(&snapshot,hash,NULL)==GE_MP_STATE_OK);
}

static void actual_bg_state(const char *path)
{
    GeAssetPack pack;
    GeMpRegistry registry;
    GeMpRegistryEntry entry;
    const uint32_t key=ge_mp_registry_key(GE_MP_BG_KIND,GE_MP_BG_NAMESPACE,GE_MP_BG_SLOT);
    assert(ge_asset_pack_open(&pack,path)==GE_ASSET_PACK_OK);
    assert(ge_mp_registry_init(&registry,&entry,1)==GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    for (size_t stage_index=0;stage_index<GE_STAGE_COUNT+GE_MULTIPLAYER_STAGE_COUNT;++stage_index) {
        const GeStageAssetDescriptor *stage=stage_index<GE_STAGE_COUNT
            ? ge_stage_asset_descriptor((GeStageId)stage_index)
            : ge_stage_asset_multiplayer(stage_index-GE_STAGE_COUNT);
        size_t size;
        uint8_t *bytes=read_asset(&pack,stage->background_path,&size);
        GeDamWorld world;
        assert(ge_dam_world_parse(bytes,size,&world)==GE_DAM_WORLD_OK); free(bytes);
        bytes=read_asset(&pack,stage->room_bounds_path,&size);
        assert(size==80+world.room_count*24);
        GeOriginalBgRoomBounds bounds[GE_BG_GEOMETRY_MAX_ROOMS];
        memcpy(bounds,bytes+80,world.room_count*24); free(bytes);
        GeOriginalBgGeometryReport report;
        GeOriginalBgGeometry *owner=ge_original_bg_geometry_load(&pack,stage,&world,bounds,&report);
        assert(owner!=NULL); ge_original_bg_geometry_bind(owner);
        GeMpStateBgSnapshot snapshot={0};
        uint64_t id;
        assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);
        assert(ge_mp_state_bg_init(&snapshot,owner)==GE_MP_STATE_OK);
        assert(ge_mp_state_bg_capture(&snapshot,owner,&registry,id)==GE_MP_STATE_OK);
        uint8_t before[32],after[32]; state_digest(&snapshot,&registry,before);
        size_t breaks=0;
        for (uint32_t room=1;room<world.room_count;++room) {
            GeOriginalBgRoomGeometry *geometry=&owner->room_geometry[room];
            for (size_t index=0;index<geometry->command_counts[0];++index) {
                const GeOriginalBgCommandInfo *command_info=&geometry->commands[0][index];
                if (!ge_bg_is_light_exact(command_info->texture) || command_info->triangle_count==0) continue;
                Gfx *gfx=ge_bg_gfx_at(geometry->blobs.primary_gdl,(ptrdiff_t)index);
                for (unsigned triangle=0;triangle<command_info->triangle_count;++triangle) {
                    u32 type=((uint8_t *)gfx)[0]==0xbf?0:triangle+1;
                    s32 indices[3]; ge_bg_light_indices_exact(gfx,type,&indices[0],&indices[1],&indices[2]);
                    if (indices[0]==indices[1] || indices[1]==indices[2] || indices[0]==indices[2]) continue;
                    assert(ge_original_bg_geometry_light_break(owner,gfx,type,(int32_t)room));
                    ++breaks;
                }
            }
        }
        assert(ge_mp_state_bg_capture(&snapshot,owner,&registry,id)==GE_MP_STATE_OK);
        state_digest(&snapshot,&registry,after);
        assert((breaks==0)==(memcmp(before,after,32)==0));
        printf("E7 %s: %u typed point colours, %zu light hits captured\n",stage->key,snapshot.values.point_count,breaks);
        assert(ge_mp_registry_release(&registry,id)==GE_MP_STATE_OK);
        ge_mp_state_bg_close(&snapshot); ge_original_bg_geometry_close(owner);
    }
    ge_asset_pack_close(&pack);
}

int main(int argc, char **argv)
{
    GeOriginalBgGeometry *a=state_owner(0), *b=state_owner(1);
    GeMpRegistryEntry entry;
    GeMpRegistry registry;
    uint64_t id,second_id;
    uint32_t key=ge_mp_registry_key(GE_MP_BG_KIND,GE_MP_BG_NAMESPACE,GE_MP_BG_SLOT);
    assert(ge_mp_registry_init(&registry,&entry,1)==GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&id)==GE_MP_STATE_OK);
    GeMpStateBgSnapshot first={0}, second={0};
    ge_original_bg_geometry_bind(a);
    assert(ge_mp_state_bg_init(&first,a)==GE_MP_STATE_OK);
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    uint8_t original[32],equivalent[32],changed[32];
    state_digest(&first,&registry,original);
    ge_original_bg_geometry_bind(b);
    assert(ge_mp_state_bg_init(&second,b)==GE_MP_STATE_OK);
    assert(ge_mp_state_bg_capture(&second,b,&registry,id)==GE_MP_STATE_OK);
    state_digest(&second,&registry,equivalent);
    assert(memcmp(original,equivalent,32)==0);
    assert(first.values.fixture_rooms[0]==1 && first.values.fixture_rooms[1]==2);
    assert(first.values.fixture_starts[0]==1 && first.values.fixture_ends[0]==4);
    assert(first.rgba[0]==UINT32_C(0xf0f0f0f0));
    assert(ge_mp_state_bg_capture(&second,a,&registry,id)==GE_MP_STATE_STALE && second.record_count==0);
    ge_original_bg_geometry_bind(a);
    Gfx *triangle=ge_bg_gfx_at(a->room_geometry[1].blobs.primary_gdl,2);
    assert(ge_original_bg_geometry_light_break(a,triangle,0,1));
    /* The already captured first snapshot is durable despite native mutation. */
    state_digest(&first,&registry,equivalent); assert(memcmp(original,equivalent,32)==0);
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,changed); assert(memcmp(original,changed,32)!=0);
    assert(first.rgba[0]==UINT32_C(0x3c3c3c3c));
    assert(first.values.darkened_cursor==3 && first.values.darkened_rooms[0]==1);
    assert(ge_original_bg_geometry_light_break(a,triangle,0,1));
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)==0);
    /* Colours must contribute independently of the darkened table. Native
     * live colours outlast entries overwritten in its circular history. */
    for (size_t channel=0;channel<4;++channel) {
        a->rooms[1].vertices[0].v.cn[channel]^=1;
        ((uint8_t *)a->room_geometry[1].blobs.point_table)[12+channel]^=1;
        (&a->room_geometry[1].collision_vertices[0].r)[channel]^=1;
        assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
        state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)!=0);
        a->rooms[1].vertices[0].v.cn[channel]^=1;
        ((uint8_t *)a->room_geometry[1].blobs.point_table)[12+channel]^=1;
        (&a->room_geometry[1].collision_vertices[0].r)[channel]^=1;
    }
    struct s_darkened_light record=a->darkened[0]; a->darkened[0]=a->darkened[1]; a->darkened[1]=record;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)!=0);
    record=a->darkened[0]; a->darkened[0]=a->darkened[1]; a->darkened[1]=record;
    /* Circular-buffer cursor and fixture physical order affect later native
     * reads even when the same set of vertices is currently dark. */
    a->darkened_cursor=4;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)!=0);
    a->darkened_cursor=3;
    s_lightfixture swapped=a->fixtures[0]; a->fixtures[0]=a->fixtures[1]; a->fixtures[1]=swapped;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)!=0);
    swapped=a->fixtures[0]; a->fixtures[0]=a->fixtures[1]; a->fixtures[1]=swapped;
    a->darkened[0].room_index=138;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_INCOMPLETE && first.record_count==0);
    a->darkened[0].room_index=1;
    a->fixtures[0].ptr_start_pertinent_DL=(Gfx *)(uintptr_t)1;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_INCOMPLETE && first.record_count==0);
    a->fixtures[0].ptr_start_pertinent_DL=ge_bg_gfx_at(a->room_geometry[1].blobs.primary_gdl,1);
    a->rooms[1].vertices[0].v.cn[3]^=1;
    --first.rgba_capacity;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_CAPACITY && first.record_count==0);
    ++first.rgba_capacity;
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_INCOMPLETE && first.record_count==0);
    a->rooms[1].vertices[0].v.cn[3]^=1;
    assert(ge_mp_registry_release(&registry,id)==GE_MP_STATE_OK);
    assert(ge_mp_state_bg_capture(&first,a,&registry,id)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_acquire(&registry,key,&second_id)==GE_MP_STATE_OK && second_id!=id);
    assert(ge_mp_state_bg_capture(&first,a,&registry,second_id)==GE_MP_STATE_OK);
    state_digest(&first,&registry,equivalent); assert(memcmp(changed,equivalent,32)!=0);
    ge_original_bg_geometry_bind(NULL);
    assert(ge_mp_state_bg_capture(&first,a,&registry,second_id)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_release(&registry,second_id)==GE_MP_STATE_OK);
    ge_original_bg_geometry_close(a); ge_original_bg_geometry_close(b);
    ge_mp_state_bg_close(&first); ge_mp_state_bg_close(&second);
    if (argc>1) { FILE *file=fopen(argv[1],"wb"); assert(file!=NULL); assert(fwrite(changed,1,32,file)==32); assert(fclose(file)==0); }
    puts("BG E7 export: pointer/descriptor independence, owned capture, colour/cursor/fixture sensitivity, stale and invalid-reference rejection pass");
    if (argc>2) actual_bg_state(argv[2]);
    return 0;
}
