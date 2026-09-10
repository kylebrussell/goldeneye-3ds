#include "ge_mp_state_visibility.h"
#include "ge_original_bg_visibility_state.h"
#include "ge_original_bg_visibility_internal.h"
#include <math.h>
#include <string.h>

extern Unk80081600 ge_bg_visibility_interpreter_state;
_Static_assert(MAXROOMCOUNT == 139 && PORTMAX == 200 && BG_STACK_SIZE == 20,
    "Review visibility schema capacities");

static const GeMpFieldSchema fields[] = {
    {256,GE_MP_STATE_U32,0,1,1},
    {257,GE_MP_STATE_U32,0,1,1},
    {258,GE_MP_STATE_U32,0,1,1},
    {259,GE_MP_STATE_U32,0,1,1},
    {260,GE_MP_STATE_U32,0,1,1},
    {261,GE_MP_STATE_U32,0,1,1},
    {262,GE_MP_STATE_U32,0,1,1},
    {263,GE_MP_STATE_I32,0,1,1},
    {264,GE_MP_STATE_I32,0,1,1},
    {265,GE_MP_STATE_I32,0,1,1},
    {266,GE_MP_STATE_I32,0,4,4},
    {267,GE_MP_STATE_F32,0,3,3},
    {268,GE_MP_STATE_F32,0,2,2},
    {269,GE_MP_STATE_I16,0,4,4},
    {270,GE_MP_STATE_F32,0,3,3},
    {271,GE_MP_STATE_F32,0,16,16},
    {272,GE_MP_STATE_F32,0,6,6},
    {273,GE_MP_STATE_F32,0,4,4},
    {274,GE_MP_STATE_F32,0,2,2},
    {275,GE_MP_STATE_U8,0,0,139},
    {276,GE_MP_STATE_U8,0,0,139},
    {277,GE_MP_STATE_U8,0,0,139},
    {278,GE_MP_STATE_U8,0,0,139},
    {279,GE_MP_STATE_F32,0,0,417},
    {280,GE_MP_STATE_F32,0,0,417},
    {281,GE_MP_STATE_I32,0,0,204},
    {282,GE_MP_STATE_I32,0,0,204},
    {283,GE_MP_STATE_I32,0,0,204},
    {284,GE_MP_STATE_F32,0,0,816},
    {285,GE_MP_STATE_U8,0,0,152},
    {286,GE_MP_STATE_U8,0,0,400},
    {287,GE_MP_STATE_U8,0,0,400},
    {288,GE_MP_STATE_U8,0,0,200},
    {289,GE_MP_STATE_U32,0,1,201},
    {290,GE_MP_STATE_F32,0,0,5400},
    {291,GE_MP_STATE_U8,0,0,512},
    {292,GE_MP_STATE_U8,0,0,512},
    {293,GE_MP_STATE_I32,0,0,512},
    {294,GE_MP_STATE_U64,0,1,1},
    {295,GE_MP_STATE_I32,0,20,20},
    {296,GE_MP_STATE_I32,0,1,1},
    {297,GE_MP_STATE_I32,0,1,1},
    {298,GE_MP_STATE_F32,0,4,4},
    {299,GE_MP_STATE_I32,0,1,1},
    {300,GE_MP_STATE_I32,0,1,1},
    {301,GE_MP_STATE_I32,0,1,1},
    {302,GE_MP_STATE_I32,0,2,2},
    {303,GE_MP_STATE_U8,0,0,200},
    {304,GE_MP_STATE_I32,0,0,200},
    {305,GE_MP_STATE_U16,0,0,200},
    {306,GE_MP_STATE_F32,0,0,800}
};
_Static_assert(sizeof(fields)/sizeof(fields[0]) == GE_MP_VISIBILITY_FIELDS,
    "Visibility field count");
static const GeMpRecordSchema schema = {
    GE_MP_VISIBILITY_KIND,GE_MP_SYSTEM_OBJECTS,fields,GE_MP_VISIBILITY_FIELDS};
const GeMpRecordSchema *ge_mp_state_visibility_schema(void) { return &schema; }

static void bind_fields(GeMpStateVisibilitySnapshot *snapshot)
{
    GeMpVisibilityValues *v=&snapshot->values;
    snapshot->fields[0]=(GeMpStateField){256,GE_MP_STATE_U32,1,&v->room_count};
    snapshot->fields[1]=(GeMpStateField){257,GE_MP_STATE_U32,1,&v->portal_count};
    snapshot->fields[2]=(GeMpStateField){258,GE_MP_STATE_U32,1,&v->draw_count};
    snapshot->fields[3]=(GeMpStateField){259,GE_MP_STATE_U32,1,&v->alpha_count};
    snapshot->fields[4]=(GeMpStateField){260,GE_MP_STATE_U32,1,&v->command_count};
    snapshot->fields[5]=(GeMpStateField){261,GE_MP_STATE_U32,1,&v->point_count};
    snapshot->fields[6]=(GeMpStateField){262,GE_MP_STATE_U32,1,&v->cache_count};
    snapshot->fields[7]=(GeMpStateField){263,GE_MP_STATE_I32,1,&v->level_index};
    snapshot->fields[8]=(GeMpStateField){264,GE_MP_STATE_I32,1,&v->current_room};
    snapshot->fields[9]=(GeMpStateField){265,GE_MP_STATE_I32,1,&v->room_cycle};
    snapshot->fields[10]=(GeMpStateField){266,GE_MP_STATE_I32,4,&v->view_related};
    snapshot->fields[11]=(GeMpStateField){267,GE_MP_STATE_F32,3,&v->scales};
    snapshot->fields[12]=(GeMpStateField){268,GE_MP_STATE_F32,2,&v->clip_range};
    snapshot->fields[13]=(GeMpStateField){269,GE_MP_STATE_I16,4,&v->viewport};
    snapshot->fields[14]=(GeMpStateField){270,GE_MP_STATE_F32,3,&v->player_position};
    snapshot->fields[15]=(GeMpStateField){271,GE_MP_STATE_F32,16,&v->world_to_screen};
    snapshot->fields[16]=(GeMpStateField){272,GE_MP_STATE_F32,6,&v->projection};
    snapshot->fields[17]=(GeMpStateField){273,GE_MP_STATE_F32,4,&v->screen_box};
    snapshot->fields[18]=(GeMpStateField){274,GE_MP_STATE_F32,2,&v->projection_offsets};
    snapshot->fields[19]=(GeMpStateField){275,GE_MP_STATE_U8,v->room_count,&v->room_rendered};
    snapshot->fields[20]=(GeMpStateField){276,GE_MP_STATE_U8,v->room_count,&v->room_neighbor};
    snapshot->fields[21]=(GeMpStateField){277,GE_MP_STATE_U8,v->room_count,&v->room_loaded_mask};
    snapshot->fields[22]=(GeMpStateField){278,GE_MP_STATE_U8,v->room_count,&v->room_visits};
    snapshot->fields[23]=(GeMpStateField){279,GE_MP_STATE_F32,v->room_count*3,&v->room_minima};
    snapshot->fields[24]=(GeMpStateField){280,GE_MP_STATE_F32,v->room_count*3,&v->room_maxima};
    snapshot->fields[25]=(GeMpStateField){281,GE_MP_STATE_I32,v->draw_count,&v->draw_rooms};
    snapshot->fields[26]=(GeMpStateField){282,GE_MP_STATE_I32,v->draw_count,&v->draw_depths};
    snapshot->fields[27]=(GeMpStateField){283,GE_MP_STATE_I32,v->draw_count,&v->draw_flags};
    snapshot->fields[28]=(GeMpStateField){284,GE_MP_STATE_F32,v->draw_count*4,&v->draw_boxes};
    snapshot->fields[29]=(GeMpStateField){285,GE_MP_STATE_U8,v->alpha_count,&v->alpha_order};
    snapshot->fields[30]=(GeMpStateField){286,GE_MP_STATE_U8,v->portal_count*2,&v->portal_rooms};
    snapshot->fields[31]=(GeMpStateField){287,GE_MP_STATE_U8,v->portal_count*2,&v->portal_controls};
    snapshot->fields[32]=(GeMpStateField){288,GE_MP_STATE_U8,v->portal_count,&v->authoritative_controls};
    snapshot->fields[33]=(GeMpStateField){289,GE_MP_STATE_U32,v->portal_count+1,&v->portal_point_offsets};
    snapshot->fields[34]=(GeMpStateField){290,GE_MP_STATE_F32,v->point_count*3,&v->portal_points};
    snapshot->fields[35]=(GeMpStateField){291,GE_MP_STATE_U8,v->command_count,&v->command_types};
    snapshot->fields[36]=(GeMpStateField){292,GE_MP_STATE_U8,v->command_count,&v->command_lengths};
    snapshot->fields[37]=(GeMpStateField){293,GE_MP_STATE_I32,v->command_count,&v->command_arguments};
    snapshot->fields[38]=(GeMpStateField){294,GE_MP_STATE_U64,1,&v->stream_hash};
    snapshot->fields[39]=(GeMpStateField){295,GE_MP_STATE_I32,20,&v->stack};
    snapshot->fields[40]=(GeMpStateField){296,GE_MP_STATE_I32,1,&v->stack_cursor};
    snapshot->fields[41]=(GeMpStateField){297,GE_MP_STATE_I32,1,&v->interpreter_visibility};
    snapshot->fields[42]=(GeMpStateField){298,GE_MP_STATE_F32,4,&v->interpreter_box};
    snapshot->fields[43]=(GeMpStateField){299,GE_MP_STATE_I32,1,&v->interpreter_stop};
    snapshot->fields[44]=(GeMpStateField){300,GE_MP_STATE_I32,1,&v->descent_count};
    snapshot->fields[45]=(GeMpStateField){301,GE_MP_STATE_I32,1,&v->max_depth};
    snapshot->fields[46]=(GeMpStateField){302,GE_MP_STATE_I32,2,&v->queue_cursors};
    snapshot->fields[47]=(GeMpStateField){303,GE_MP_STATE_U8,v->portal_count,&v->portal_depths};
    snapshot->fields[48]=(GeMpStateField){304,GE_MP_STATE_I32,v->portal_count,&v->cache_flags};
    snapshot->fields[49]=(GeMpStateField){305,GE_MP_STATE_U16,v->cache_count,&v->cache_portals};
    snapshot->fields[50]=(GeMpStateField){306,GE_MP_STATE_F32,v->cache_count*4,&v->cache_boxes};
}

static GeMpStateStatus incomplete(GeMpStateVisibilitySnapshot *snapshot, int reference)
{
    if (reference) ++snapshot->coverage.unresolved_references;
    else ++snapshot->coverage.unclassified_fields;
    return GE_MP_STATE_INCOMPLETE;
}

static int room_metadata_unused(const s_room_info *room)
{
    return room->model_bin_loaded == 0 && room->vertices == 0
        && room->ptr_expanded_mapping_info == 0 && room->ptr_secondary_expanded_mapping_info == 0
        && room->csize_point_index_binary == 0 && room->csize_primary_DL_binary == 0
        && room->csize_secondary_DL_binary == 0 && room->usize_point_index_binary == 0
        && room->usize_primary_DL_binary == 0 && room->usize_secondary_DL_binary == 0
        && room->cur_room_totalsize == 0 && room->vtx_batch_bounds == 0
        && room->num_vtx_batch_bounds == 0 && room->field_32 == 0
        && room->field_35 == 0 && room->field_36 == 0;
}

static int same_float_bits(const float *first, const float *second)
{
    return memcmp(first,second,sizeof(*first)) == 0;
}

GeMpStateStatus ge_mp_state_visibility_capture(GeMpStateVisibilitySnapshot *snapshot,
    const GeDamVisibilityRuntime *runtime, const uint8_t *portal_controls,
    size_t portal_control_count, const GeMpRegistry *registry, uint64_t actor_id)
{
    GeOriginalBgVisibilityStateSource source;
    if (snapshot == NULL) return GE_MP_STATE_INVALID;
    snapshot->record_count=0;
    memset(&snapshot->record,0,sizeof(snapshot->record));
    memset(&snapshot->coverage,0,sizeof(snapshot->coverage));
    if (runtime == NULL || registry == NULL || (uint32_t)(actor_id >> 32)
            != ge_mp_registry_key(GE_MP_VISIBILITY_KIND,GE_MP_VISIBILITY_NAMESPACE,GE_MP_VISIBILITY_SLOT))
        return GE_MP_STATE_INVALID;
    GeMpStateStatus status=ge_mp_registry_resolve(registry,actor_id,NULL);
    if (status != GE_MP_STATE_OK) return status;
    if (!runtime->loaded || runtime->last_original_status != GE_ORIGINAL_BG_VISIBILITY_OK
            || !ge_original_bg_visibility_state_read(runtime->program,
                runtime->background,runtime->background_size,&source))
        return incomplete(snapshot,1);
    if (source.room_count == 0 || source.room_count > MAXROOMCOUNT
            || runtime->room_count != source.room_count || source.portal_count != portal_control_count
            || (portal_control_count != 0 && portal_controls == NULL)) return incomplete(snapshot,1);
    if (g_BgCurrentRoom < 0 || g_BgCurrentRoom >= g_MaxNumRooms
            || g_BgNumberOfRoomsDrawn < 0 || g_BgNumberOfRoomsDrawn > 204
            || num_visible_rooms_in_cur_global_vis_packet < 0
            || num_visible_rooms_in_cur_global_vis_packet > 152
            || D_80044858 < 0 || D_80044858 >= 10
            || g_BgStackCount < 0 || g_BgStackCount >= BG_STACK_SIZE
            || g_BgPortalQueueReadIndex < 0 || g_BgPortalQueueReadIndex >= BG_PORTAL_QUEUE_LEN
            || g_BgPortalQueueReadIndex != g_BgPortalQueueWriteIndex
            || D_80044898 < 0 || D_8004489C < 0
            || ge_bg_visibility_interpreter_state.unk14 != 0)
        return incomplete(snapshot,0);
    if (runtime->level_index != levelentry_index
            || !same_float_bits(&runtime->level_scale,&room_data_float1)
            || !same_float_bits(&runtime->visibility_scale,&mCurrentLevelVisibilityScale)
            || !same_float_bits(&runtime->near_distance,&source.clip_range[0])
            || !same_float_bits(&runtime->far_distance,&source.clip_range[1])) return incomplete(snapshot,0);
    GeMpVisibilityValues *v=&snapshot->values;
    memset(v,0,sizeof(*v));
    v->room_count=source.room_count; v->portal_count=source.portal_count;
    v->command_count=source.command_count; v->stream_hash=source.stream_hash;
    v->draw_count=(uint32_t)g_BgNumberOfRoomsDrawn;
    v->alpha_count=(uint32_t)num_visible_rooms_in_cur_global_vis_packet;
    v->level_index=levelentry_index; v->current_room=g_BgCurrentRoom; v->room_cycle=D_80044858;
    memcpy(v->view_related,bgViewRelated,sizeof(v->view_related));
    v->scales[0]=room_data_float1; v->scales[1]=room_data_float2;
    v->scales[2]=mCurrentLevelVisibilityScale;
#define COPY(member) memcpy(v->member,source.member,sizeof(v->member))
    COPY(clip_range); COPY(viewport); COPY(player_position); COPY(world_to_screen);
    COPY(projection); COPY(screen_box); COPY(projection_offsets);
    COPY(command_types); COPY(command_lengths); COPY(command_arguments);
#undef COPY
    for (size_t room=0;room<MAXROOMCOUNT;++room) {
        const s_room_info *native=&g_BgRoomInfo[room];
        if (!room_metadata_unused(native)) return incomplete(snapshot,0);
        if (room >= source.room_count) {
            /* Every pass resets the whole publication. No unclassified room
             * can silently contribute visibility outside the authored range. */
            if (native->room_rendered || native->room_neighbor_to_rendered
                    || native->room_loaded_mask || native->portal_visit_count)
                return incomplete(snapshot,1);
            continue;
        }
        if (memcmp(native->minbounds.f,runtime->room_bounds[room].minimum,sizeof(coord3d)) != 0
                || memcmp(native->maxbounds.f,runtime->room_bounds[room].maximum,sizeof(coord3d)) != 0)
            return incomplete(snapshot,0);
        v->room_rendered[room]=native->room_rendered;
        v->room_neighbor[room]=native->room_neighbor_to_rendered;
        v->room_loaded_mask[room]=native->room_loaded_mask;
        v->room_visits[room]=native->portal_visit_count;
        memcpy(v->room_minima+room*3,native->minbounds.f,sizeof(coord3d));
        memcpy(v->room_maxima+room*3,native->maxbounds.f,sizeof(coord3d));
    }
    for (size_t i=0;i<v->draw_count;++i) {
        const s_bound_info *room=&dword_CODE_bss_8007FFA0[i];
        if (room->roomid < 0 || (uint32_t)room->roomid >= v->room_count)
            return incomplete(snapshot,1);
        v->draw_rooms[i]=room->roomid; v->draw_depths[i]=room->unk1; v->draw_flags[i]=room->next;
        memcpy(v->draw_boxes+i*4,room->bbox.f,sizeof(bbox2d));
    }
    for (size_t i=0;i<v->alpha_count;++i) {
        const uint8_t room=(uint8_t)list_visible_rooms_in_cur_global_vis_packet[i];
        if (room >= v->room_count) return incomplete(snapshot,1);
        v->alpha_order[i]=room;
    }
    for (size_t i=0;i<v->portal_count;++i) {
        const bg_portal_data_entry *portal=&g_BgPortals[i];
        if (portal->connectedRoom1 >= v->room_count || portal->connectedRoom2 >= v->room_count
                || portal->controlbytes1 != portal_controls[i]) return incomplete(snapshot,1);
        v->portal_rooms[i*2]=portal->connectedRoom1; v->portal_rooms[i*2+1]=portal->connectedRoom2;
        v->portal_controls[i*2]=portal->controlbytes1; v->portal_controls[i*2+1]=portal->controlbytes2;
        v->authoritative_controls[i]=portal_controls[i];
        v->portal_point_offsets[i]=v->point_count;
        const bg_portal_entry *geometry=portal->offset_portal;
        /* The bridge has validated this precise owned polygon address and
         * its 3..9 point count. Copy coordinates by fields, never pointer bits. */
        const uint8_t *points=(const uint8_t *)geometry+offsetof(bg_portal_entry,point);
        for (size_t point=0;point<geometry->numPoints;++point) {
            memcpy(v->portal_points+v->point_count*3,points+point*sizeof(coord3d),sizeof(coord3d));
            ++v->point_count;
        }
        v->portal_depths[i]=D_800442FC[i];
        v->cache_flags[i]=table_for_portals[i].count;
        if (v->cache_flags[i] < -1 || v->cache_flags[i] > 19) return incomplete(snapshot,0);
        if (v->cache_flags[i] >= 0) {
            v->cache_portals[v->cache_count]=(uint16_t)i;
            memcpy(v->cache_boxes+v->cache_count*4,table_for_portals[i].bbox.f,sizeof(bbox2d));
            ++v->cache_count;
        }
    }
    v->portal_point_offsets[v->portal_count]=v->point_count;
    memcpy(v->stack,g_BgStack,sizeof(v->stack)); v->stack_cursor=g_BgStackCount;
    v->interpreter_visibility=current_visibility;
    memcpy(v->interpreter_box,ge_bg_visibility_interpreter_state.unk0.f,sizeof(bbox2d));
    v->interpreter_stop=ge_bg_visibility_interpreter_state.unk10;
    v->descent_count=D_80044898; v->max_depth=D_8004489C;
    v->queue_cursors[0]=g_BgPortalQueueReadIndex; v->queue_cursors[1]=g_BgPortalQueueWriteIndex;
    bind_fields(snapshot);
    for (size_t i=0;i<GE_MP_VISIBILITY_FIELDS;++i) {
        const GeMpStateField *field=&snapshot->fields[i];
        if (field->type != GE_MP_STATE_F32) continue;
        const float *values=field->values;
        for (size_t j=0;j<field->count;++j)
            if (!isfinite(values[j])) return incomplete(snapshot,0);
    }
    snapshot->record=(GeMpStateRecord){actor_id,GE_MP_VISIBILITY_KIND,snapshot->fields,GE_MP_VISIBILITY_FIELDS};
    snapshot->record_count=1;
    snapshot->coverage.enumerated_systems=GE_MP_SYSTEM_OBJECTS;
    return GE_MP_STATE_OK;
}
