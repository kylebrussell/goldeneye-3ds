#include "ge_original_effect_runtime.h"
#include "ge_original_effect_buffers.h"
#include "ge_original_bg_visibility.h"
#include <ultra64.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/glass.h"
#include "game/explosion.h"
#include "game/gun.h"

static _Alignas(16) s_shattered_window_piece shards[200];
extern s_bullet_spark g_BulletSparkArray[20];
extern s_moving_bullet_spark g_MovingBulletSparkArray[50];
static GeOriginalEffectRuntimeStorage owned;
static uint64_t lifetime,buffer_lifetime;
void ge_original_effect_reset_static_exact(void);
void ge_original_effect_reset_shards_exact(void *,s32,s32);
void ge_original_effect_pre_world_exact(void);
int ge_original_effect_render_room_exact(s32,s32,void *,int (*)(void *,void *,int));
void ge_original_effect_reset_audio_exact(void);

int ge_original_effect_runtime_reset(uint32_t count,int32_t stage)
{
    if(count<1 || count>4 || lifetime==UINT64_MAX)return 0;
    if(ge_original_effect_buffers_lifetime()==UINT64_MAX)return 0;
    ge_original_effect_reset_static_exact();
    if(!ge_original_effect_buffers_reset_players(count,stage))return 0;
    ge_original_effect_reset_shards_exact(shards,(s32)count,stage);
    ge_original_effect_reset_audio_exact();
    buffer_lifetime=ge_original_effect_buffers_lifetime();
    owned=(GeOriginalEffectRuntimeStorage){g_ExplosionBuffer,g_SmokeBuffer,
        g_ScorchBuffer,g_BulletImpactBuffer,g_FlyingParticlesBuffer,shards,
        g_BulletSparkArray,g_MovingBulletSparkArray,g_Casings,
        (uint32_t)max_particles,(uint32_t)SHATTERED_WINDOW_PIECES_BUFFER_LEN,
        count==1?SCORCH_BUFFER_LEN:0,count,stage,++lifetime};
    return 1;
}
int ge_original_effect_runtime_storage(GeOriginalEffectRuntimeStorage *storage)
{
    if(!storage || !owned.lifetime
            || buffer_lifetime!=ge_original_effect_buffers_lifetime() || owned.explosions!=g_ExplosionBuffer
            || owned.smoke!=g_SmokeBuffer || owned.scorches!=g_ScorchBuffer
            || owned.impacts!=g_BulletImpactBuffer || owned.particles!=g_FlyingParticlesBuffer
            || owned.shards!=ptr_shattered_window_pieces
            || owned.particle_count!=(uint32_t)max_particles
            || owned.shard_count!=(uint32_t)SHATTERED_WINDOW_PIECES_BUFFER_LEN)return 0;
    *storage=owned;return 1;
}
void ge_original_effect_runtime_end(void){owned.lifetime=0;}
int ge_original_effect_runtime_pre_world(void)
{
    GeOriginalEffectRuntimeStorage storage;
    if(!ge_original_effect_runtime_storage(&storage))return 0;
    ge_original_effect_pre_world_exact();return 1;
}
int ge_original_effect_runtime_retire_view(void)
{ return ge_original_effect_runtime_render_view(NULL,NULL); }

int ge_original_effect_runtime_render_view(void *context,
    int (*render_prop)(void *,void *,int))
{
    GeOriginalBgRenderRoom rooms[GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE];
    GeOriginalEffectRuntimeStorage storage;
    size_t count;
    if(!ge_original_effect_runtime_storage(&storage)
            || !ge_original_bg_visibility_render_rooms(rooms,
            GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE,&count))return 0;
    /* The first read validates the native count before the original bounded
     * loop. The prefix mutates native signed depth, observed by E9, then the
     * copied traversal must use that updated publication. */
    ge_original_bg_visibility_render_prefix_exact();
    if(!ge_original_bg_visibility_render_rooms(rooms,
            GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE,&count))return 0;
    /* Same room sequence as bg.c: signed unk1, original j
     * order within each depth. Skip absent depth values without iterating
     * their empty buckets; no native state changes during this ordering. */
    for(size_t i=1;i<count;++i) {
        GeOriginalBgRenderRoom room=rooms[i];size_t j=i;
        while(j && rooms[j-1].depth>room.depth){rooms[j]=rooms[j-1];--j;}
        rooms[j]=room;
    }
    for(size_t i=0;i<count;++i) {
        if(!ge_original_effect_render_room_exact(rooms[i].room,0,context,render_prop)
                || !ge_original_effect_render_room_exact(rooms[i].room,2,context,render_prop))return 0;
    }
    /* Reverse depth groups, retaining native j order inside each group. */
    for(size_t end=count;end;) {
        size_t start=end-1;
        while(start && rooms[start-1].depth==rooms[end-1].depth)--start;
        for(size_t i=start;i<end;++i)
            if(!ge_original_effect_render_room_exact(rooms[i].room,1,context,render_prop))return 0;
        end=start;
    }
    return 1;
}
