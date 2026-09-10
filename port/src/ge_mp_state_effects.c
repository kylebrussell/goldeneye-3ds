#include "ge_mp_state_effects.h"
#include "ge_original_covert_modem_projectile.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/glass.h"
#include "game/explosion.h"
#include "game/gun.h"
#include <math.h>
#include <string.h>
extern struct rgba_u8 g_BulletSparkColors[8];
extern ALSoundState *g_CasingSfxState;
extern s32 D_800483C0;
_Static_assert(EXPLOSION_BUFFER_LEN==6 && SMOKE_BUFFER_LEN==20,"effect pool ABI");
_Static_assert(EXPLOSION_PARTS_LEN==40 && SMOKE_PARTS_LEN==10,"effect parts ABI");
_Static_assert(sizeof(float)==4 && sizeof(Vtx)==16,"effect scalar/vertex ABI");
_Static_assert(offsetof(Projectile,unkD8)==offsetof(Projectile,unkd4)+sizeof(float)
 && offsetof(Projectile,unkDC)==offsetof(Projectile,unkd4)+2*sizeof(float),
 "original projectile fallback coord3d alias must remain contiguous");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(sizeof(struct Explosion)==0x3e0 && sizeof(struct Smoke)==0x198,"N64 explosion/smoke ABI");
_Static_assert(sizeof(struct BulletImpact)==0x50 && sizeof(struct Scorch)==0x58,"N64 impact/scorch ABI");
_Static_assert(sizeof(struct FlyingParticles)==0x78 && sizeof(s_shattered_window_piece)==0x68,"N64 fragment ABI");
_Static_assert(sizeof(s_bullet_spark)==0x2c && sizeof(s_moving_bullet_spark)==0x3c,"N64 spark ABI");
_Static_assert(sizeof(Projectile)==0xec && offsetof(Projectile,unkd4)==0xd4,"N64 projectile ABI");
_Static_assert(sizeof(CasingRecord)==0xa0,"N64 US casing ABI");
#endif
static uint32_t key(unsigned component,unsigned slot)
{return ge_mp_registry_key(GE_MP_EFFECTS_KIND,(uint8_t)component,(uint16_t)slot);}
static GeMpStateStatus valid(const GeMpStateEffectsOwner *o)
{
 GeOriginalEffectRuntimeStorage s;const Projectile *p;uint32_t n;uint64_t life;
 if(!o || !o->live)return GE_MP_STATE_STALE;
 if(!ge_original_effect_runtime_storage(&s) || s.lifetime!=o->storage.lifetime
  || !ge_original_projectile_pool_storage(&p,&n,&life) || p!=o->projectiles
  || n!=20 || life!=o->projectile_lifetime)return GE_MP_STATE_STALE;
 return GE_MP_STATE_OK;
}
static const GeMpRegistryEntry *entry(const GeMpRegistry *r,uint32_t k)
{for(size_t i=0;i<r->count;++i)if(r->entries[i].key==k)return &r->entries[i];return NULL;}
GeMpStateStatus ge_mp_state_effects_begin(GeMpStateEffectsOwner *o,GeMpRegistry *r)
{
 GeMpStateEffectsOwner next={0};uint32_t n;size_t missing=0;
 if(!o || !r || !r->entries || r->count>r->capacity || r->capacity>GE_MP_STATE_MAX_RECORDS)return GE_MP_STATE_INVALID;
 if(o->live)return GE_MP_STATE_STALE;
 for(size_t i=0;i<r->count;++i)if(!(r->entries[i].key>>24)
  || r->entries[i].live>1 || (r->entries[i].live&&!r->entries[i].generation)
  || (i&&r->entries[i-1].key>=r->entries[i].key))return GE_MP_STATE_INVALID;
 if(!ge_original_effect_runtime_storage(&next.storage)
  || !ge_original_projectile_pool_storage(&next.projectiles,&n,&next.projectile_lifetime)
  || n!=20 || next.storage.particle_count>200 || next.storage.shard_count>200
  || next.storage.scorch_count>20)return GE_MP_STATE_INCOMPLETE;
 const uint16_t counts[]={1,6,20,(uint16_t)next.storage.particle_count,100,
  (uint16_t)next.storage.scorch_count,(uint16_t)next.storage.shard_count,20,50,20,20};
 memcpy(next.counts,counts,sizeof(counts));
 for(unsigned c=0;c<GE_MP_EFFECTS_COMPONENTS;++c) {
  next.offsets[c]=next.count;next.count+=counts[c];
  for(unsigned s=0;s<counts[c];++s) {
   const GeMpRegistryEntry *e=entry(r,key(c,s));
   if(!e)++missing;else if(e->live)return GE_MP_STATE_STALE;
   else if(e->generation==UINT32_MAX)return GE_MP_STATE_EXHAUSTED;
  }
 }
 if(missing>r->capacity-r->count)return GE_MP_STATE_CAPACITY;
 for(unsigned c=0;c<GE_MP_EFFECTS_COMPONENTS;++c)for(unsigned s=0;s<counts[c];++s) {
  GeMpStateStatus st;if(!entry(r,key(c,s))) {
   st=ge_mp_registry_define(r,key(c,s));if(st!=GE_MP_STATE_OK)return st;
  }
  st=ge_mp_registry_acquire(r,key(c,s),&next.ids[next.offsets[c]+s]);
  if(st!=GE_MP_STATE_OK)return st;
 }
 next.registry=r;next.live=1;*o=next;return GE_MP_STATE_OK;
}
GeMpStateStatus ge_mp_state_effects_end(GeMpStateEffectsOwner *o)
{
 if(!o || !o->live)return GE_MP_STATE_STALE;
 GeMpStateStatus result=valid(o);
 for(unsigned i=0;i<o->count;++i) {
  GeMpStateStatus s=ge_mp_registry_release(o->registry,o->ids[i]);
  if(s!=GE_MP_STATE_OK)result=s;
 }
 memset(o,0,sizeof(*o));return result;
}
static const void *base(const GeMpStateEffectsOwner *o,unsigned c,size_t *width)
{
#define B(k,p) case k:*width=sizeof(*(p));return (p)
 switch(c){
 B(GE_MP_EFFECT_EXPLOSION,o->storage.explosions);B(GE_MP_EFFECT_SMOKE,o->storage.smoke);
 B(GE_MP_EFFECT_PARTICLE,o->storage.particles);B(GE_MP_EFFECT_IMPACT,o->storage.impacts);
 B(GE_MP_EFFECT_SCORCH,o->storage.scorches);B(GE_MP_EFFECT_SHARD,o->storage.shards);
 B(GE_MP_EFFECT_SPARK,o->storage.sparks);B(GE_MP_EFFECT_MOVING_SPARK,o->storage.moving_sparks);
 B(GE_MP_EFFECT_PROJECTILE,o->projectiles);B(GE_MP_EFFECT_CASING,o->storage.casings);
 default:return NULL;}
#undef B
}
GeMpStateStatus ge_mp_state_effects_actor(const GeMpStateEffectsOwner *o,
 unsigned c,const void *p,uint64_t *id)
{
 size_t width;uintptr_t start,address=(uintptr_t)p;GeMpStateStatus s=valid(o);
 if(!id)return GE_MP_STATE_INVALID;if(s!=GE_MP_STATE_OK)return s;
 if(c<1 || c>=GE_MP_EFFECTS_COMPONENTS)return GE_MP_STATE_INVALID;
 start=(uintptr_t)base(o,c,&width);
 if(!p || !start || address<start || address-start>=o->counts[c]*width
  || (address-start)%width)return GE_MP_STATE_INCOMPLETE;
 uint64_t actor=o->ids[o->offsets[c]+(address-start)/width];
 s=ge_mp_registry_resolve(o->registry,actor,NULL);if(s==GE_MP_STATE_OK)*id=actor;return s;
}
#define F(id,t,min,max) {id,GE_MP_STATE_##t,0,min,max}
static const GeMpFieldSchema fields[]={F(256,U8,1,1),F(257,U16,1,1),F(258,BOOL,1,1),
 F(259,I32,0,32),F(260,I16,0,128),F(261,I8,0,16),F(262,U32,0,32),
 F(263,U16,0,32),F(264,U8,0,128),F(265,F32,0,224),
 {266,GE_MP_STATE_ACTOR_REF,GE_MP_FIELD_NULLABLE,0,8}};
#undef F
static const GeMpRecordSchema schema={GE_MP_EFFECTS_KIND,
 GE_MP_SYSTEM_EXPLOSIONS|GE_MP_SYSTEM_PROJECTILES,fields,GE_MP_EFFECTS_FIELDS};
const GeMpRecordSchema *ge_mp_state_effects_schema(void){return &schema;}
typedef struct Writer {GeMpStateEffectsSnapshot *out;const GeMpStateEffectsProviders *providers;
 const GeMpRegistry *registry;int failed;} Writer;
#define PUT(name,idx,type) static void put_##name(Writer *w,type value) { \
 if(w->out->used[idx]>=sizeof(w->out->name)/sizeof(*w->out->name)){w->failed=1;return;} \
 w->out->name[w->out->used[idx]++]=value; }
PUT(i32,0,int32_t) PUT(i16,1,int16_t) PUT(i8,2,int8_t) PUT(u32,3,uint32_t)
PUT(u16,4,uint16_t) PUT(u8,5,uint8_t) PUT(refs,7,uint64_t)
#undef PUT
static void put_f32(Writer *w,float value)
{if(!isfinite(value) || w->out->used[6]>=16384){w->failed=1;return;}w->out->f32[w->out->used[6]++]=value;}
static void ref(Writer *w,GeMpEffectReferenceKind kind,const void *p)
{
 uint64_t id=0;
 if((kind==GE_MP_EFFECT_REF_IMAGE_TABLE && !p) || (p && (!w->providers || !w->providers->reference
  || w->providers->reference(w->providers->context,kind,p,&id)!=GE_MP_STATE_OK || !id
  || ge_mp_registry_resolve(w->registry,id,NULL)!=GE_MP_STATE_OK))) {
  ++w->out->coverage.unresolved_references;w->out->unresolved_kinds|=1U<<kind;
 }
 put_refs(w,id);
}
static void coord(Writer *w,const coord3d *p){for(unsigned i=0;i<3;++i)put_f32(w,p->f[i]);}
static void matrix(Writer *w,const Mtxf *m){for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)put_f32(w,m->m[i][j]);}
static void vertices(Writer *w,const Vtx *v,unsigned count)
{for(unsigned i=0;i<count;++i){for(unsigned j=0;j<3;++j)put_i16(w,v[i].v.ob[j]);
 for(unsigned j=0;j<2;++j)put_i16(w,v[i].v.tc[j]);put_u16(w,v[i].v.flag);
 for(unsigned j=0;j<4;++j)put_u8(w,v[i].v.cn[j]);}}
static void spark(Writer *w,const s_bullet_spark *p)
{
 put_i16(w,p->lifetime);if(p->lifetime<=0)return;
 put_i32(w,p->age);put_i16(w,p->unk06);put_f32(w,p->unk08);
 put_f32(w,p->unk10);put_f32(w,p->unk14);put_f32(w,p->unk18);
 put_f32(w,p->unk1c);put_f32(w,p->unk20);put_f32(w,p->unk24);
 put_u8(w,p->unk28);put_u8(w,p->unk29);put_u8(w,p->unk2A);put_u8(w,p->unk2B);
 ref(w,GE_MP_EFFECT_REF_IMAGE_TABLE,p->unk0C);
}

static void copy(Writer *w,const GeMpStateEffectsOwner *o,unsigned c,unsigned s,unsigned r)
{
 uint8_t *active=&w->out->active[r];
#define I(v) put_i32(w,(int32_t)(v))
#define H(v) put_i16(w,(int16_t)(v))
#define B(v) put_i8(w,(int8_t)(v))
#define U(v) put_u32(w,(uint32_t)(v))
#define F(v) put_f32(w,(v))
#define R(k,p) ref(w,GE_MP_EFFECT_REF_##k,p)
 switch(c) {
 case GE_MP_EFFECT_GLOBAL:
  *active=1;I(o->storage.stage);I(g_NumExplosionEntries);I(g_NumSmokeEntries);
  I(g_NumParticleEntries);I(g_NumImpactEntries);I(g_NumScorchEntries);I(g_NextShardNum);I(D_800483C0);
  U(o->storage.player_count);for(unsigned i=0;i<GE_MP_EFFECTS_COMPONENTS;++i)U(o->counts[i]);
  F(g_SpExplosionDamageMult);
  for(unsigned i=0;i<8;++i){put_u8(w,g_BulletSparkColors[i].r);put_u8(w,g_BulletSparkColors[i].g);
   put_u8(w,g_BulletSparkColors[i].b);put_u8(w,g_BulletSparkColors[i].a);}
  R(AUDIO,g_CasingSfxState);
  if(g_NumParticleEntries<0 || g_NumParticleEntries>=max_particles || g_NumImpactEntries<0
   || g_NumImpactEntries>=100 || g_NumScorchEntries<0 || g_NumScorchEntries>=20
   || g_NextShardNum<0 || g_NextShardNum>=SHATTERED_WINDOW_PIECES_BUFFER_LEN)w->failed=1;
  break;
 case GE_MP_EFFECT_EXPLOSION: {
  const struct Explosion *p=&o->storage.explosions[s];*active=p->prop!=NULL;
  if(*active){H(p->age);H(p->unk3CA);B(p->explosion_type);B(p->unk3CD);B(p->player);
   R(PROP,p->prop);R(PROP,p->source);if(p->unk3CD){coord(w,&p->pos);H(p->room);}}
  /* Free effect slots retain parts until overwritten by future constructors. */
  for(unsigned i=0;i<40;++i)H(p->parts[i].frame);
  for(unsigned i=0;i<40;++i)if(p->parts[i].frame>0) {
   coord(w,&p->parts[i].pos);F(p->parts[i].size);F(p->parts[i].rot);
  }
  break;}
 case GE_MP_EFFECT_SMOKE: {
  const struct Smoke *p=&o->storage.smoke[s];*active=p->prop!=NULL;
  if(*active){H(p->duration);H(p->smoke_type);R(PROP,p->prop);}
  for(unsigned i=0;i<10;++i)F(p->parts[i].size);
  for(unsigned i=0;i<10;++i)if(p->parts[i].size!=0) {
   const struct SmokePart *q=&p->parts[i];coord(w,&q->pos);F(q->rot);F(q->deltarot);
   F(q->offset1);F(q->offset2);F(q->alpha);H(q->count);
  }
  break;}
 case GE_MP_EFFECT_PARTICLE: {
  const struct FlyingParticles *p=&o->storage.particles[s];*active=p->unk00>0;I(p->unk00);
  if(*active){coord(w,&p->position);coord(w,&p->rotation);coord(w,&p->position_drift);
   coord(w,&p->rotation_drift);vertices(w,p->vertex_list,4);}break;}
 case GE_MP_EFFECT_IMPACT: {
  const struct BulletImpact *p=&o->storage.impacts[s];*active=p->room>=0;H(p->room);
  if(*active){H(p->impact_type);B(p->model_render_pos_index);B(p->room_clear_flag);
   R(PROP,p->prop);vertices(w,p->vertex_list,4);}break;}
 case GE_MP_EFFECT_SCORCH: {
  const struct Scorch *p=&o->storage.scorches[s];*active=p->roomid>=0;H(p->roomid);
  if(*active){coord(w,&p->pos);F(p->explosion_size);vertices(w,p->vertex_list,4);}break;}
 case GE_MP_EFFECT_SHARD: {
  const s_shattered_window_piece *p=&o->storage.shards[s];*active=p->active>0;I(p->active);
  if(*active){coord(w,&p->pos);coord(w,&p->rot);coord(w,&p->velocity);coord(w,&p->angvel);
   /* Named native fields, without aliasing the struct as packed Vtx bytes. */
#define SHARD_VERTEX(n) H(p->v##n##x);H(p->v##n##y);H(p->v##n##z); \
 H(p->v##n##s);H(p->v##n##t);H(p->v##n##flag);put_u8(w,p->v##n##r); \
 put_u8(w,p->v##n##g);put_u8(w,p->v##n##b);put_u8(w,p->v##n##a)
   SHARD_VERTEX(1);SHARD_VERTEX(2);SHARD_VERTEX(3);
#undef SHARD_VERTEX
  }break;}
 case GE_MP_EFFECT_SPARK: {
  const s_bullet_spark *p=&o->storage.sparks[s];*active=p->lifetime>0;spark(w,p);break;}
 case GE_MP_EFFECT_MOVING_SPARK: {
  const s_moving_bullet_spark *p=&o->storage.moving_sparks[s];*active=p->unk00.lifetime>0;
  spark(w,&p->unk00);if(*active){coord(w,&p->velocity);F(p->unk38);}break;}
 case GE_MP_EFFECT_PROJECTILE: {
  const Projectile *p=&o->projectiles[s];*active=!(p->flags&PROJECTILEFLAG_FREE);
  /* Reset leaves several scalar/matrix fields untouched. Retain all named
   * scalars even while free; do not infer unused tail defaults. */
  U(p->flags);coord(w,&p->speed);coord(w,&p->unk10);F(p->unk1C);matrix(w,&p->mtx);
  F(p->unk60);F(p->unk64);for(unsigned i=0;i<4;++i)F(p->unk68[i]);
  for(unsigned i=0;i<4;++i)F(p->unk78[i]);F(p->unk8C);U(p->unk90);F(p->unk94);
  U(p->lastSfxTimer);U(p->soundSlot);U(p->unkA8);U(p->unkAC);F(p->unkB0);F(p->unkB4);
  I(p->droptype);I(p->refreshrate);F(p->unkC0);F(p->unkC4);F(p->unkC8);
  for(unsigned i=0;i<8;++i)put_u8(w,p->unkCC[i]);
  /* gunInitProjectileFromPlayer writes this tail through coord3d, and
   * collision consumes the same three floats. Decode the declared U32 Y/Z
   * members by bits, without aliasing reads or integer conversion. */
  float target_y,target_z;
  memcpy(&target_y,&p->unkD8,sizeof(target_y));
  memcpy(&target_z,&p->unkDC,sizeof(target_z));
  F(p->unkd4);F(target_y);F(target_z);
  U(p->age);I(p->unkE8);
  R(PROP,*active?p->ownerprop:NULL);R(OBJECT,*active?p->obj:NULL);
  R(AUDIO,p->sounds[0]);R(AUDIO,p->sounds[1]);break;}
 case GE_MP_EFFECT_CASING: {
  const CasingRecord *p=&o->storage.casings[s];*active=p->header!=NULL;
  if(*active){F(p->floor_y_pos);coord(w,&p->pos);coord(w,&p->vel);
   matrix(w,&p->rot_mtx);matrix(w,&p->rot_velocity_mtx);R(MODEL_HEADER,p->header);}break;}
 }
#undef I
#undef H
#undef B
#undef U
#undef F
#undef R
}
GeMpStateStatus ge_mp_state_effects_export(const GeMpStateEffectsOwner *o,
 const GeMpStateEffectsProviders *providers,GeMpStateEffectsSnapshot *out)
{
 GeMpStateStatus status=valid(o);
 if(!out)return GE_MP_STATE_INVALID;
 memset(out,0,sizeof(*out));
 if(status!=GE_MP_STATE_OK){out->coverage.unclassified_fields=1;return status;}
 Writer w={out,providers,o->registry,0};
 for(unsigned c=0;c<GE_MP_EFFECTS_COMPONENTS;++c)for(unsigned s=0;s<o->counts[c];++s) {
  unsigned r=o->offsets[c]+s;size_t before[8];memcpy(before,out->used,sizeof(before));
  out->component[r]=(uint8_t)c;out->slot[r]=(uint16_t)s;
  copy(&w,o,c,s,r);
  out->fields[r][0]=(GeMpStateField){256,GE_MP_STATE_U8,1,&out->component[r]};
  out->fields[r][1]=(GeMpStateField){257,GE_MP_STATE_U16,1,&out->slot[r]};
  out->fields[r][2]=(GeMpStateField){258,GE_MP_STATE_BOOL,1,&out->active[r]};
#define FIELD(index,name) out->fields[r][index+3]=(GeMpStateField){(uint16_t)(259+index), \
 fields[index+3].type,(uint32_t)(out->used[index]-before[index]),out->name+before[index]}
  FIELD(0,i32);FIELD(1,i16);FIELD(2,i8);FIELD(3,u32);FIELD(4,u16);FIELD(5,u8);FIELD(6,f32);FIELD(7,refs);
#undef FIELD
  out->records[r]=(GeMpStateRecord){o->ids[r],GE_MP_EFFECTS_KIND,out->fields[r],GE_MP_EFFECTS_FIELDS};
  for(unsigned f=0;f<GE_MP_EFFECTS_FIELDS;++f)
   if(out->fields[r][f].count>fields[f].max_count)w.failed=1;
  if(ge_mp_registry_resolve(o->registry,o->ids[r],NULL)!=GE_MP_STATE_OK)w.failed=1;
 }
 if(w.failed)++out->coverage.unclassified_fields;
 if(out->coverage.unclassified_fields || out->coverage.unresolved_references)return GE_MP_STATE_INCOMPLETE;
 out->record_count=o->count;
 out->coverage.enumerated_systems=GE_MP_SYSTEM_EXPLOSIONS|GE_MP_SYSTEM_PROJECTILES;
 return GE_MP_STATE_OK;
}
