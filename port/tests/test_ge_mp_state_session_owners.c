#define main ge_mp_base_session_fixture_main
#include "test_ge_mp_state_session.c"
#undef main
#include "ge_original_stage_setup.h"

static GeOriginalStageActiveProps active;
static GeOriginalStageSetupRuntime setup_owner;
static stagesetup setup_native;
static GeOriginalMultiplayerHud hud;
static PropRecord object;
static s32 prop_words[1];
static AIListRecord ai_records[1];
static uint64_t prop_ids[4],setup_id,chr_id,propdefs_id,ai_id;
static unsigned owner_mutations;
/* This fixture exercises the actual compose/close and zero-clock admission
 * path only. Reaching gameplay dispatch is an error, never a fake tick. */
void ge_original_dam_guard_all_chr_tick_exact(void){assert(!"unexpected gameplay dispatch");}
void ge_original_dam_guard_props_tick_exact(void){assert(!"unexpected gameplay dispatch");}
static GeMpStateStatus owner_reference(void *context,uint16_t field,unsigned index,
    GeMpSessionReferenceKind kind,const void *native,size_t extent,GeMpSessionReference *ref)
{
    if(field==missing_field)return GE_MP_STATE_INCOMPLETE;
    ref->offset=0;
    if(kind==GE_MP_SESSION_REF_PROP)for(unsigned i=0;i<4;++i)if(native==&props[i]) {
        ref->actor_id=prop_ids[i];return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_SETUP_OWNER && native==&setup_owner && extent==sizeof(setup_owner)) {
        ref->actor_id=setup_id;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_CHR_POOL && native==chrs && extent<=sizeof(chrs)) {
        ref->actor_id=chr_id;return GE_MP_STATE_OK;
    }
    if(kind==GE_MP_SESSION_REF_PROPDEFS && native==prop_words) {ref->actor_id=propdefs_id;return GE_MP_STATE_OK;}
    if(kind==GE_MP_SESSION_REF_AILISTS && native==ai_records) {ref->actor_id=ai_id;return GE_MP_STATE_OK;}
    return reference(context,field,index,kind,native,extent,ref);
}
static const GeMpSessionProviders owner_providers={NULL,owner_reference};
static void owner_capture(unsigned gaps)
{
    GeOriginalStageActiveProps saved=active;GeOriginalMultiplayerHud old_hud=hud;
    uint64_t rng=g_randomSeed,chr_rng=g_chrObjRandomSeed;
    assert(ge_mp_state_session_capture(&registry,&owner_providers,&schedule,&snapshot)==GE_MP_STATE_INCOMPLETE);
    assert(snapshot.total_gaps>=3);
    if(gaps)assert(snapshot.total_gaps==gaps);
    assert(!memcmp(&saved,&active,sizeof(active)) && !memcmp(&old_hud,&hud,sizeof(hud)));
    assert(rng==g_randomSeed && chr_rng==g_chrObjRandomSeed);
}
static void owner_fingerprint(uint8_t digest[32])
{unsigned char bytes[4096];owner_capture(0);assert(ge_mp_sha256(bytes,payload(bytes),digest)==GE_MP_STATE_OK);}
static void compose(unsigned count)
{
    void *player_props[4];for(unsigned i=0;i<count;++i)player_props[i]=&props[i];
    GeOriginalStageActivePropInput input={0,&object,GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    assert(ge_original_stage_active_props_compose_players(&active,&setup_owner,player_props,count,
        chrs,count,&input,1)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    schedule.active_props=&active;schedule.hud=&hud;
}
static void owner_setup(unsigned count,unsigned peer)
{
    setup(count,peer);memset(&hud,0,sizeof(hud));memset(&object,0,sizeof(object));object.type=PROP_TYPE_OBJ;
    setup_native=g_CurrentSetup;setup_native.propDefs=(PropDefHeaderRecord *)prop_words;setup_native.ailists=ai_records;
    setup_owner=(GeOriginalStageSetupRuntime){.setup=&setup_native,.prop_record_count=1};
    for(unsigned i=0;i<count;++i)prop_ids[i]=acquire(0xE3,i);
    setup_id=acquire(0x21,4);chr_id=acquire(0x21,5);propdefs_id=acquire(0x21,6);ai_id=acquire(0x21,7);
    compose(count);owner_capture(3);
    assert(snapshot.values.active_player_count==count && snapshot.values.active_count==1);
    for(unsigned i=0;i<count;++i)assert(snapshot.values.active_players[i]==prop_ids[i]);
    assert(snapshot.values.active_setup==setup_id && snapshot.values.active_chrs==chr_id);
}
static void owner_close(void)
{
    ge_original_stage_active_props_close(&active);
    schedule.active_props=NULL;schedule.hud=NULL;teardown();
}
#define OWNER_MUTATE(expression,value) do {uint8_t a[32],b[32];owner_fingerprint(a); \
    __typeof__(expression) saved=(expression);(expression)=(value);owner_fingerprint(b); \
    assert(memcmp(a,b,32));(expression)=saved;++owner_mutations;}while(0)
static void owner_fields(unsigned count)
{
    OWNER_MUTATE(active.pre_ticks,19);OWNER_MUTATE(active.ticks,17);OWNER_MUTATE(active.paused_ticks,11);
    OWNER_MUTATE(active.count,99);OWNER_MUTATE(active.chr_count,1);OWNER_MUTATE(active.bound,0);
    OWNER_MUTATE(active.pre_tick_pending,1);OWNER_MUTATE(active.player_count,1);OWNER_MUTATE(active.player_tick_mask,128);
    OWNER_MUTATE(active.last_binding_mismatch,32);OWNER_MUTATE(active.ordered,NULL);
    for(unsigned i=0;i<count;++i)OWNER_MUTATE(active.player_props[i],&props[(i+1)%count]);
    OWNER_MUTATE(active.setup,NULL);OWNER_MUTATE(active.chrs,NULL);OWNER_MUTATE(g_ChrSlots,NULL);
    OWNER_MUTATE(g_NumChrSlots,-1);OWNER_MUTATE(g_CurrentSetup.propDefs,NULL);OWNER_MUTATE(g_CurrentSetup.ailists,NULL);
    OWNER_MUTATE(hud.tick,17);OWNER_MUTATE(hud.player_count,4);OWNER_MUTATE(hud.initialized,1);
    OWNER_MUTATE(hud.prepared,1);OWNER_MUTATE(hud.captured,1);OWNER_MUTATE(hud.failed,1);OWNER_MUTATE(hud.intro_camera,1);
    /* Consumed composition entries and display payloads never influence a
     * later wrapper admission. The native linked list remains untouched. */
    uint8_t a[32],b[32];owner_fingerprint(a);
    GeOriginalStageActivePropInput old=active.ordered[0];
    active.ordered[0]=(GeOriginalStageActivePropInput){SIZE_MAX,NULL,(GeOriginalStageActivePropKind)127};
    active.player_prop=NULL;memset(&hud.players,0xA5,sizeof(hud.players));owner_fingerprint(b);
    assert(!memcmp(a,b,32));active.ordered[0]=old;active.player_prop=&props[0];memset(hud.players,0,sizeof(hud.players));
    GeOriginalStageActivePropInput *saved=active.ordered;active.ordered=(void*)(uintptr_t)0x1234;
    owner_capture(4);assert(snapshot.values.active_ordered_present==1);active.ordered=saved;
    const uint16_t refs[]={310,311,313,315,318,320};
    for(unsigned i=0;i<sizeof(refs)/sizeof(refs[0]);++i) {
        missing_field=refs[i];owner_capture(0);assert(!snapshot.fields[missing_field-256].count);
        assert(snapshot.coverage.unresolved_references);
    }
    missing_field=0;assert(ge_mp_registry_release(&registry,chr_id)==GE_MP_STATE_OK);
    owner_capture(0);assert(snapshot.coverage.unresolved_references);
    assert(ge_mp_registry_acquire(&registry,ge_mp_registry_key(0x21,255,5),&chr_id)==GE_MP_STATE_OK);owner_capture(3);
}
static void lifecycle(unsigned count)
{
    g_ClockTimer=0;g_GlobalTimer=1;g_GlobalTimerDelta=0;
    assert(ge_original_stage_active_props_pre_tick_exact(&active)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_stage_active_props_tick_exact(&active)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    owner_capture(3);assert(snapshot.values.active_paused_ticks==1 && !snapshot.values.active_pending);
    /* Actual HUD begin rejects missing/duplicate frame admission. Capturing
     * this unfinished phase reports a gap, without pretending it committed. */
    assert(ge_original_multiplayer_hud_begin_frame(&hud,0,count));owner_capture(4);
    assert(snapshot.values.hud_initialized==1 && snapshot.values.hud_player_count==count);
    assert(!ge_original_multiplayer_hud_begin_frame(&hud,1,count));
    memset(&hud,0,sizeof(hud));
    GeOriginalStageActiveProps other={0};void *player_props[4];
    for(unsigned i=0;i<count;++i)player_props[i]=&props[i];
    assert(ge_original_stage_active_props_compose_players(&other,&setup_owner,player_props,count,chrs,count,NULL,0)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    owner_capture(4);assert(!snapshot.fields[299-256].count); /* old owner cannot claim replacement */
    ge_original_stage_active_props_close(&active);
    GeOriginalStageActivePropsNativeView view;
    assert(ge_original_stage_active_props_native_view(&other,&view) && view.owner==&other);
    assert(!ge_original_stage_active_props_native_view(&active,&view));
    ge_original_stage_active_props_close(&other);assert(!ge_original_stage_active_props_native_view(&other,&view));
    compose(count);owner_capture(3);assert(!snapshot.values.active_paused_ticks && !snapshot.values.active_ticks);
}
int main(int argc,char **argv)
{
    assert(argc==4);unsigned count=(unsigned)atoi(argv[1]),peer=(unsigned)atoi(argv[2]);uint8_t a[32],b[32];
    if(peer){owner_setup(4,peer);lifecycle(4);owner_close();}
    owner_setup(count,peer);owner_fingerprint(a);owner_fields(count);lifecycle(count);
    owner_close();owner_setup(4,peer);owner_close();owner_setup(count,peer);owner_fingerprint(b);assert(!memcmp(a,b,32));
    unsigned char bytes[4096];owner_capture(3);FILE *file=fopen(argv[3],"wb");assert(file);
    size_t n=payload(bytes);assert(fwrite(bytes,1,n,file)==n && !fclose(file));incomplete_digest();owner_close();
    printf("kind0 owners: %up peer%u, %u typed mutations, native compose/close/replacement + paused admission, HUD partial-frame rejection and history convergence passed\n",count,peer,owner_mutations);
}
