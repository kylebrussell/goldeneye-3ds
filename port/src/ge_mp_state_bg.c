#include "ge_mp_state_bg.h"
#include "ge_original_bg_geometry_internal.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(GE_MP_BG_FIXTURES == GE_BG_LIGHT_FIXTURES
    && GE_MP_BG_DARKENED == GE_BG_DARKENED_VERTICES
    && GE_MP_BG_ROOMS == GE_BG_GEOMETRY_MAX_ROOMS, "Review BG state capacities");
_Static_assert(sizeof(Vtx) == 16 && sizeof(struct s_darkened_light) == 4,
    "Review original light representation");

#define F(id,type,min,max) {id,GE_MP_STATE_##type,0,min,max}
static const GeMpFieldSchema fields[] = {
    F(256,U32,1,1), F(257,U32,1,1), F(258,U32,1,1), F(259,F32,2,2),
    F(260,I16,1,1), F(261,I16,1,1), F(262,I32,1,1),
    F(263,I16,100,100), F(264,U8,100,100), F(265,U32,100,100), F(266,U32,100,100),
    F(267,U16,512,512), F(268,U16,512,512),
    F(269,U32,3,GE_MP_BG_ROOMS+1), F(270,U32,2,GE_MP_BG_ROOMS),
    F(271,U32,2,GE_MP_BG_ROOMS), F(272,U32,2,GE_MP_BG_ROOMS),
    F(273,F32,6,GE_MP_BG_ROOMS*3), F(274,F32,6,GE_MP_BG_ROOMS*3),
    F(275,F32,6,GE_MP_BG_ROOMS*3), F(276,U32,0,GE_MP_STATE_MAX_VALUES)
};
#undef F
_Static_assert(sizeof(fields)/sizeof(fields[0]) == GE_MP_BG_FIELDS, "BG schema field count");
static const GeMpRecordSchema schema = {GE_MP_BG_KIND,GE_MP_SYSTEM_OBJECTS,fields,GE_MP_BG_FIELDS};
const GeMpRecordSchema *ge_mp_state_bg_schema(void) { return &schema; }

static GeMpStateStatus geometry_ready(const GeOriginalBgGeometry *geometry, size_t *points)
{
    size_t count=0;
    if (geometry == NULL || geometry != ge_bg_geometry_current) return GE_MP_STATE_STALE;
    if (geometry->status != GE_ORIGINAL_BG_GEOMETRY_OK || geometry->room_count < 2
            || geometry->room_count > GE_MP_BG_ROOMS) return GE_MP_STATE_INCOMPLETE;
    for (size_t room=0;room<geometry->room_count;++room) {
        size_t n=geometry->room_geometry[room].vertex_count;
        if (n > UINT16_MAX || n > GE_MP_STATE_MAX_VALUES-count) return GE_MP_STATE_CAPACITY;
        count+=n;
    }
    *points=count;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_state_bg_init(GeMpStateBgSnapshot *snapshot, const GeOriginalBgGeometry *geometry)
{
    size_t points;
    GeMpStateStatus status;
    if (snapshot == NULL || snapshot->initialized || snapshot->rgba != NULL) return GE_MP_STATE_INVALID;
    status=geometry_ready(geometry,&points);
    if (status != GE_MP_STATE_OK) return status;
    uint32_t *rgba=points != 0 ? calloc(points,sizeof(*rgba)) : NULL;
    if (points != 0 && rgba == NULL) return GE_MP_STATE_CAPACITY;
    memset(snapshot,0,sizeof(*snapshot));
    snapshot->rgba=rgba; snapshot->rgba_capacity=points; snapshot->initialized=1;
    return GE_MP_STATE_OK;
}

void ge_mp_state_bg_close(GeMpStateBgSnapshot *snapshot)
{
    if (snapshot == NULL) return;
    free(snapshot->rgba);
    memset(snapshot,0,sizeof(*snapshot));
}

static int command_offset(const uint8_t *base, size_t count, const Gfx *command,
    int allow_end, uint32_t *offset)
{
    uintptr_t start=(uintptr_t)base,address=(uintptr_t)command;
    if (base == NULL || command == NULL || address < start || (address-start)%8 != 0
            || address-start > count*8 || (!allow_end && address-start == count*8)) return 0;
    *offset=(uint32_t)((address-start)/8);
    return 1;
}

static int fixture_range(const GeOriginalBgGeometry *geometry, const s_lightfixture *fixture,
    uint8_t *list, uint32_t *start, uint32_t *end)
{
    const GeOriginalBgRoomGeometry *room=&geometry->room_geometry[fixture->room_index];
    for (unsigned i=0;i<2;++i) {
        const uint8_t *base=i==0?room->blobs.primary_gdl:room->blobs.secondary_gdl;
        if (command_offset(base,room->command_counts[i],fixture->ptr_start_pertinent_DL,1,start)
                && command_offset(base,room->command_counts[i],fixture->ptr_end_pertinent_DL,1,end)
                && *start <= *end) { *list=(uint8_t)i; return 1; }
    }
    return 0;
}

static GeMpStateStatus incomplete(GeMpStateBgSnapshot *snapshot, int reference)
{
    if (reference) ++snapshot->coverage.unresolved_references;
    else ++snapshot->coverage.unclassified_fields;
    return GE_MP_STATE_INCOMPLETE;
}

GeMpStateStatus ge_mp_state_bg_capture(GeMpStateBgSnapshot *snapshot,
    const GeOriginalBgGeometry *geometry, const GeMpRegistry *registry, uint64_t actor_id)
{
    size_t points, cursor=0;
    GeMpStateStatus status;
    if (snapshot == NULL) return GE_MP_STATE_INVALID;
    snapshot->record_count=0;
    memset(&snapshot->record,0,sizeof(snapshot->record));
    memset(&snapshot->coverage,0,sizeof(snapshot->coverage));
    if (!snapshot->initialized || (snapshot->rgba_capacity != 0 && snapshot->rgba == NULL)
            || registry == NULL || (uint32_t)(actor_id >> 32)
                != ge_mp_registry_key(GE_MP_BG_KIND,GE_MP_BG_NAMESPACE,GE_MP_BG_SLOT))
        return GE_MP_STATE_INVALID;
    status=ge_mp_registry_resolve(registry,actor_id,NULL);
    if (status != GE_MP_STATE_OK) return status;
    status=geometry_ready(geometry,&points);
    if (status != GE_MP_STATE_OK) return status;
    if (points > snapshot->rgba_capacity) return GE_MP_STATE_CAPACITY;
    if (!isfinite(geometry->level_scale) || !isfinite(geometry->inverse_scale)
            || geometry->level_scale <= 0 || geometry->inverse_scale <= 0
            || geometry->fixture_slot < 0 || geometry->fixture_slot >= (s32)GE_MP_BG_FIXTURES
            || geometry->fixture_room <= 0 || (size_t)geometry->fixture_room >= geometry->room_count
            || geometry->darkened_cursor < 0 || geometry->darkened_cursor >= (s32)GE_MP_BG_DARKENED)
        return incomplete(snapshot,0);
    GeMpBgValues *values=&snapshot->values;
    memset(values,0,sizeof(*values));
    values->status=(uint32_t)geometry->status;
    values->room_count=(uint32_t)geometry->room_count;
    values->point_count=(uint32_t)points;
    values->scales[0]=geometry->level_scale; values->scales[1]=geometry->inverse_scale;
    values->fixture_slot=geometry->fixture_slot; values->fixture_room=geometry->fixture_room;
    values->darkened_cursor=geometry->darkened_cursor;
    for (size_t i=0;i<GE_MP_BG_FIXTURES;++i) {
        const s_lightfixture *fixture=&geometry->fixtures[i];
        values->fixture_rooms[i]=fixture->room_index;
        values->fixture_lists[i]=UINT8_MAX;
        values->fixture_starts[i]=values->fixture_ends[i]=UINT32_MAX;
        if (fixture->room_index == 0) {
            if (fixture->ptr_start_pertinent_DL != NULL || fixture->ptr_end_pertinent_DL != NULL)
                return incomplete(snapshot,1);
        } else if (fixture->room_index < 0 || (size_t)fixture->room_index >= geometry->room_count
                || !fixture_range(geometry,fixture,&values->fixture_lists[i],
                    &values->fixture_starts[i],&values->fixture_ends[i])) return incomplete(snapshot,1);
    }
    for (size_t i=0;i<GE_MP_BG_DARKENED;++i) {
        uint16_t room=geometry->darkened[i].room_index, point=geometry->darkened[i].vtx_index;
        if ((size_t)room >= geometry->room_count
                || (room != 0 && (size_t)point >= geometry->room_geometry[room].vertex_count))
            return incomplete(snapshot,1);
        values->darkened_rooms[i]=room; values->darkened_points[i]=point;
    }
    for (size_t id=0;id<geometry->room_count;++id) {
        const GeOriginalBgRoomGeometry *room=&geometry->room_geometry[id];
        const s_room_info *native=&geometry->rooms[id];
        if (room->command_counts[0] > INT16_MAX || room->command_counts[1] > INT16_MAX
                || native->num_vtx_batch_bounds < 0
                || (room->vertex_count != 0 && (native->vertices == NULL
                    || room->blobs.point_table == NULL || room->collision_vertices == NULL)))
            return incomplete(snapshot,0);
        values->point_offsets[id]=(uint32_t)cursor;
        values->primary_commands[id]=(uint32_t)room->command_counts[0];
        values->secondary_commands[id]=(uint32_t)room->command_counts[1];
        values->vertex_batches[id]=(uint32_t)native->num_vtx_batch_bounds;
        for (size_t axis=0;axis<3;++axis) {
            if (!isfinite(geometry->origins[id].f[axis]) || !isfinite(native->minbounds.f[axis])
                    || !isfinite(native->maxbounds.f[axis])) return incomplete(snapshot,0);
            values->origins[id*3+axis]=geometry->origins[id].f[axis];
            values->minima[id*3+axis]=native->minbounds.f[axis];
            values->maxima[id*3+axis]=native->maxbounds.f[axis];
        }
        for (size_t point=0;point<room->vertex_count;++point) {
            const uint8_t *colour=native->vertices[point].v.cn;
            if (memcmp(colour,room->blobs.point_table+point*16+12,4) != 0
                    || memcmp(colour,&room->collision_vertices[point].r,4) != 0)
                return incomplete(snapshot,0);
            snapshot->rgba[cursor++]=((uint32_t)colour[0]<<24)|((uint32_t)colour[1]<<16)
                |((uint32_t)colour[2]<<8)|colour[3];
        }
    }
    values->point_offsets[geometry->room_count]=(uint32_t)cursor;
    size_t field=0;
#define VALUE(type,member,count) do { snapshot->fields[field]=(GeMpStateField){ \
    (uint16_t)(256+field),GE_MP_STATE_##type,(uint32_t)(count),&(member)}; ++field; } while (0)
    VALUE(U32,values->status,1); VALUE(U32,values->room_count,1); VALUE(U32,values->point_count,1);
    VALUE(F32,values->scales,2); VALUE(I16,values->fixture_slot,1); VALUE(I16,values->fixture_room,1);
    VALUE(I32,values->darkened_cursor,1); VALUE(I16,values->fixture_rooms,100);
    VALUE(U8,values->fixture_lists,100); VALUE(U32,values->fixture_starts,100); VALUE(U32,values->fixture_ends,100);
    VALUE(U16,values->darkened_rooms,512); VALUE(U16,values->darkened_points,512);
    VALUE(U32,values->point_offsets,geometry->room_count+1); VALUE(U32,values->primary_commands,geometry->room_count);
    VALUE(U32,values->secondary_commands,geometry->room_count); VALUE(U32,values->vertex_batches,geometry->room_count);
    VALUE(F32,values->origins,geometry->room_count*3); VALUE(F32,values->minima,geometry->room_count*3);
    VALUE(F32,values->maxima,geometry->room_count*3);
#undef VALUE
    snapshot->fields[field]=(GeMpStateField){276,GE_MP_STATE_U32,(uint32_t)points,snapshot->rgba};
    snapshot->record=(GeMpStateRecord){actor_id,GE_MP_BG_KIND,snapshot->fields,GE_MP_BG_FIELDS};
    snapshot->coverage.enumerated_systems=GE_MP_SYSTEM_OBJECTS;
    snapshot->record_count=1;
    return GE_MP_STATE_OK;
}
