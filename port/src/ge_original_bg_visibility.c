#include "ge_original_bg_visibility.h"
#include "ge_original_bg_visibility_internal.h"

#include <math.h>
#include <string.h>

#define GE_BG_BASE UINT32_C(0x0f000000)
#define GE_GLOBAL_VIS_MAX_COMMANDS 512U
#define GE_GLOBAL_VIS_PORTAL_ARGUMENT 100U
#define GE_GLOBAL_VIS_INTEGER_ARGUMENT 101U
/* sub_GAME_7F0B5864's unchanged scratch space holds 19 transformed points.
 * A portal may contribute its authored points from both sides, so the exact
 * safe source bound is floor(19 / 2). Depot contains a canonical seven-point
 * portal; the earlier six-point adapter rejected that valid background. */
#define GE_BG_PORTAL_MAX_POINTS 9U

s_room_info g_BgRoomInfo[MAXROOMCOUNT];
s32 g_BgNumberOfRoomsDrawn;
s_bound_info dword_CODE_bss_8007FFA0[204];
s32 bgViewRelated[4] = {1, 1, -1, -1};
u8 D_800442FC[PORTMAX];
bg_queued_portal_entry g_BgPortalQueue[BG_PORTAL_QUEUE_LEN];
bg_portal_data_entry *g_BgPortals;
s32 D_80044898;
s32 D_8004489C = 15;
s32 g_BgPortalQueueWriteIndex;
s32 g_BgPortalQueueReadIndex;
s32 g_BgStack[BG_STACK_SIZE];
s32 g_BgStackCount;
s32 current_visibility;
struct unk_portalstruct table_for_portals[PORTMAX];
f32 room_data_float1 = 1.0f;
f32 room_data_float2 = 1.0f;
f32 mCurrentLevelVisibilityScale = 1.0f;
s32 levelentry_index = 13;
s32 D_80044858;
s32 g_BgCurrentRoom = 1;
s32 g_MaxNumRooms = MAXROOMCOUNT;
s32 dword_CODE_bss_8007FF98;
s32 ge_bg_visibility_portal_metric_padding;
f32 D_80044900;
s32 *dword_CODE_bss_8007FF90;
char list_visible_rooms_in_cur_global_vis_packet[0x98];
s32 num_visible_rooms_in_cur_global_vis_packet;
player *g_CurrentPlayer;

typedef struct GePortalGeometryStorage {
    u8 numPoints;
    u8 padding[3];
    coord3d points[GE_BG_PORTAL_MAX_POINTS];
} GePortalGeometryStorage;

static bg_portal_data_entry ge_portals[PORTMAX + 1U];
static GePortalGeometryStorage ge_portal_geometry[PORTMAX];
static uint32_t ge_portal_addresses[PORTMAX];
static GlobalVisCommand ge_global_commands[GE_GLOBAL_VIS_MAX_COMMANDS];
static player ge_player;
static coord3d ge_player_position;
static Mtxf ge_world_to_screen;
static f32 ge_near_distance;
static f32 ge_far_distance;
static s16 ge_view_left;
static s16 ge_view_top;
static s16 ge_view_width;
static s16 ge_view_height;
static const GeOriginalBgVisibilityProviders *ge_providers;
static size_t ge_preload_request_count;
static size_t ge_global_command_count;
static uint64_t ge_global_stream_hash;

static uint32_t ge_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static float ge_be_float(const uint8_t *data)
{
    const uint32_t bits = ge_be32(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

static int ge_offset(uint32_t address, size_t size, size_t *offset)
{
    uint32_t relative;

    if (address < GE_BG_BASE) return 0;
    relative = address - GE_BG_BASE;
    if ((uint64_t)relative >= size) return 0;
    *offset = relative;
    return 1;
}

static uint64_t ge_fnv1a64(const uint8_t *data, size_t size)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;

    for (index = 0U; index < size; ++index) {
        value ^= data[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static GeOriginalBgVisibilityStatus ge_materialize_portals(
    const uint8_t *background, size_t background_size,
    size_t room_count, size_t *portal_count)
{
    size_t table_offset;
    size_t index;

    if (background_size < 20U
            || !ge_offset(ge_be32(background + 8U), background_size,
                          &table_offset)) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
    }
    memset(ge_portals, 0, sizeof(ge_portals));
    memset(ge_portal_geometry, 0, sizeof(ge_portal_geometry));
    memset(ge_portal_addresses, 0, sizeof(ge_portal_addresses));
    for (index = 0U; index < PORTMAX; ++index) {
        const size_t record_offset = table_offset + index * 8U;
        uint32_t geometry_address;
        size_t geometry_offset;
        size_t point_index;
        uint8_t point_count;

        if (record_offset > background_size
                || background_size - record_offset < 8U) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        geometry_address = ge_be32(background + record_offset);
        if (geometry_address == 0U) break;
        if (!ge_offset(geometry_address, background_size, &geometry_offset)
                || background_size - geometry_offset < 4U) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        point_count = background[geometry_offset];
        if (point_count < 3U || point_count > GE_BG_PORTAL_MAX_POINTS
                || background_size - geometry_offset
                    < 4U + (size_t)point_count * 12U) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        if (background[record_offset + 4U] >= room_count
                || background[record_offset + 5U] >= room_count) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        ge_portal_geometry[index].numPoints = point_count;
        for (point_index = 0U; point_index < point_count; ++point_index) {
            const uint8_t *point = background + geometry_offset + 4U
                + point_index * 12U;
            ge_portal_geometry[index].points[point_index].x =
                ge_be_float(point);
            ge_portal_geometry[index].points[point_index].y =
                ge_be_float(point + 4U);
            ge_portal_geometry[index].points[point_index].z =
                ge_be_float(point + 8U);
        }
        ge_portals[index].offset_portal =
            (bg_portal_entry *)&ge_portal_geometry[index];
        ge_portal_addresses[index] = geometry_address;
        ge_portals[index].connectedRoom1 = background[record_offset + 4U];
        ge_portals[index].connectedRoom2 = background[record_offset + 5U];
        ge_portals[index].controlbytes1 = background[record_offset + 6U];
        ge_portals[index].controlbytes2 = background[record_offset + 7U];
    }
    if (index == PORTMAX) return GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED;
    *portal_count = index;
    return GE_ORIGINAL_BG_VISIBILITY_OK;
}

static int ge_global_expected_length(uint8_t type)
{
    switch (type) {
    case VISOP_END:
    case VISOP_PUSH:
    case VISOP_POP:
    case VISOP_AND:
    case VISOP_OR:
    case VISOP_NOT:
    case VISOP_XOR:
    case VISOP_FORCE_VISIBLE:
    case VISOP_REMOVE_VIS:
    case VISOP_IF_STATEMENT:
    case VISOP_DONT_EXEC_COMMANDS_EVEN_ON_RETURN:
    case VISOP_ENDIF_CONTINUE_EXEC:
    case VISOP_IF_STATEMENT_PULL_FROM_STACK:
    case VISOP_TOGGLE_EXEC_VS_READONLY:
    case VISOP_ENDIF:
        return 1;
    case VISOP_MATCH_PORTAL_VIS:
    case VISOP_ADD_VISIBLE_ROOM:
    case VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL:
    case VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL:
    case VISOP_DISABLE_ROOM:
    case VISOP_PRELOAD_ROOM:
        return 2;
    case VISOP_PUSH_IF_ROOM_IN_RANGE:
    case VISOP_DISABLE_ROOM_RANGE:
    case VISOP_PRELOAD_ROOM_RANGE:
        return 3;
    default:
        return 0;
    }
}

static GeOriginalBgVisibilityStatus ge_materialize_global_visibility(
    const uint8_t *background, size_t background_size, size_t room_count,
    size_t portal_count)
{
    uint32_t address;
    size_t stream_offset;
    size_t portal_offset;
    size_t byte_count;
    size_t command_count;
    size_t index;
    size_t depth = 0U;

    ge_global_command_count = 0U;
    ge_global_stream_hash = 0U;
    if (background_size < 20U) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
    }
    address = ge_be32(background + 12U);
    if (address == 0U) return GE_ORIGINAL_BG_VISIBILITY_OK;
    if (!ge_offset(address, background_size, &stream_offset)
            || !ge_offset(ge_be32(background + 8U), background_size,
                          &portal_offset)
            || portal_offset <= stream_offset) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
    }
    byte_count = portal_offset - stream_offset;
    if (byte_count % 8U != 0U) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
    }
    command_count = byte_count / 8U;
    if (command_count == 0U
            || command_count > GE_GLOBAL_VIS_MAX_COMMANDS) {
        return GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED;
    }
    for (index = 0U; index < command_count; ++index) {
        const uint8_t *source = background + stream_offset + index * 8U;
        uint32_t argument = ge_be32(source + 4U);
        size_t portal;

        if (source[2] != 0U || source[3] != 0U) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        ge_global_commands[index].type = source[0];
        ge_global_commands[index].length = source[1];
        ge_global_commands[index].padding[0] = 0U;
        ge_global_commands[index].padding[1] = 0U;
        if (source[0] == GE_GLOBAL_VIS_PORTAL_ARGUMENT) {
            for (portal = 0U; portal < portal_count; ++portal) {
                if (ge_portal_addresses[portal] == argument) break;
            }
            if (portal == portal_count) {
                return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
            }
            ge_global_commands[index].arg = (s32)portal;
        } else {
            ge_global_commands[index].arg = (s32)argument;
        }
    }
    for (index = 0U; index < command_count;) {
        GlobalVisCommand *command = &ge_global_commands[index];
        const int expected = ge_global_expected_length(command->type);
        size_t argument_index;

        if (expected == 0 || command->length != (u8)expected
                || index + (size_t)expected > command_count) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        if (command->type == VISOP_END) {
            if (index + 1U != command_count || depth != 0U) {
                return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
            }
            break;
        }
        for (argument_index = 1U;
                argument_index < (size_t)expected; ++argument_index) {
            GlobalVisCommand *argument = &ge_global_commands[
                index + argument_index];
            const int portal_argument = command->type == VISOP_MATCH_PORTAL_VIS
                || command->type == VISOP_VISIBLE_IF_SEEN_THROUGH_PORTAL
                || command->type == VISOP_NOT_VISIBLE_IF_SEEN_THROUGH_PORTAL;
            if (argument->length != 0U
                    || argument->type != (portal_argument
                        ? GE_GLOBAL_VIS_PORTAL_ARGUMENT
                        : GE_GLOBAL_VIS_INTEGER_ARGUMENT)
                    || (!portal_argument && (argument->arg < 0
                        || (size_t)argument->arg >= room_count))) {
                return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
            }
        }
        if (command->type == VISOP_IF_STATEMENT_PULL_FROM_STACK
                || command->type == VISOP_IF_STATEMENT) {
            ++depth;
        } else if (command->type == VISOP_ENDIF
                || command->type == VISOP_ENDIF_CONTINUE_EXEC) {
            if (depth == 0U) {
                return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
            }
            --depth;
        }
        index += (size_t)expected;
    }
    if (ge_global_commands[command_count - 1U].type != VISOP_END) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
    }
    ge_global_stream_hash = ge_fnv1a64(background + stream_offset,
                                       byte_count);
    ge_global_command_count = command_count;
    return GE_ORIGINAL_BG_VISIBILITY_OK;
}

Mtxf *camGetWorldToScreenMtxf(void)
{
    return &ge_world_to_screen;
}

coord3d *bondviewGetCurrentPlayersPosition(void)
{
    return &ge_player_position;
}

void mtx4TransformVecInPlace(Mtxf *matrix, coord3d *vector)
{
    coord3d result;
    size_t axis;

    for (axis = 0U; axis < 3U; ++axis) {
        result.f[axis] = matrix->m[0][axis] * vector->x
            + matrix->m[1][axis] * vector->y
            + matrix->m[2][axis] * vector->z
            + matrix->m[3][axis];
    }
    *vector = result;
}

void transform3Dto2DWithZScaling(coord3d *in, void *opaque_out)
{
    coord2d *out = opaque_out;
    const f32 inverse_z = in->z == 0.0f ? -1.0e20f : 1.0f / in->z;

    out->y = in->y * inverse_z * g_CurrentPlayer->c_recipscaley
        + g_CurrentPlayer->c_screentop + g_CurrentPlayer->c_halfheight;
    out->x = g_CurrentPlayer->c_screenleft + g_CurrentPlayer->c_halfwidth
        - in->x * inverse_z * g_CurrentPlayer->c_recipscalex;
}

void bbox2dCopy(bbox2d *destination, const bbox2d *source)
{
    *destination = *source;
}

void viGetZRange(f32 *zrange)
{
    zrange[0] = ge_near_distance;
    zrange[1] = ge_far_distance;
}

s16 viGetX(void) { return ge_view_width; }
s16 viGetY(void) { return ge_view_height; }
s16 viGetViewLeft(void) { return ge_view_left; }
s16 viGetViewTop(void) { return ge_view_top; }
s16 viGetViewWidth(void) { return ge_view_width; }
s16 viGetViewHeight(void) { return ge_view_height; }

s32 bgCheckIfRoomModelNeedsLoad(s32 roomID)
{
    ++ge_preload_request_count;
    if (ge_providers != NULL && ge_providers->preload_room != NULL) {
        return ge_providers->preload_room(ge_providers->context,
                                          (uint8_t)roomID) != 0U;
    }
    return 0;
}

GeOriginalBgVisibilityStatus ge_original_bg_visibility_run(
    const GeOriginalBgVisibilityInput *input,
    GeOriginalBgVisibilityResult *result)
{
    GeOriginalBgVisibilityStatus status;
    size_t portal_count;
    size_t room_index;
    size_t output_index;
    const float half_fov_radians = input != NULL
        ? input->vertical_fov_degrees * 0.00872664625997164788f : 0.0f;
    float tangent;

    if (input == NULL || result == NULL || input->background == NULL
            || input->room_bounds == NULL || input->room_count == 0U
            || input->room_count > MAXROOMCOUNT
            || input->current_room >= input->room_count
            || input->level_scale <= 0.0f
            || input->visibility_scale <= 0.0f
            || input->vertical_fov_degrees <= 0.0f
            || input->aspect_ratio <= 0.0f
            || input->view_width <= 0 || input->view_height <= 0) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
    }
    status = ge_materialize_portals(input->background,
        input->background_size, input->room_count, &portal_count);
    if (status != GE_ORIGINAL_BG_VISIBILITY_OK) return status;
    status = ge_materialize_global_visibility(input->background,
        input->background_size, input->room_count, portal_count);
    if (status != GE_ORIGINAL_BG_VISIBILITY_OK) return status;
    if (input->providers != NULL
            && input->providers->portal_controls != NULL
            && input->providers->portal_control_count < portal_count) {
        return GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT;
    }
    memset(result, 0, sizeof(*result));
    memset(g_BgRoomInfo, 0, sizeof(g_BgRoomInfo));
    memset(dword_CODE_bss_8007FFA0, 0, sizeof(dword_CODE_bss_8007FFA0));
    memset(D_800442FC, 0, sizeof(D_800442FC));
    memset(g_BgStack, 0, sizeof(g_BgStack));
    for (room_index = 0U; room_index < input->room_count; ++room_index) {
        memcpy(g_BgRoomInfo[room_index].minbounds.f,
               input->room_bounds[room_index].minimum, sizeof(coord3d));
        memcpy(g_BgRoomInfo[room_index].maxbounds.f,
               input->room_bounds[room_index].maximum, sizeof(coord3d));
    }
    g_BgPortals = ge_portals;
    ge_providers = input->providers;
    ge_preload_request_count = 0U;
    if (ge_providers != NULL && ge_providers->portal_controls != NULL) {
        for (room_index = 0U; room_index < portal_count; ++room_index) {
            ge_portals[room_index].controlbytes1 =
                ge_providers->portal_controls[room_index];
        }
    }
    g_BgCurrentRoom = input->current_room;
    /* The original loader publishes the selected background's exact room
     * count before visibility runs.  This matters for authored zero-portal
     * stages, whose canonical fallback scans every room rather than walking
     * portal adjacency. */
    g_MaxNumRooms = (s32)input->room_count;
    room_data_float1 = input->level_scale;
    room_data_float2 = 1.0f / input->level_scale;
    mCurrentLevelVisibilityScale = input->visibility_scale;
    ge_player_position.x = input->player_position[0];
    ge_player_position.y = input->player_position[1];
    ge_player_position.z = input->player_position[2];
    memcpy(ge_world_to_screen.m, input->world_to_screen,
           sizeof(ge_world_to_screen.m));
    ge_view_left = input->view_left;
    ge_view_top = input->view_top;
    ge_view_width = input->view_width;
    ge_view_height = input->view_height;
    ge_near_distance = input->near_distance;
    ge_far_distance = input->far_distance;
    memset(&ge_player, 0, sizeof(ge_player));
    ge_player.c_screenleft = input->view_left;
    ge_player.c_screentop = input->view_top;
    ge_player.c_halfwidth = (float)input->view_width * 0.5f;
    ge_player.c_halfheight = (float)input->view_height * 0.5f;
    tangent = tanf(half_fov_radians);
    ge_player.c_recipscaley = ge_player.c_halfheight / tangent;
    ge_player.c_recipscalex = ge_player.c_halfwidth
        / (tangent * input->aspect_ratio);
    g_CurrentPlayer = &ge_player;
    dword_CODE_bss_8007FF90 = ge_global_command_count != 0U
        ? (s32 *)ge_global_commands : NULL;
    num_visible_rooms_in_cur_global_vis_packet = 0;
    g_BgStackCount = 0;
    D_80044898 = 0;
    D_8004489C = 15;
    levelentry_index = input->level_index;
    for (room_index = 0U; room_index < portal_count; ++room_index) {
        bgOrderPortal((s32)room_index);
    }
    bgDetermineVisibleRooms();
    if (g_BgNumberOfRoomsDrawn < 0
            || g_BgNumberOfRoomsDrawn
                > (s32)GE_ORIGINAL_BG_VISIBILITY_MAX_VISIBLE) {
        return GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED;
    }
    for (output_index = 0U;
            output_index < (size_t)g_BgNumberOfRoomsDrawn; ++output_index) {
        const s_bound_info *source = &dword_CODE_bss_8007FFA0[output_index];
        GeOriginalBgVisibleRoom *destination = &result->rooms[output_index];

        if (source->roomid < 0 || (size_t)source->roomid >= input->room_count) {
            return GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND;
        }
        destination->room = (uint8_t)source->roomid;
        destination->portal_depth = (uint8_t)source->unk1;
        destination->special = (uint8_t)source->next;
        destination->screen_box[0] = source->bbox.min.x;
        destination->screen_box[1] = source->bbox.min.y;
        destination->screen_box[2] = source->bbox.max.x;
        destination->screen_box[3] = source->bbox.max.y;
    }
    result->room_count = (size_t)g_BgNumberOfRoomsDrawn;
    result->portal_count = portal_count;
    result->portal_descents = (uint32_t)D_80044898;
    result->global_command_count = ge_global_command_count;
    result->global_stream_fnv1a64 = ge_global_stream_hash;
    result->preload_request_count = ge_preload_request_count;
    result->global_visibility_used = ge_global_command_count != 0U;
    return GE_ORIGINAL_BG_VISIBILITY_OK;
}

int ge_original_bg_visibility_room_snapshot(
    uint8_t room, GeOriginalBgRoomVisibilitySnapshot *snapshot)
{
    if (snapshot == NULL || room >= MAXROOMCOUNT) return 0;
    snapshot->current_room = g_BgCurrentRoom;
    snapshot->maximum_room_count = g_MaxNumRooms;
    snapshot->rooms_drawn = g_BgNumberOfRoomsDrawn;
    snapshot->rendered = g_BgRoomInfo[room].room_rendered;
    snapshot->neighbor_to_rendered =
        g_BgRoomInfo[room].room_neighbor_to_rendered;
    snapshot->loaded_mask = g_BgRoomInfo[room].room_loaded_mask;
    return 1;
}

const char *ge_original_bg_visibility_status_name(
    GeOriginalBgVisibilityStatus status)
{
    switch (status) {
    case GE_ORIGINAL_BG_VISIBILITY_OK: return "ok";
    case GE_ORIGINAL_BG_VISIBILITY_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_BG_VISIBILITY_INVALID_BACKGROUND:
        return "invalid background";
    case GE_ORIGINAL_BG_VISIBILITY_CAPACITY_EXCEEDED:
        return "capacity exceeded";
    default: return "unknown";
    }
}

int32_t ge_original_bg_find_portal_between_rooms(
    int32_t room1, int32_t room2, const float point1[3],
    const float point2[3])
{
    coord3d first;
    coord3d second;

    if (g_BgPortals == NULL || point1 == NULL || point2 == NULL
            || room1 < 0 || room2 < 0 || room1 >= MAXROOMCOUNT
            || room2 >= MAXROOMCOUNT) return -1;
    memcpy(first.f, point1, sizeof(first.f));
    memcpy(second.f, point2, sizeof(second.f));
    return bgGetPortalBetweenRooms(room1, room2, &first, &second);
}

int32_t ge_original_bg_find_portal_on_line(
    const float point1[3], const float point2[3])
{
    coord3d first;
    coord3d second;

    if (g_BgPortals == NULL || point1 == NULL || point2 == NULL) return -1;
    memcpy(first.f, point1, sizeof(first.f));
    memcpy(second.f, point2, sizeof(second.f));
    return sub_GAME_7F0B9E04(&first, &second);
}

int ge_original_bg_set_portal_open(int32_t portal, int open,
                                   uint8_t *portal_controls,
                                   size_t portal_control_count)
{
    if (g_BgPortals == NULL || portal < 0
            || (size_t)portal >= portal_control_count
            || portal_controls == NULL) return 0;
    bgToggleDataPortalsContrlBytes1Bit1(portal, open != 0);
    portal_controls[portal] = g_BgPortals[portal].controlbytes1;
    return 1;
}
