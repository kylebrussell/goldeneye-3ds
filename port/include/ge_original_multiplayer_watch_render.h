#ifndef GE_ORIGINAL_MULTIPLAYER_WATCH_RENDER_H
#define GE_ORIGINAL_MULTIPLAYER_WATCH_RENDER_H
#include <stdint.h>
#define GE_ORIGINAL_MP_WATCH_DRAW_CAPACITY 24U
#define GE_ORIGINAL_MP_WATCH_TEXT_CAPACITY 256U
struct Ge3dsOriginalHudAtlas;
typedef enum GeOriginalMultiplayerWatchRenderStatus {
    GE_ORIGINAL_MP_WATCH_RENDER_OK,
    GE_ORIGINAL_MP_WATCH_RENDER_INVALID,
    GE_ORIGINAL_MP_WATCH_RENDER_RESOURCE_FAILED,
    GE_ORIGINAL_MP_WATCH_RENDER_CAPACITY
} GeOriginalMultiplayerWatchRenderStatus;
typedef struct GeOriginalMultiplayerWatchViewport {
    int32_t screen_width,screen_height,left,top,width,height;
    /* Actual text renderer resource settings; native stage defaults are
     * spacing=0, overlap_correction=-1. Japanese fonts are not supplied. */
    int32_t text_spacing,overlap_correction;
} GeOriginalMultiplayerWatchViewport;
typedef struct GeOriginalMultiplayerWatchTextDraw {
    int32_t x,y,width,height,lineheight,y_offset;
    uint32_t rgba,outline_rgba;
    uint8_t outlined;
    char text[GE_ORIGINAL_MP_WATCH_TEXT_CAPACITY];
} GeOriginalMultiplayerWatchTextDraw;
typedef struct GeOriginalMultiplayerWatchRenderSnapshot {
    GeOriginalMultiplayerWatchViewport viewport;
    GeOriginalMultiplayerWatchTextDraw draws[GE_ORIGINAL_MP_WATCH_DRAW_CAPACITY];
    uint32_t count,player;
    uint8_t constructed,restored,scissor,visible;
} GeOriginalMultiplayerWatchRenderSnapshot;
/* Selected native player must equal slot. No selection, VI publication,
 * tick, RNG or game-state mutation. Source menu decisions and exact font
 * measurements are captured into owned strings; invalid/partial output has
 * no published commands. Run while native owners are still alive. */
GeOriginalMultiplayerWatchRenderStatus ge_original_multiplayer_watch_render_capture(
    uint32_t slot,const GeOriginalMultiplayerWatchViewport *,
    const struct Ge3dsOriginalHudAtlas *,GeOriginalMultiplayerWatchRenderSnapshot *);
#endif
