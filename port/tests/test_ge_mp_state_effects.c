/* Reuse explicit resource boundaries and real constructors/update regressions.
 * This is effects-owner acceptance, never a complete simulation fixture. */
#define main native_runtime_regression
#include "test_ge_original_effect_runtime.c"
#undef main
#include "ge_mp_state_effects.h"
#include "ge_original_covert_modem_projectile.h"
#include "game/image_bank.h"

static GeMpRegistry registry;
static GeMpRegistryEntry entries[800];
static GeMpStateEffectsOwner owner;
static GeMpStateEffectsSnapshot output;
static PropRecord fixture_props[2];
static ObjectRecord fixture_object;
static struct sImageTableEntry fixture_images[2];
static unsigned audio_identity,model_identity;
static uint64_t resource_ids[6];
extern Projectile g_Projectiles[20];
extern struct rgba_u8 g_BulletSparkColors[8];
void fixture_projectile_reset(Projectile *);
void fixture_gun_projectile_fallback(ObjectRecord *,coord3d *,int);

static GeMpStateStatus reference(void *context,GeMpEffectReferenceKind kind,const void *p,uint64_t *id)
{
    (void)context;
    const void *pointers[]={&fixture_props[0],&fixture_props[1],&fixture_object,
        &audio_identity,fixture_images,&model_identity};
    const unsigned kinds[]={GE_MP_EFFECT_REF_PROP,GE_MP_EFFECT_REF_PROP,GE_MP_EFFECT_REF_OBJECT,
        GE_MP_EFFECT_REF_AUDIO,GE_MP_EFFECT_REF_IMAGE_TABLE,GE_MP_EFFECT_REF_MODEL_HEADER};
    for(unsigned i=0;i<6;++i)if(kind==kinds[i] && p==pointers[i]) {
        *id=resource_ids[i];return GE_MP_STATE_OK;
    }
    return GE_MP_STATE_INCOMPLETE;
}
static const GeMpStateEffectsProviders providers={NULL,reference};
static void registry_init(void)
{
    assert(ge_mp_registry_init(&registry,entries,800)==GE_MP_STATE_OK);
    for(unsigned i=0;i<6;++i) {
        uint32_t key=ge_mp_registry_key(0xA0,(uint8_t)i,0);
        assert(ge_mp_registry_define(&registry,key)==GE_MP_STATE_OK);
        assert(ge_mp_registry_acquire(&registry,key,&resource_ids[i])==GE_MP_STATE_OK);
    }
}
static void export_ok(void)
{
    GeMpStateStatus status=ge_mp_state_effects_export(&owner,&providers,&output);
    if(status!=GE_MP_STATE_OK)fprintf(stderr,"effects export status %d unknown %u refs %u kinds %x\n",
        status,output.coverage.unclassified_fields,output.coverage.unresolved_references,output.unresolved_kinds);
    assert(status==GE_MP_STATE_OK);
    assert(output.record_count==owner.count);
    assert(output.coverage.enumerated_systems==(GE_MP_SYSTEM_EXPLOSIONS|GE_MP_SYSTEM_PROJECTILES));
    const GeMpRecordSchema *schema=ge_mp_state_effects_schema();
    for(unsigned r=0;r<owner.count;++r) {
        assert(output.records[r].actor_id==owner.ids[r]);
        if(r)assert(output.records[r-1].actor_id<output.records[r].actor_id);
        for(unsigned f=0;f<GE_MP_EFFECTS_FIELDS;++f) {
            const GeMpStateField *field=&output.fields[r][f];
            assert(field->id==schema->fields[f].id && field->type==schema->fields[f].type);
            assert(field->count>=schema->fields[f].min_count && field->count<=schema->fields[f].max_count);
        }
    }
}
/* Typed payload observation only. The canonical state API must reject the
 * intentionally incomplete global schema below, even after E8 exports. */
static uint64_t fingerprint(unsigned component)
{
    static const unsigned widths[]={0,1,2,4,8,1,2,4,8,4,1,8,4};
    uint64_t hash=UINT64_C(14695981039346656037);export_ok();
    for(unsigned r=owner.offsets[component];r<owner.offsets[component]+owner.counts[component];++r)
        for(unsigned f=0;f<GE_MP_EFFECTS_FIELDS;++f) {
            const GeMpStateField *field=&output.fields[r][f];
            const unsigned char *bytes=field->values;
            hash=(hash^field->count)*UINT64_C(1099511628211);
            for(size_t n=0;n<widths[field->type]*field->count;++n)
                hash=(hash^bytes[n])*UINT64_C(1099511628211);
        }
    return hash;
}
#define MUTATE(component,member) do { \
    __typeof__(member) saved=(member);uint64_t before=fingerprint(component); \
    (member)=(saved)+1;assert(fingerprint(component)!=before);(member)=saved; \
} while(0)
static void begin(unsigned count)
{
    assert(ge_original_effect_runtime_reset(count,LEVELID_LIBRARY));
    ge_original_covert_modem_projectile_reset();
    assert(ge_mp_state_effects_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(ge_mp_state_effects_begin(&owner,&registry)==GE_MP_STATE_STALE);
    assert(owner.counts[GE_MP_EFFECT_PARTICLE]==200/count);
    assert(owner.counts[GE_MP_EFFECT_SHARD]==200/count);
    assert(owner.counts[GE_MP_EFFECT_SCORCH]==(count==1?20:0));
    export_ok();
}
static void native_mutations(unsigned count)
{
    begin(count);
    coord3d pos={1,10,3};
    flareimage2=explosion_smokeimages=scattered_explosions=fixture_images;
    randomSetSeed(17);
    glassCreateShard(&pos,0.2f,3);
    s_bullet_spark *spark=bullet_spark_create(&pos,1,2,1);assert(spark);
    explosionInitFlyingParticles(&pos,1,2,3,1);
    g_MovingBulletSparkArray[0].unk00=*spark;
    g_MovingBulletSparkArray[0].velocity=(coord3d){1,2,3};
    g_MovingBulletSparkArray[0].unk38=-100;
    CasingRecord *casing=&g_Casings[0];
    casing->header=(void *)&model_identity;casing->floor_y_pos=-100;
    casing->pos=pos;casing->vel=(coord3d){1,2,3};
    for(unsigned i=0;i<4;++i)casing->rot_mtx.m[i][i]=casing->rot_velocity_mtx.m[i][i]=1;
    uint64_t before[GE_MP_EFFECTS_COMPONENTS];
    for(unsigned c=0;c<GE_MP_EFFECTS_COMPONENTS;++c)before[c]=fingerprint(c);
    assert(ge_original_effect_runtime_pre_world());
    const unsigned changed[]={GE_MP_EFFECT_PARTICLE,GE_MP_EFFECT_SHARD,GE_MP_EFFECT_SPARK,
        GE_MP_EFFECT_MOVING_SPARK,GE_MP_EFFECT_CASING};
    for(unsigned i=0;i<sizeof(changed)/sizeof(*changed);++i)assert(fingerprint(changed[i])!=before[changed[i]]);
    assert(g_FlyingParticlesBuffer[0].unk00==2 && ptr_shattered_window_pieces[0].active==2);
    MUTATE(GE_MP_EFFECT_GLOBAL,g_NumExplosionEntries);MUTATE(GE_MP_EFFECT_GLOBAL,g_NumSmokeEntries);
    MUTATE(GE_MP_EFFECT_GLOBAL,g_BulletSparkColors[3].b);MUTATE(GE_MP_EFFECT_GLOBAL,g_SpExplosionDamageMult);
    MUTATE(GE_MP_EFFECT_PARTICLE,g_FlyingParticlesBuffer[0].vertex_list[2].v.cn[3]);
    MUTATE(GE_MP_EFFECT_PARTICLE,g_FlyingParticlesBuffer[0].rotation_drift.z);
    MUTATE(GE_MP_EFFECT_SHARD,ptr_shattered_window_pieces[0].v2s);
    MUTATE(GE_MP_EFFECT_SHARD,ptr_shattered_window_pieces[0].angvel.y);
    MUTATE(GE_MP_EFFECT_SPARK,spark->unk06);MUTATE(GE_MP_EFFECT_SPARK,spark->unk2B);
    MUTATE(GE_MP_EFFECT_MOVING_SPARK,g_MovingBulletSparkArray[0].unk38);
    MUTATE(GE_MP_EFFECT_CASING,casing->rot_velocity_mtx.m[2][1]);
    struct Explosion *explosion=&g_ExplosionBuffer[0];
    explosion->prop=&fixture_props[0];explosion->source=&fixture_props[1];
    explosion->age=1;explosion->unk3CA=2;explosion->explosion_type=1;explosion->unk3CD=1;
    explosion->player=0;explosion->room=1;explosion->pos=pos;
    explosion->parts[3]=(struct ExplosionPart){.pos=pos,.size=3,.rot=4,.frame=5};
    MUTATE(GE_MP_EFFECT_EXPLOSION,explosion->parts[3].rot);
    MUTATE(GE_MP_EFFECT_EXPLOSION,explosion->pos.y);MUTATE(GE_MP_EFFECT_EXPLOSION,explosion->player);
    explosion->prop=NULL; /* Constructor only replaces part0; free part3 still matters. */
    MUTATE(GE_MP_EFFECT_EXPLOSION,explosion->parts[3].pos.z);
    struct Smoke *smoke=&g_SmokeBuffer[0];smoke->prop=&fixture_props[0];smoke->smoke_type=1;
    smoke->duration=2;smoke->parts[2]=(struct SmokePart){.pos=pos,.size=-2,.rot=1,.deltarot=2,
        .offset1=3,.offset2=4,.alpha=5,.count=6};
    MUTATE(GE_MP_EFFECT_SMOKE,smoke->parts[2].alpha); /* original tick uses size != 0 */
    smoke->prop=NULL;MUTATE(GE_MP_EFFECT_SMOKE,smoke->parts[2].offset2);
    struct BulletImpact *impact=&g_BulletImpactBuffer[0];impact->room=2;impact->impact_type=1;
    impact->prop=&fixture_props[1];impact->model_render_pos_index=2;impact->room_clear_flag=0;
    MUTATE(GE_MP_EFFECT_IMPACT,impact->room_clear_flag);MUTATE(GE_MP_EFFECT_IMPACT,impact->vertex_list[1].v.tc[0]);
    if(count==1){g_ScorchBuffer[0].roomid=2;MUTATE(GE_MP_EFFECT_SCORCH,g_ScorchBuffer[0].explosion_size);}
    Projectile *p=&g_Projectiles[0];
    p->unk64=17;p->sounds[1]=(void *)&audio_identity;
    fixture_projectile_reset(p); /* Actual original reset leaves these retained fields. */
    assert(p->unk64==17 && p->sounds[1]==(void *)&audio_identity);
    p->ownerprop=&fixture_props[0];p->obj=&fixture_object;
#define PROJECTILE(member) MUTATE(GE_MP_EFFECT_PROJECTILE,p->member)
    PROJECTILE(flags);PROJECTILE(speed.z);PROJECTILE(unk10.x);PROJECTILE(unk1C);
    PROJECTILE(mtx.m[3][2]);PROJECTILE(unk60);PROJECTILE(unk64);PROJECTILE(unk68[3]);PROJECTILE(unk78[2]);
    PROJECTILE(unk8C);PROJECTILE(unk90);PROJECTILE(unk94);PROJECTILE(lastSfxTimer);PROJECTILE(soundSlot);
    PROJECTILE(unkA8);PROJECTILE(unkAC);PROJECTILE(unkB0);PROJECTILE(unkB4);PROJECTILE(droptype);
    PROJECTILE(refreshrate);PROJECTILE(unkC0);PROJECTILE(unkC4);PROJECTILE(unkC8);PROJECTILE(unkCC[7]);
    PROJECTILE(unkd4);PROJECTILE(unkD8);PROJECTILE(unkDC);PROJECTILE(age);PROJECTILE(unkE8);
    p->flags=PROJECTILEFLAG_FREE;PROJECTILE(unk64);
#undef PROJECTILE
    if(count==1) {
        /* Largest simultaneous physical population checks all typed arenas. */
        explosion->prop=&fixture_props[0];
        for(unsigned i=0;i<40;++i)explosion->parts[i]=explosion->parts[3];
        for(unsigned i=1;i<6;++i)g_ExplosionBuffer[i]=*explosion;
        smoke->prop=&fixture_props[0];
        for(unsigned i=0;i<10;++i)smoke->parts[i]=smoke->parts[2];
        for(unsigned i=1;i<20;++i)g_SmokeBuffer[i]=*smoke;
        for(unsigned i=1;i<200;++i) {
            g_FlyingParticlesBuffer[i]=g_FlyingParticlesBuffer[0];
            ptr_shattered_window_pieces[i]=ptr_shattered_window_pieces[0];
        }
        for(unsigned i=1;i<100;++i)g_BulletImpactBuffer[i]=*impact;
        for(unsigned i=1;i<50;++i)g_MovingBulletSparkArray[i]=g_MovingBulletSparkArray[0];
        for(unsigned i=1;i<20;++i) {
            g_BulletSparkArray[i]=*spark;g_ScorchBuffer[i]=g_ScorchBuffer[0];
            g_Projectiles[i]=*p;g_Casings[i]=*casing;
        }
        export_ok();assert(output.record_count==GE_MP_EFFECTS_RECORDS);
    }
    uint64_t id=0;assert(ge_mp_state_effects_actor(&owner,GE_MP_EFFECT_EXPLOSION,explosion,&id)==GE_MP_STATE_OK);
    assert(id==owner.ids[owner.offsets[GE_MP_EFFECT_EXPLOSION]]);
    assert(ge_mp_state_effects_actor(&owner,GE_MP_EFFECT_EXPLOSION,(char *)explosion+1,&id)==GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_OK);
    assert(ge_mp_registry_resolve(&registry,id,NULL)==GE_MP_STATE_STALE);
    ge_original_effect_runtime_end();assert(!ge_original_effect_runtime_pre_world());
}
static void fallback_target_aliases(void)
{
    begin(2);
    Projectile *p=&g_Projectiles[0];
    ObjectRecord object={0};object.projectile=p;object.runtime_bitflags=0x80;
    coord3d target={1.25f,-2.5f,3.75f};
    p->flags=0;
    fixture_gun_projectile_fallback(&object,&target,0);
    assert(p->unkd4==0 && !p->unkD8 && !p->unkDC);
    fixture_gun_projectile_fallback(&object,&target,1);
    assert(p->flags&PROJECTILEFLAG_00000100);
    export_ok();
    unsigned record=owner.offsets[GE_MP_EFFECT_PROJECTILE];
    assert(output.fields[record][9].count==43 && output.fields[record][6].count==7);
    const float *values=output.fields[record][9].values;
    assert(values[40]==target.x && values[41]==target.y && values[42]==target.z);
    for(unsigned axis=0;axis<3;++axis) {
        target=(coord3d){1,2,3};target.f[axis]=-0.0f;
        fixture_gun_projectile_fallback(&object,&target,1);export_ok();
        values=output.fields[record][9].values;
        uint32_t bits;memcpy(&bits,&values[40+axis],sizeof(bits));
        assert(bits==UINT32_C(0x80000000));
        const float invalid[]={NAN,INFINITY,-INFINITY};
        for(unsigned n=0;n<3;++n) {
            target=(coord3d){1,2,3};target.f[axis]=invalid[n];
            fixture_gun_projectile_fallback(&object,&target,1);
            assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
            assert(!output.record_count && output.coverage.unclassified_fields);
            /* Free slot retention must apply identical float validation. */
            p->flags|=PROJECTILEFLAG_FREE;
            assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
            p->flags&=~PROJECTILEFLAG_FREE;
        }
    }
    target=(coord3d){1,2,3};fixture_gun_projectile_fallback(&object,&target,1);export_ok();
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_OK);
    ge_original_effect_runtime_end();
}
static void failures_and_lifetimes(void)
{
    begin(2);
    uint64_t first=owner.ids[0];
    /* Unknown actual prop/reference is reported; arbitrary pointers never enter payloads. */
    g_ExplosionBuffer[0].prop=(void *)(uintptr_t)1;g_ExplosionBuffer[0].source=NULL;
    g_ExplosionBuffer[0].unk3CD=0;
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(!output.record_count && output.unresolved_kinds==(1U<<GE_MP_EFFECT_REF_PROP));
    g_ExplosionBuffer[0].prop=NULL;
    g_BulletSparkArray[0].lifetime=1;g_BulletSparkArray[0].unk0C=NULL;
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.unresolved_kinds==(1U<<GE_MP_EFFECT_REF_IMAGE_TABLE));
    g_BulletSparkArray[0].lifetime=0;
    /* Unused inactive payload is gated; free projectile retained state is not. */
    g_FlyingParticlesBuffer[0].position.x=NAN;export_ok();
    g_Projectiles[0].unk64=NAN;
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    assert(output.coverage.unclassified_fields && !output.record_count);
    g_Projectiles[0].unk64=0;
    g_NumParticleEntries=max_particles;
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INCOMPLETE);
    g_NumParticleEntries=0;
    const GeMpRecordSchema global={0,GE_MP_SYSTEM_SESSION,NULL,0};
    GeMpRecordSchema schemas[]={global,*ge_mp_state_effects_schema()};
    GeMpStateSchema schema={1,GE_MP_SYSTEM_SESSION|GE_MP_SYSTEM_EXPLOSIONS|GE_MP_SYSTEM_PROJECTILES,0,schemas,2};
    GeMpCanonicalSnapshot snapshot={.session=1,.tick=1,.schema=&schema,.registry=&registry,
        .records=output.records,.record_count=output.record_count,.enabled_systems=schema.registered_systems};
    uint8_t digest[32];memset(digest,0xA5,sizeof(digest));
    assert(ge_mp_state_sha256(&snapshot,digest,NULL)==GE_MP_STATE_INCOMPLETE);
    for(unsigned i=0;i<32;++i)assert(digest[i]==0xA5);
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_OK);
    begin(4);assert(owner.ids[0]==first+1);
    /* A reset without matching end is detected despite unchanged static addresses. */
    assert(ge_original_effect_runtime_reset(4,LEVELID_LIBRARY));
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_STALE);
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_state_effects_begin(&owner,&registry)==GE_MP_STATE_OK);
    ge_original_covert_modem_projectile_reset();
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_mp_state_effects_begin(&owner,&registry)==GE_MP_STATE_OK);
    assert(ge_original_effect_buffers_reset_players(4,LEVELID_LIBRARY));
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE);
    assert(ge_original_effect_runtime_reset(4,LEVELID_LIBRARY));
    assert(ge_mp_state_effects_begin(&owner,&registry)==GE_MP_STATE_OK);
    ge_original_effect_runtime_end();assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE);
    GeMpRegistry tiny;GeMpRegistryEntry tiny_entries[1];
    assert(ge_mp_registry_init(&tiny,tiny_entries,1)==GE_MP_STATE_OK);
    assert(ge_original_effect_runtime_reset(2,LEVELID_LIBRARY));
    assert(ge_mp_state_effects_begin(&owner,&tiny)==GE_MP_STATE_CAPACITY);
    assert(tiny.count==0 && !owner.live);ge_original_effect_runtime_end();
}
static void malformed_cleanup(void)
{
    static GeMpStateEffectsOwner saved_owner;
    static GeMpRegistryEntry saved_entries[800];
    begin(2);saved_owner=owner;size_t n=registry.count;
    memcpy(saved_entries,entries,n*sizeof(*entries));
#define UNCHANGED() do { \
    GeMpStateEffectsOwner before;memcpy(&before,&owner,sizeof(before)); \
    GeMpRegistry before_registry;memcpy(&before_registry,&registry,sizeof(before_registry)); \
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_INVALID); \
    assert(!memcmp(&before,&owner,sizeof(owner)) && registry.count==n); \
    assert(!memcmp(&before_registry,&registry,sizeof(registry))); \
    assert(!memcmp(saved_entries,entries,n*sizeof(*entries))); \
    owner=saved_owner; \
} while(0)
    /* Last ID: all preceding valid E8 identities must remain live. */
    unsigned last=owner.count-1;
    owner.ids[last]=resource_ids[0];UNCHANGED();
    owner.ids[last]=owner.ids[owner.offsets[GE_MP_EFFECT_PROJECTILE]];UNCHANGED();
    owner.ids[last]=owner.ids[last-1];UNCHANGED();
    owner.ids[last]=(uint64_t)ge_mp_registry_key(GE_MP_EFFECTS_KIND,255,19)<<32|1;UNCHANGED();
    owner.ids[last]&=UINT64_C(0xffffffff00000000);UNCHANGED();
    owner.count=GE_MP_EFFECTS_RECORDS+1;UNCHANGED();
    --owner.count;UNCHANGED();
    ++owner.count;UNCHANGED();
    ++owner.counts[GE_MP_EFFECT_CASING];UNCHANGED();
    --owner.counts[GE_MP_EFFECT_PARTICLE];UNCHANGED();
    ++owner.offsets[GE_MP_EFFECT_CASING];UNCHANGED();
    owner.ids[owner.count]=resource_ids[0];UNCHANGED();
    owner.storage.particle_count=201;UNCHANGED();
    owner.storage.player_count=0;UNCHANGED();
    owner.storage.particles=(const void *)(uintptr_t)1;
    assert(ge_mp_state_effects_export(&owner,&providers,&output)==GE_MP_STATE_INVALID);
    assert(!output.record_count);UNCHANGED();
    size_t capacity=registry.capacity;registry.capacity=n-1;
    UNCHANGED();registry.capacity=capacity;
    /* Native expiration does not authorize malformed identities either. */
    ge_original_effect_runtime_end();owner.ids[last]=resource_ids[0];UNCHANGED();
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE && !owner.live);
    for(unsigned i=0;i<saved_owner.count;++i)
        assert(ge_mp_registry_resolve(&registry,saved_owner.ids[i],NULL)==GE_MP_STATE_STALE);
    assert(ge_mp_registry_resolve(&registry,resource_ids[0],NULL)==GE_MP_STATE_OK);
    begin(4);uint64_t old=owner.ids[owner.count-1],successor;
    assert(ge_mp_registry_release(&registry,old)==GE_MP_STATE_OK);
    assert(ge_mp_registry_acquire(&registry,(uint32_t)(old>>32),&successor)==GE_MP_STATE_OK);
    assert(successor!=old);
    assert(ge_mp_state_effects_end(&owner)==GE_MP_STATE_STALE && !owner.live);
    assert(ge_mp_registry_resolve(&registry,successor,NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_resolve(&registry,resource_ids[0],NULL)==GE_MP_STATE_OK);
    assert(ge_mp_registry_release(&registry,successor)==GE_MP_STATE_OK);
    ge_original_effect_runtime_end();
#undef UNCHANGED
}
int main(void)
{
    native_runtime_regression();registry_init();
    native_mutations(2);native_mutations(4);native_mutations(1);
    fallback_target_aliases();failures_and_lifetimes();malformed_cleanup();
    puts("E8 effects: native 2/4 construction/update, typed mutations, negative smoke and free-part retention, ABI fields, stage lifetime and incomplete schema rejection passed");
    return 0;
}
