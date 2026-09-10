#include "ge_mp_state_visibility.h"
#include "ge_original_bg_visibility_internal.h"
#include "ge_original_bg_visibility_state.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern Unk80081600 ge_bg_visibility_interpreter_state;
GeOriginalBgVisibilityStatus ge_renderer_private_ge_original_bg_visibility_run(
    const GeOriginalBgVisibilityInput *,GeOriginalBgVisibilityResult *);
static GeMpStateVisibilitySnapshot snapshot, durable;
static GeMpRegistry registry;
static GeMpRegistryEntry registry_entry;
static uint64_t actor;
static FILE *digests;

static void digest(const GeMpStateVisibilitySnapshot *state, uint8_t hash[32])
{
    uint32_t global=1;
    GeMpFieldSchema global_schema={256,GE_MP_STATE_U32,0,1,1};
    GeMpStateField global_field={256,GE_MP_STATE_U32,1,&global};
    GeMpRecordSchema records_schema[2]={{0,GE_MP_SYSTEM_SESSION,&global_schema,1},*ge_mp_state_visibility_schema()};
    /* Completeness here attests only this isolated fixture's declared domains. */
    GeMpStateSchema schema={1,GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_OBJECTS,1,records_schema,2};
    GeMpStateRecord records[2]={{0,0,&global_field,1},state->record};
    GeMpCanonicalSnapshot canonical={.session=17,.tick=1,.enabled_systems=schema.registered_systems,
        .schema=&schema,.registry=&registry,.records=records,.record_count=2,
        .coverage={schema.registered_systems,0,0}};
    memset(canonical.build_sha256,1,32); memset(canonical.content_sha256,2,32);
    memset(canonical.rules_sha256,3,32);
    assert(state->record_count==1 && ge_mp_state_sha256(&canonical,hash,NULL)==GE_MP_STATE_OK);
}

static void record_hash(const char *key,const uint8_t hash[32])
{
    fprintf(digests,"%s ",key);
    for (size_t i=0;i<32;++i) fprintf(digests,"%02x",hash[i]);
    fputc('\n',digests);
}

static void be32(uint8_t *p,uint32_t n)
{ p[0]=(uint8_t)(n>>24);p[1]=(uint8_t)(n>>16);p[2]=(uint8_t)(n>>8);p[3]=(uint8_t)n; }
static uint32_t read32(const uint8_t *p)
{ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
static void bef(uint8_t *p,float f) { uint32_t bits;memcpy(&bits,&f,4);be32(p,bits); }
static void cmd(uint8_t *p,uint8_t type,uint8_t len,uint32_t arg)
{ p[0]=type;p[1]=len;be32(p+4,arg); }

static GeDamVisibilityRuntime fixture(void)
{
    GeDamVisibilityRuntime r={0};
    r.background_size=212; r.background=calloc(1,r.background_size); assert(r.background);
    r.room_count=3; r.level_index=13; r.level_scale=r.visibility_scale=1;
    r.near_distance=1; r.far_distance=1000; r.loaded=1;
    uint8_t *b=r.background;
    be32(b+8,0x0f000054);be32(b+12,0x0f000014);
    cmd(b+20,VISOP_FORCE_VISIBLE,1,0);cmd(b+28,VISOP_ADD_VISIBLE_ROOM,2,0);
    cmd(b+36,101,0,2);cmd(b+44,VISOP_PUSH,1,42);
    cmd(b+52,VISOP_PRELOAD_ROOM_RANGE,3,0);cmd(b+60,101,0,1);cmd(b+68,101,0,2);
    cmd(b+76,VISOP_END,1,0);
    for (size_t i=0;i<2;++i) {
        size_t offset=108+i*52;
        be32(b+84+i*8,0x0f000000+(uint32_t)offset);
        b[84+i*8+4]=1;b[84+i*8+5]=2;b[offset]=4;
        const float xy[4][2]={{-2,-2},{-2,2},{2,2},{2,-2}};
        for (size_t j=0;j<4;++j) {
            bef(b+offset+4+j*12,xy[j][0]);bef(b+offset+8+j*12,xy[j][1]);
            bef(b+offset+12+j*12,-10-(float)i*5);
        }
    }
    for (size_t i=0;i<3;++i) {
        r.room_bounds[i]=(GeOriginalBgRoomBounds){{-5,-5,-30},{5,5,-1}};
    }
    r.program=ge_original_bg_visibility_program_create(b,r.background_size,r.room_count,&r.last_original_status);
    assert(r.program && r.last_original_status==GE_ORIGINAL_BG_VISIBILITY_OK);
    return r;
}

static GeOriginalBgVisibilityInput input(const GeDamVisibilityRuntime *r,const GeOriginalBgVisibilityProviders *p)
{
    GeOriginalBgVisibilityInput in={.background=r->background,.background_size=r->background_size,
        .room_bounds=r->room_bounds,.room_count=r->room_count,.level_index=r->level_index,.current_room=1,
        .level_scale=r->level_scale,.visibility_scale=r->visibility_scale,.near_distance=r->near_distance,
        .far_distance=r->far_distance,.vertical_fov_degrees=60,.aspect_ratio=4.0f/3.0f,
        .view_width=320,.view_height=240,.providers=p,.program=r->program};
    for (size_t i=0;i<4;++i) in.world_to_screen[i][i]=1;
    return in;
}

static void capture(const GeDamVisibilityRuntime *r,const uint8_t *controls,size_t count,uint8_t hash[32])
{
    GeMpStateStatus status=ge_mp_state_visibility_capture(&snapshot,r,controls,count,&registry,actor);
    if (status!=GE_MP_STATE_OK) fprintf(stderr,"E9 reject status%d gaps%u refs%u\n",status,
        snapshot.coverage.unclassified_fields,snapshot.coverage.unresolved_references);
    assert(status==GE_MP_STATE_OK);
    digest(&snapshot,hash);
}
static void reject(const GeDamVisibilityRuntime *r,const uint8_t *controls,size_t count)
{
    assert(ge_mp_state_visibility_capture(&snapshot,r,controls,count,&registry,actor)!=GE_MP_STATE_OK);
    assert(snapshot.record_count==0 && snapshot.record.actor_id==0 && snapshot.record.fields==NULL);
    assert(snapshot.coverage.enumerated_systems==0);
}
static uint8_t preload(void *context,uint8_t room) { (void)room; return *(uint8_t *)context; }

static void focused(void)
{
    GeDamVisibilityRuntime r=fixture();
    uint8_t controls[2]={0},result_code=0;
    GeOriginalBgVisibilityProviders providers={&result_code,preload,controls,2};
    GeOriginalBgVisibilityInput in=input(&r,&providers);
    GeOriginalBgVisibilityResult result;
    uint8_t baseline[32],changed[32],saved[32];
    reject(&r,controls,2); /* No successful publication yet. */
    D_80044858=0;
    assert(ge_original_bg_visibility_run(&in,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(result.preload_request_count==2);
    capture(&r,controls,2,baseline);record_hash("fixture",baseline);
    assert(snapshot.values.draw_count>=2 && snapshot.values.command_count==8);
    assert(ge_mp_state_visibility_capture(&durable,&r,controls,2,&registry,actor)==GE_MP_STATE_OK);
    ++D_80044858;capture(&r,controls,2,changed);assert(memcmp(baseline,changed,32));--D_80044858;
    digest(&durable,saved);assert(!memcmp(saved,baseline,32));
    ge_bg_visibility_interpreter_state.unk0.min.x+=0.25f;
    capture(&r,controls,2,changed);assert(memcmp(baseline,changed,32));
    ge_bg_visibility_interpreter_state.unk0.min.x-=0.25f;
    int32_t depth=dword_CODE_bss_8007FFA0[0].unk1;
    dword_CODE_bss_8007FFA0[0].unk1=-7;
    capture(&r,controls,2,changed);assert(snapshot.values.draw_depths[0]==-7 && memcmp(baseline,changed,32));
    dword_CODE_bss_8007FFA0[0].unk1=depth;
    s_bound_info first=dword_CODE_bss_8007FFA0[0];
    dword_CODE_bss_8007FFA0[0]=dword_CODE_bss_8007FFA0[1];dword_CODE_bss_8007FFA0[1]=first;
    capture(&r,controls,2,changed);assert(memcmp(baseline,changed,32));
    dword_CODE_bss_8007FFA0[1]=dword_CODE_bss_8007FFA0[0];dword_CODE_bss_8007FFA0[0]=first;
    num_visible_rooms_in_cur_global_vis_packet=2;
    list_visible_rooms_in_cur_global_vis_packet[0]=1;list_visible_rooms_in_cur_global_vis_packet[1]=2;
    capture(&r,controls,2,saved);
    list_visible_rooms_in_cur_global_vis_packet[0]=2;list_visible_rooms_in_cur_global_vis_packet[1]=1;
    capture(&r,controls,2,changed);assert(memcmp(saved,changed,32));
    /* Re-run the same camera with local preload/residency disagreement. Only
     * the per-call diagnostic request count changes, not canonical fields. */
    result_code=1;D_80044858=0;
    assert(ge_original_bg_visibility_run(&in,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(result.preload_request_count==1);
    capture(&r,controls,2,changed);assert(!memcmp(baseline,changed,32));
    /* Private eye storage, including the formerly function-static bbox, is
     * independent even when using the same immutable relocated program. */
    GeOriginalBgVisibilityInput eye=in;eye.current_room=2;eye.projection_offset_px[0]=200;
    assert(ge_renderer_private_ge_original_bg_visibility_run(&eye,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    capture(&r,controls,2,changed);assert(!memcmp(baseline,changed,32));
    assert(ge_original_bg_set_portal_open(0,0,controls,2));
    capture(&r,controls,2,changed);assert(memcmp(baseline,changed,32));
    assert(ge_original_bg_set_portal_open(0,1,controls,2));
    capture(&r,controls,2,changed);assert(!memcmp(baseline,changed,32));
    controls[0]^=1;reject(&r,controls,2);controls[0]^=1;
    ++r.background;reject(&r,controls,2);--r.background;
    bg_portal_entry *polygon=g_BgPortals[0].offset_portal;
    g_BgPortals[0].offset_portal=NULL;reject(&r,controls,2);g_BgPortals[0].offset_portal=polygon;
    polygon->numPoints=10;reject(&r,controls,2);polygon->numPoints=4;
    polygon->point.x+=1;capture(&r,controls,2,changed);assert(memcmp(baseline,changed,32));
    polygon->point.x-=1;
    s32 *commands=dword_CODE_bss_8007FF90;
    dword_CODE_bss_8007FF90=NULL;reject(&r,controls,2);dword_CODE_bss_8007FF90=commands;
    player *projection=g_CurrentPlayer;
    g_CurrentPlayer=NULL;reject(&r,controls,2);g_CurrentPlayer=projection;
    ++g_BgPortalQueueWriteIndex;reject(&r,controls,2);--g_BgPortalQueueWriteIndex;
    g_BgRoomInfo[1].model_bin_loaded=1;reject(&r,controls,2);g_BgRoomInfo[1].model_bin_loaded=0;
    float box=ge_bg_visibility_interpreter_state.unk0.min.x;
    ge_bg_visibility_interpreter_state.unk0.min.x=NAN;reject(&r,controls,2);
    ge_bg_visibility_interpreter_state.unk0.min.x=box;
    /* Semantically dead queue/cache tails may contain arbitrary old values. */
    g_BgPortalQueue[499].sp10[0]=NAN;table_for_portals[199].bbox.min.x=NAN;
    capture(&r,controls,2,changed);assert(!memcmp(baseline,changed,32));
    GeDamVisibilityRuntime copy=fixture();GeOriginalBgVisibilityInput same=input(&copy,&providers);
    D_80044858=0;assert(ge_original_bg_visibility_run(&same,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    capture(&copy,controls,2,changed);assert(!memcmp(baseline,changed,32));
    reject(&r,controls,2);
    ge_dam_visibility_runtime_close(&r); /* Closing an old program keeps the live one. */
    capture(&copy,controls,2,changed);
    same.view_width=0;assert(ge_original_bg_visibility_run(&same,&result)!=GE_ORIGINAL_BG_VISIBILITY_OK);
    reject(&copy,controls,2);
    ge_dam_visibility_runtime_close(&copy);reject(&copy,controls,2);
}

static void actual(const char *path)
{
    GeAssetPack pack;
    assert(ge_asset_pack_open(&pack,path)==GE_ASSET_PACK_OK);
    size_t passes=0;
    for (size_t index=0;index<GE_STAGE_COUNT+GE_MULTIPLAYER_STAGE_COUNT;++index) {
        const GeStageAssetDescriptor *stage=index<GE_STAGE_COUNT
            ? ge_stage_asset_descriptor((GeStageId)index):ge_stage_asset_multiplayer(index-GE_STAGE_COUNT);
        GeDamVisibilityRuntime runtime;
        assert(ge_dam_visibility_runtime_load_for_stage(&pack,stage,&runtime)==GE_DAM_VISIBILITY_RUNTIME_OK);
        uint8_t controls[200]={0};size_t count=0;
        size_t offset=read32(runtime.background+8)-0x0f000000;
        while (read32(runtime.background+offset+count*8)!=0) {
            assert(count<200);controls[count]=runtime.background[offset+count*8+6];++count;
        }
        GeOriginalBgVisibilityProviders providers={.portal_controls=controls,.portal_control_count=count};
        GeOriginalBgVisibilityInput in=input(&runtime,&providers);
        GeOriginalBgVisibilityResult result;uint8_t hash[32]={0};
        for (size_t room=1;room<runtime.room_count;++room) {
            in.current_room=(uint8_t)room;
            for (size_t axis=0;axis<3;++axis) {
                in.player_position[axis]=(runtime.room_bounds[room].minimum[axis]
                    +runtime.room_bounds[room].maximum[axis])*0.5f;
                in.world_to_screen[3][axis]=-in.player_position[axis];
            }
            runtime.last_original_status=ge_original_bg_visibility_run(&in,&result);
            assert(runtime.last_original_status==GE_ORIGINAL_BG_VISIBILITY_OK);
            capture(&runtime,controls,count,hash);++passes;
        }
        record_hash(stage->key,hash);
        printf("E9 %s: %zu authored rooms, %zu portals captured\n",stage->key,runtime.room_count,count);
        ge_dam_visibility_runtime_close(&runtime);
    }
    ge_asset_pack_close(&pack);
    printf("E9 captured %zu canonical room-camera passes across all32 stages\n",passes);
}

int main(int argc,char **argv)
{
    assert(argc>=2);digests=fopen(argv[1],"wb");assert(digests);
    uint32_t key=ge_mp_registry_key(GE_MP_VISIBILITY_KIND,GE_MP_VISIBILITY_NAMESPACE,GE_MP_VISIBILITY_SLOT);
    assert(ge_mp_registry_init(&registry,&registry_entry,1)==GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,key,&actor)==GE_MP_STATE_OK);
    focused();if(argc>2)actual(argv[2]);
    assert(ge_mp_registry_release(&registry,actor)==GE_MP_STATE_OK);
    GeDamVisibilityRuntime r={0};reject(&r,NULL,0);
    assert(fclose(digests)==0);
    puts("E9 typed canonical visibility, references, private isolation and failure publication checks passed");
    return 0;
}
