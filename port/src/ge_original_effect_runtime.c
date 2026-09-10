#include "ge_original_effect_runtime.h"
#include "ge_original_effect_buffers.h"
#include "ge_original_bg_visibility.h"
#include <ultra64.h>
#include <bondtypes.h>
#include "game/glass.h"

static _Alignas(16) s_shattered_window_piece shards[200];
void ge_original_effect_reset_static_exact(void);
void ge_original_effect_reset_shards_exact(void *,s32,s32);
void ge_original_effect_pre_world_exact(void);
int ge_original_effect_render_room_exact(s32,s32,void *,int (*)(void *,void *,int));
void ge_original_effect_reset_audio_exact(void);

int ge_original_effect_runtime_reset(uint32_t count,int32_t stage)
{
    if(count<1 || count>4)return 0;
    ge_original_effect_reset_static_exact();
    if(!ge_original_effect_buffers_reset_players(count,stage))return 0;
    ge_original_effect_reset_shards_exact(shards,(s32)count,stage);
    ge_original_effect_reset_audio_exact();
    return 1;
}
int ge_original_effect_runtime_pre_world(void)
{
    if(ptr_shattered_window_pieces!=shards || SHATTERED_WINDOW_PIECES_BUFFER_LEN<1
            || SHATTERED_WINDOW_PIECES_BUFFER_LEN>200)return 0;
    ge_original_effect_pre_world_exact();return 1;
}
int ge_original_effect_runtime_retire_view(void)
{ return ge_original_effect_runtime_render_view(NULL,NULL); }

int ge_original_effect_runtime_render_view(void *context,
    int (*render_prop)(void *,void *,int))
{
    GeOriginalBgRenderRoom rooms[GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE];
    size_t count;
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
