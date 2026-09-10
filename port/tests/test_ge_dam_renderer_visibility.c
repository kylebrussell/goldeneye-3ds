/* Include the adapter to snapshot ALL private canonical execution storage,
 * not merely the public room flags. The renderer links as another TU. */
#include "../src/ge_original_bg_visibility.c"
#include "ge_dam_renderer_visibility.h"
#include <assert.h>
#include <stdio.h>

static uint8_t before[100000], after[100000];
static size_t snapshot(uint8_t *bytes)
{
    size_t n = 0;
#define COPY(v) do { assert(n + sizeof(v) <= sizeof(before)); memcpy(bytes+n, &(v), sizeof(v)); n += sizeof(v); } while (0)
    COPY(g_BgRoomInfo); COPY(g_BgNumberOfRoomsDrawn); COPY(dword_CODE_bss_8007FFA0);
    COPY(bgViewRelated); COPY(D_800442FC); COPY(g_BgPortalQueue); COPY(g_BgPortals);
    COPY(D_80044898); COPY(D_8004489C); COPY(g_BgPortalQueueWriteIndex);
    COPY(g_BgPortalQueueReadIndex); COPY(g_BgStack); COPY(g_BgStackCount);
    COPY(current_visibility); COPY(table_for_portals); COPY(room_data_float1);
    COPY(room_data_float2); COPY(mCurrentLevelVisibilityScale); COPY(levelentry_index);
    COPY(D_80044858); COPY(g_BgCurrentRoom); COPY(g_MaxNumRooms); COPY(dword_CODE_bss_8007FF98);
    COPY(ge_bg_visibility_portal_metric_padding); COPY(D_80044900); COPY(dword_CODE_bss_8007FF90);
    COPY(list_visible_rooms_in_cur_global_vis_packet); COPY(num_visible_rooms_in_cur_global_vis_packet);
    COPY(g_CurrentPlayer); COPY(ge_portals); COPY(ge_portal_geometry); COPY(ge_portal_addresses);
    COPY(ge_global_commands); COPY(ge_player); COPY(ge_player_position); COPY(ge_world_to_screen);
    COPY(ge_near_distance); COPY(ge_far_distance); COPY(ge_projection_offset_px);
    COPY(ge_view_left); COPY(ge_view_top); COPY(ge_view_width); COPY(ge_view_height);
    COPY(ge_providers); COPY(ge_preload_request_count); COPY(ge_global_command_count);
    COPY(ge_global_stream_hash);
#undef COPY
    return n;
}

static void be32(uint8_t *data, uint32_t value)
{
    data[0] = value >> 24; data[1] = value >> 16; data[2] = value >> 8; data[3] = value;
}
static void bef(uint8_t *data, float value)
{
    uint32_t bits; memcpy(&bits, &value, 4); be32(data, bits);
}
static void portal(uint8_t *bg, size_t i, float x)
{
    size_t geometry = 128 + i * 52;
    be32(bg + 96 + i * 8, GE_BG_BASE + geometry);
    bg[100 + i * 8] = 2 + i; bg[101 + i * 8] = 1;
    bg[geometry] = 4;
    const float points[4][2] = {{-0.25f,-1},{-0.25f,1},{0.25f,1},{0.25f,-1}};
    for (size_t p=0;p<4;++p) {
        bef(bg+geometry+4+p*12, x+points[p][0]);
        bef(bg+geometry+8+p*12, points[p][1]);
        bef(bg+geometry+12+p*12, -10);
    }
}
static size_t forbidden_calls;
static uint8_t forbidden(void *context, uint8_t room)
{
    (void)context; (void)room; ++forbidden_calls; return 0;
}
static int has(const GeOriginalBgVisibilityResult *r, uint8_t room)
{
    for (size_t i=0;i<r->room_count;++i) if (r->rooms[i].room == room) return 1;
    return 0;
}
int main(void)
{
    uint8_t bg[256] = {0};
    be32(bg+8, GE_BG_BASE+96);
    portal(bg,0,-8.5f); portal(bg,1,8.5f);
    GeDamVisibilityRuntime runtime = {0};
    runtime.background = bg; runtime.background_size=sizeof(bg);
    runtime.room_count=4; runtime.loaded=1; runtime.level_index=13;
    runtime.level_scale=runtime.visibility_scale=1;
    runtime.near_distance=1; runtime.far_distance=1000;
    for (size_t r=1;r<4;++r) {
        runtime.room_bounds[r]=(GeOriginalBgRoomBounds){{-10,-2,-12},{10,2,-8}};
    }
    GeOriginalBondCameraResult camera = {0};
    for (size_t i=0;i<4;++i) camera.view[i][i]=1;
    camera.room=1; camera.viewport_scale[0]=640; camera.viewport_scale[1]=480;
    camera.projection[0][0]=1.2990381f; camera.projection[1][1]=1.7320508f;
    camera.projection[2][3]=-1;
    const float position[3]={0};
    const GeRendererEyeShift zero[2]={{0},{0}};
    const GeRendererEyeShift shifts[2]={{1440,-8},{-1440,8}};
    GeOriginalBgVisibilityInput input;
    assert(ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&zero[0],&input));
    /* Canonical wrapper uses its original symmetric FOV path. */
    input.projection_focal_px[0]=input.projection_focal_px[1]=0;
    GeOriginalBgVisibilityResult canonical;
    GeDamPreloadQueue queue;
    uint8_t initial=1, controls[2]={0};
    assert(ge_dam_preload_queue_init(&queue,4,4,&initial,1)==GE_DAM_PRELOAD_OK);
    GeOriginalBgVisibilityProviders providers={NULL,forbidden,controls,2};
    input.providers=&providers;
    runtime.program=ge_original_bg_visibility_program_create(bg,sizeof(bg),4,NULL);
    assert(runtime.program);
    input.program=runtime.program;
    assert(ge_original_bg_visibility_run(&input,&canonical)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(has(&canonical,1) && !has(&canonical,2) && !has(&canonical,3));
    const size_t length=snapshot(before);
    GeDamRendererVisibility result;
    GeDamPreloadQueue original_queue=queue;
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &canonical,zero,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(memcmp(&canonical,&result.visible,sizeof(canonical))==0);
    assert(!result.stereo);
    /* The queue's 137-room array is smaller than the 139-room renderer
     * membership. Its adjacent struct padding must never become requests. */
    {
        GeDamPreloadQueue padded_queue=queue;
        const size_t request_end=offsetof(GeDamPreloadQueue,visibility_requested)
            +sizeof(padded_queue.visibility_requested);
        memset((uint8_t *)&padded_queue+request_end,0xa5,
            offsetof(GeDamPreloadQueue,room_count)-request_end);
        assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&padded_queue,
            &canonical,zero,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
        for(size_t room=sizeof(padded_queue.visibility_requested);room<sizeof(result.preload);++room)
            assert(result.preload[room]==0);
    }
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &canonical,shifts,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(result.membership[2]==2 && result.membership[3]==4);
    assert(result.preload[2] && result.preload[3]);
    assert(result.visible.room_count==3);
    assert(forbidden_calls==0 && memcmp(&queue,&original_queue,sizeof(queue))==0);
    assert(snapshot(after)==length && memcmp(before,after,length)==0);
    /* An explicit commit loads only authorized eye rooms, protects the mono
     * resident, and remains idempotent while requests await installation. */
    for (size_t room=0;room<4;++room) if(result.preload[room]) ge_dam_preload_queue_request(&queue,room);
    assert(queue.pending_count==2 && queue.states[1]==GE_DAM_PRELOAD_ROOM_RESIDENT);
    for (unsigned pass=0;pass<32;++pass) {
        controls[0]=pass&1; controls[1]=(pass>>1)&1;
        assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
            &canonical,shifts,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
        assert((result.membership[2]!=0)==!(controls[0]&1));
        assert((result.membership[3]!=0)==!(controls[1]&1));
        assert(snapshot(after)==length && memcmp(before,after,length)==0);
    }
    /* Algebraic shader agreement, including an authored off-axis center and
     * camera rotation: x_eye=f*(x + C/f)/W + O + K. */
    controls[0]=controls[1]=0;
    camera.projection[2][0]=0.125f;
    for(size_t eye=0;eye<2;++eye) {
        assert(ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&shifts[eye],&input));
        for(float depth=1;depth<2000;depth*=1.75f) {
            float actual=input.projection_focal_px[0]*(2+input.world_to_screen[3][0])/depth
                +input.projection_offset_px[0];
            float expect=input.projection_focal_px[0]*2/depth-20
                +shifts[eye].constant_px/depth+shifts[eye].coefficient_px;
            assert(fabsf(actual-expect)<0.001f);
        }
        assert(fabsf(input.player_position[0]+input.world_to_screen[3][0])<0.0001f);
    }
    camera.projection[2][0]=0;
    /* A rotated view uses the inverse basis for the isolated eye position. */
    camera.view[0][0]=0.8f; camera.view[2][0]=0.6f;
    camera.view[0][2]=-0.6f; camera.view[2][2]=0.8f;
    assert(ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&shifts[0],&input));
    for (size_t col=0;col<3;++col) {
        float eye_delta=0;
        for(size_t row=0;row<3;++row)
            eye_delta+=(input.player_position[row]-position[row])*camera.view[row][col];
        assert(fabsf(eye_delta+(col==0 ? input.world_to_screen[3][0] : 0))<0.0001f);
    }
    camera.projection[0][1]=0.25f;
    assert(!ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&shifts[0],&input));
    camera.projection[0][1]=0;
    camera.projection[3][0]=1;
    assert(!ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&shifts[0],&input));
    camera.projection[3][0]=0;
    memset(camera.view,0,sizeof(camera.view));
    for(size_t axis=0;axis<4;++axis)camera.view[axis][axis]=1;
    /* A maximal canonical list may contain duplicates. Stereo retains the
     * canonical room but deduplicates before adding either eye's rooms. */
    GeOriginalBgVisibilityResult repeated=canonical;
    for(size_t i=1;i<GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE;++i)
        repeated.rooms[i]=repeated.rooms[0];
    repeated.room_count=GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE;
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &repeated,zero,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(memcmp(&repeated,&result.visible,sizeof(repeated))==0);
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &repeated,shifts,&result)==GE_ORIGINAL_BG_VISIBILITY_OK && result.visible.room_count==3);
    GeDamRendererVisibility saved=result;
    GeRendererEyeShift bad[2]={{NAN,0},{0}};
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &canonical,bad,&result)==GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT);
    assert(memcmp(&saved,&result,sizeof(result))==0);
    assert(snapshot(after)==length && memcmp(before,after,length)==0);
    ge_original_bg_visibility_program_close(runtime.program); runtime.program=NULL;
    /* Authored global DISABLE_ROOM precedes traversal and remains effective
     * even though that room is geometrically eye-visible. */
    be32(bg+12,GE_BG_BASE+72);
    bg[72]=0x24; bg[73]=2; bg[80]=101; be32(bg+84,2); bg[88]=0; bg[89]=1;
    input.program=NULL; input.background=bg;
    assert(ge_dam_renderer_visibility_eye_input(&runtime,&camera,position,&zero[0],&input));
    input.providers=&providers;
    assert(ge_original_bg_visibility_run(&input,&canonical)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(canonical.global_visibility_used);
    assert(ge_dam_renderer_visibility_run(&runtime,&camera,position,&providers,&queue,
        &canonical,shifts,&result)==GE_ORIGINAL_BG_VISIBILITY_OK);
    assert(!result.membership[2] && result.membership[3]==4);
    puts("renderer eye visibility: canonical bytes, mono identity, doorway eyes, streaming, door/global rules passed");
}
