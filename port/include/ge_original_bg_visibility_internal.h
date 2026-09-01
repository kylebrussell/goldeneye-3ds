#ifndef GE_ORIGINAL_BG_VISIBILITY_INTERNAL_H
#define GE_ORIGINAL_BG_VISIBILITY_INTERNAL_H

#include <math.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef uint8_t u8;
typedef float f32;
typedef int32_t bool;

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define MAXROOMCOUNT 139
#define PORTMAX 200
#define BG_PORTAL_QUEUE_LEN 500
#define BG_STACK_SIZE 20
#define LEVEL_INDEX_CRAD 21
#define PORTALFLAG_DISABLED 0x01
#define PORTALFLAG_SPECIAL 0x02

/* This slice coexists with other independently bounded original subsystems.
 * Namespace only its harness storage and platform-service symbols; retain the
 * original bg function names and shared g_BgPortals ABI. */
#define g_BgRoomInfo ge_bg_visibility_room_info
#define g_BgNumberOfRoomsDrawn ge_bg_visibility_rooms_drawn
#define dword_CODE_bss_8007FFA0 ge_bg_visibility_draw_rooms
#define bgViewRelated ge_bg_visibility_view_related
#define D_800442FC ge_bg_visibility_portal_depths
#define g_BgPortalQueue ge_bg_visibility_portal_queue
#define D_80044898 ge_bg_visibility_descent_count
#define D_8004489C ge_bg_visibility_max_depth
#define g_BgPortalQueueWriteIndex ge_bg_visibility_queue_write
#define g_BgPortalQueueReadIndex ge_bg_visibility_queue_read
#define g_BgStack ge_bg_visibility_stack
#define g_BgStackCount ge_bg_visibility_stack_count
#define current_visibility ge_bg_visibility_current
#define table_for_portals ge_bg_visibility_portal_cache
#define room_data_float1 ge_bg_visibility_level_scale
#define room_data_float2 ge_bg_visibility_inverse_level_scale
#define mCurrentLevelVisibilityScale ge_bg_visibility_visibility_scale
#define levelentry_index ge_bg_visibility_level_index
#define D_80044858 ge_bg_visibility_room_cycle
#define D_80044900 ge_bg_visibility_portal_metric_distance
#define g_BgCurrentRoom ge_bg_visibility_current_room
#define g_MaxNumRooms ge_bg_visibility_max_rooms
#define dword_CODE_bss_8007FF98 ge_bg_visibility_debug_count
#define dword_CODE_bss_8007FF90 ge_bg_visibility_global_commands
#define list_visible_rooms_in_cur_global_vis_packet \
    ge_bg_visibility_global_rooms
#define num_visible_rooms_in_cur_global_vis_packet \
    ge_bg_visibility_global_room_count
#define g_CurrentPlayer ge_bg_visibility_player
#define camGetWorldToScreenMtxf ge_bg_visibility_world_to_screen
#define bondviewGetCurrentPlayersPosition ge_bg_visibility_player_position
#define mtx4TransformVecInPlace ge_bg_visibility_transform_vec
#define transform3Dto2DWithZScaling ge_bg_visibility_project_vec
#define viGetZRange ge_bg_visibility_get_z_range
#define viGetX ge_bg_visibility_get_vi_x
#define viGetY ge_bg_visibility_get_vi_y
#define viGetViewLeft ge_bg_visibility_get_view_left
#define viGetViewTop ge_bg_visibility_get_view_top
#define viGetViewWidth ge_bg_visibility_get_view_width
#define viGetViewHeight ge_bg_visibility_get_view_height
#define bgCheckIfRoomModelNeedsLoad ge_bg_visibility_room_needs_load
#define bbox2dCopy ge_bg_visibility_bbox_copy

typedef union Mtxf {
    f32 m[4][4];
    s32 unused;
} Mtxf;

typedef struct coord2d {
    union {
        struct { f32 x, y; };
        f32 f[2];
    };
} coord2d;

typedef struct coord3d {
    union {
        struct { f32 x, y, z; };
        f32 f[3];
    };
} coord3d;

typedef struct bbox2d {
    union {
        struct { coord2d min, max; };
        f32 f[2][2];
    };
} bbox2d;

struct rectbbox { f32 left, up, right, down; };

typedef struct s_room_info {
    u8 room_rendered;
    u8 room_neighbor_to_rendered;
    u8 model_bin_loaded;
    u8 portal_visit_count;
    /* N64 pointers are 32-bit ABI words.  Keeping them as storage words is
     * essential on a 64-bit host: this shared array is also consumed by
     * unchanged decompiled bodies compiled against src/game/bg.h. */
    uint32_t vertices;
    uint32_t ptr_expanded_mapping_info;
    uint32_t ptr_secondary_expanded_mapping_info;
    s32 csize_point_index_binary;
    s32 csize_primary_DL_binary;
    s32 csize_secondary_DL_binary;
    s32 usize_point_index_binary;
    s32 usize_primary_DL_binary;
    s32 usize_secondary_DL_binary;
    s32 cur_room_totalsize;
    uint32_t vtx_batch_bounds;
    s16 num_vtx_batch_bounds;
    s16 field_32;
    u8 room_loaded_mask;
    u8 field_35;
    s16 field_36;
    coord3d minbounds;
    coord3d maxbounds;
} s_room_info;

_Static_assert(offsetof(s_room_info, room_rendered) == 0x00U,
               "room rendered ABI offset");
_Static_assert(offsetof(s_room_info, room_loaded_mask) == 0x34U,
               "room loaded-mask ABI offset");
_Static_assert(offsetof(s_room_info, minbounds) == 0x38U,
               "room minimum-bounds ABI offset");
_Static_assert(offsetof(s_room_info, maxbounds) == 0x44U,
               "room maximum-bounds ABI offset");
_Static_assert(sizeof(s_room_info) == 0x50U,
               "canonical N64 room-info ABI size");

typedef struct s_bound_info {
    s32 roomid;
    s32 unk1;
    bbox2d bbox;
    s32 next;
} s_bound_info;

typedef struct bg_portal_entry {
    u8 numPoints;
    u8 padding[3];
    coord3d point;
} bg_portal_entry;

typedef struct bg_portal_data_entry {
    bg_portal_entry *offset_portal;
    u8 connectedRoom1;
    u8 connectedRoom2;
    u8 controlbytes1;
    u8 controlbytes2;
} bg_portal_data_entry;

typedef struct unk_portalstruct {
    union { s32 count; s32 unk0; };
    bbox2d bbox;
} PortalCache;

typedef struct bg_queued_portal_entry {
    s32 arg0;
    s32 roomnum;
    s32 portalnum;
    s32 arg3;
    f32 sp10[4];
} bg_queued_portal_entry;

struct PortalMetric {
    coord3d normal;
    f32 min;
    f32 max;
};

typedef struct GlobalVisCommand {
    u8 type;
    u8 length;
    u8 padding[2];
    s32 arg;
} GlobalVisCommand;

extern f32 D_80044900;
s32 bgGetPortalBetweenRooms(s32 room1, s32 room2, coord3d *point1,
                            coord3d *point2);
void bgToggleDataPortalsContrlBytes1Bit1(s32 portal, s32 toggle);
s32 sub_GAME_7F0B9F14(s32 portalnum, coord3d *point1, coord3d *point2);

_Static_assert(sizeof(GlobalVisCommand) == 8U,
               "original global-vis command ABI must remain 8 bytes");

typedef struct Unk80081600 {
    bbox2d unk0;
    s32 unk10;
    s32 unk14;
} Unk80081600;

enum GlobalVisOpcode {
    VISOP_END = 0x00, VISOP_PUSH = 0x01, VISOP_POP = 0x02,
    VISOP_AND = 0x03, VISOP_OR = 0x04, VISOP_NOT = 0x05,
    VISOP_XOR = 0x06, VISOP_PUSH_IF_ROOM_IN_RANGE = 0x14,
    VISOP_FORCE_VISIBLE = 0x1e, VISOP_MATCH_PORTAL_VIS = 0x1f,
    VISOP_ADD_VISIBLE_ROOM = 0x20, VISOP_REMOVE_VIS = 0x21,
    VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x22,
    VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL = 0x23,
    VISOP_DISABLE_ROOM = 0x24, VISOP_DISABLE_ROOM_RANGE = 0x25,
    VISOP_PRELOAD_ROOM = 0x26, VISOP_PRELOAD_ROOM_RANGE = 0x27,
    VISOP_IF_STATEMENT = 0x50,
    VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN = 0x51,
    VISOP_ENDIF_CONTINUE_EXEC = 0x52,
    VISOP_IF_STATEMENT_PULL_FROM_STACK = 0x5a,
    VISOP_TOGGLE_EXEC_VS_READONLY = 0x5b, VISOP_ENDIF = 0x5c
};

typedef struct player {
    bbox2d screensize;
    f32 c_screenleft;
    f32 c_screentop;
    f32 c_halfwidth;
    f32 c_halfheight;
    f32 c_recipscalex;
    f32 c_recipscaley;
} player;

extern s_room_info g_BgRoomInfo[MAXROOMCOUNT];
extern s32 g_BgNumberOfRoomsDrawn;
extern s_bound_info dword_CODE_bss_8007FFA0[204];
extern s32 bgViewRelated[4];
extern u8 D_800442FC[PORTMAX];
extern bg_queued_portal_entry g_BgPortalQueue[BG_PORTAL_QUEUE_LEN];
extern bg_portal_data_entry *g_BgPortals;
extern s32 D_80044898, D_8004489C;
extern s32 g_BgPortalQueueWriteIndex, g_BgPortalQueueReadIndex;
extern s32 g_BgStack[BG_STACK_SIZE], g_BgStackCount;
extern s32 current_visibility;
extern struct unk_portalstruct table_for_portals[PORTMAX];
extern f32 room_data_float1, room_data_float2;
extern f32 mCurrentLevelVisibilityScale;
extern s32 levelentry_index, D_80044858, g_BgCurrentRoom;
extern s32 g_MaxNumRooms, dword_CODE_bss_8007FF98;
extern s32 *dword_CODE_bss_8007FF90;
extern char list_visible_rooms_in_cur_global_vis_packet[0x98];
extern s32 sub_GAME_7F0B9E04(coord3d *arg0, coord3d *arg1);
extern s32 num_visible_rooms_in_cur_global_vis_packet;
extern player *g_CurrentPlayer;

Mtxf *camGetWorldToScreenMtxf(void);
coord3d *bondviewGetCurrentPlayersPosition(void);
void mtx4TransformVecInPlace(Mtxf *matrix, coord3d *vector);
void transform3Dto2DWithZScaling(coord3d *in, void *out);
void bbox2dCopy(bbox2d *destination, const bbox2d *source);
void viGetZRange(f32 *zrange);
s16 viGetX(void);
s16 viGetY(void);
s16 viGetViewLeft(void);
s16 viGetViewTop(void);
s16 viGetViewWidth(void);
s16 viGetViewHeight(void);
s32 bgCheckIfRoomModelNeedsLoad(s32 roomID);

s32 sub_GAME_7F0B39BC(int curroom, int unk1, bbox2d *screensize,
                      s32 next);
void bgResetPortalVisitCounts(void);
void sub_GAME_7F0B5168(void);
bool bgIsRoomOnScreen(s32 roomID, struct rectbbox *screenbox);
bool bgProjectRoomCoordToScreen(coord3d *src, coord3d *dst);
s32 sub_GAME_7F0B5528(s32 portalnum, f32 arg1, coord3d *arg2);
s32 sub_GAME_7F0B5864(s32 portalnum, bbox2d *bbox);
s32 bgRectIntersect(bbox2d *a, bbox2d *b);
void bgRectOutersect(bbox2d *a, bbox2d *b);
void bgResetPortalQueue(void);
u8 bgIncrementRoomPortalVisitCount(s32 roomnum);
void bgQueuePortalTraversal(s32 arg0, s32 arg1, s32 portalnum, s32 depth,
                            f32 *arg4);
bool bgProcessNextQueuedPortal(s32 *arg0);
s32 sub_GAME_7F0B7F84(s32 value, s32 roomnum, s32 portalnum, s32 depth,
                      bbox2d *parentbox);
s32 bgStackPush(s32 arg0);
s32 bgStackPop(void);
s32 bgStackGetNthValueFromEnd(s32 n);
GlobalVisCommand *parse_global_vis_command_list(GlobalVisCommand *cmd,
                                                s32 execute);
void *sub_GAME_7F0B8A24(s32 *pc);
void bgDetermineVisibleRooms(void);
void bgUpdateCurrentPlayerScreenMinMax(void);
void bgGetRoomCenter(s32 roomnum, coord3d *dst);
void sub_GAME_7F0B96CC(s32 portalnum, f32 *out);
f32 sub_GAME_7F0B9990(s32 portalnum);
s8 bgSwapConnectedRooms(s32 index);
void bgOrderPortal(s32 portalnum);

#endif
