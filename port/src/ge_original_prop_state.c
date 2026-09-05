#include "ge_original_prop_state.h"

#include <stdint.h>
#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondtypes.h>
#include "game/chrai.h"
#include "ge_original_dam_world.h"

#define GE_ORIGINAL_PROP_ROOM_CAPACITY 256U

extern PropRecord g_Props[MAX_PROPS];
extern s16 *RoomPropListBlockIndices;
extern struct roomproplistblock *RoomPropListBlocks;
extern PropRecord *g_ActivePropsTail;
extern PropRecord *g_ActivePropsHead;
extern PropRecord *g_FreeProps;

PropRecord *chrpropAllocate(void);
void chrpropActivate(PropRecord *prop);
void chrpropEnable(PropRecord *prop);
void chrpropRegisterRoom(PropRecord *prop, s16 room);
void chrpropDeregisterRooms(PropRecord *prop);
void chrpropDelist(PropRecord *prop);
void chrpropDisable(PropRecord *prop);
void chrpropFree(PropRecord *prop);
ObjectRecord *ge_original_room_object_at_position_slice(
    coord3d *position, s32 room, f32 *top, f32 *bottom);
void mtx4TransformVecInPlace(Mtxf *matrix, coord3d *vector);
void matrix_4x4_copy(Mtxf *src, Mtxf *dst);
void matrix_4x4_set_position(coord3d *position, Mtxf *matrix);
void matrix_4x4_multiply_homogeneous(Mtxf *lhs, Mtxf *rhs, Mtxf *dst);

s32 g_MaxNumRooms;

static s16 ge_room_prop_list_block_indices[GE_ORIGINAL_PROP_ROOM_CAPACITY];
static struct roomproplistblock ge_room_prop_list_blocks[BSS_8007161C_LEN];
static GeOriginalPropState *ge_prop_state;

static int ge_prop_pointer_in_pool(const void *opaque_prop)
{
    const uintptr_t address = (uintptr_t)opaque_prop;
    const uintptr_t first = (uintptr_t)&g_Props[0];
    const uintptr_t end = (uintptr_t)&g_Props[MAX_PROPS];

    return address >= first && address < end
        && (address - first) % sizeof(g_Props[0]) == 0U;
}

int ge_original_prop_state_reset(GeOriginalPropState *state,
                                 uint32_t room_capacity)
{
    uint32_t i;
    uint32_t j;

    if (state == NULL || room_capacity == 0U
            || room_capacity > GE_ORIGINAL_PROP_ROOM_CAPACITY) {
        return 0;
    }

    memset(state, 0, sizeof(*state));
    state->room_capacity = room_capacity;
    ge_prop_state = state;
    g_MaxNumRooms = (s32)room_capacity;

    memset(g_Props, 0, sizeof(g_Props));
    g_ActivePropsTail = NULL;
    g_ActivePropsHead = NULL;
    g_FreeProps = g_Props;
    for (i = 0; i < MAX_PROPS - 1U; i++) {
        g_Props[i].prev = &g_Props[i + 1U];
    }

    RoomPropListBlockIndices = ge_room_prop_list_block_indices;
    RoomPropListBlocks = ge_room_prop_list_blocks;
    for (i = 0; i < GE_ORIGINAL_PROP_ROOM_CAPACITY; i++) {
        RoomPropListBlockIndices[i] = -1;
    }
    for (i = 0; i < BSS_8007161C_LEN; i++) {
        RoomPropListBlocks[i].propnums[0] = -2;
        for (j = 1; j < BSS_8007161C_DATA_LEN; j++) {
            RoomPropListBlocks[i].propnums[j] = -1;
        }
    }

    return 1;
}

void *ge_original_prop_state_allocate(void *context, void *definition)
{
    GeOriginalPropState *state = context;
    PropRecord *prop;

    if (state == NULL || state != ge_prop_state || definition == NULL) {
        return NULL;
    }
    prop = chrpropAllocate();
    if (prop != NULL) {
        state->allocation_calls++;
    }
    return prop;
}

int ge_original_prop_state_bind_object(void *definition, void *opaque_prop)
{
    ObjectRecord *object = definition;
    PropRecord *prop = opaque_prop;
    if (object == NULL || prop == NULL) return 0;
    object->prop = prop;
    return 1;
}

int ge_original_prop_state_set_primary_room(void *opaque_prop, int16_t room)
{
    PropRecord *prop = opaque_prop;
    if (!ge_prop_pointer_in_pool(prop) || room < 0 || room > UINT8_MAX - 1)
        return 0;
    prop->rooms[0] = (uint8_t)room;
    prop->rooms[1] = UINT8_MAX;
    prop->rooms[2] = UINT8_MAX;
    return 1;
}

void *ge_original_prop_state_allocate_player(void *context)
{
    GeOriginalPropState *state = context;
    PropRecord *prop;

    if (state == NULL || state != ge_prop_state) {
        return NULL;
    }
    prop = chrpropAllocate();
    if (prop != NULL) {
        state->allocation_calls++;
    }
    return prop;
}

void ge_original_prop_state_activate(void *context, void *opaque_prop)
{
    GeOriginalPropState *state = context;

    if (state == ge_prop_state && ge_prop_pointer_in_pool(opaque_prop)) {
        chrpropActivate((PropRecord *)opaque_prop);
        state->activation_calls++;
    }
}

void ge_original_prop_state_enable(void *context, void *opaque_prop)
{
    GeOriginalPropState *state = context;

    if (state == ge_prop_state && ge_prop_pointer_in_pool(opaque_prop)) {
        chrpropEnable((PropRecord *)opaque_prop);
        state->enable_calls++;
    }
}

void ge_original_prop_state_register_room(void *context, void *opaque_prop,
                                          int16_t room)
{
    GeOriginalPropState *state = context;

    if (state == ge_prop_state && ge_prop_pointer_in_pool(opaque_prop)
            && room >= 0 && (uint32_t)room < state->room_capacity) {
        chrpropRegisterRoom((PropRecord *)opaque_prop, (s16)room);
        state->room_registration_calls++;
    }
}

int ge_original_prop_state_release(void *context, void *opaque_prop)
{
    GeOriginalPropState *state = context;
    PropRecord *prop = opaque_prop;
    if (state != ge_prop_state || !ge_prop_pointer_in_pool(prop)
            || !ge_original_prop_state_is_active(prop)) return 0;
    chrpropDeregisterRooms(prop);
    chrpropDelist(prop);
    chrpropDisable(prop);
    chrpropFree(prop);
    return 1;
}

size_t ge_original_prop_state_native_prop_size(void)
{
    return sizeof(PropRecord);
}

uint32_t ge_original_prop_state_active_count(void)
{
    const PropRecord *prop = g_ActivePropsTail;
    uint32_t count = 0U;

    while (prop != NULL && count < MAX_PROPS) {
        count++;
        prop = prop->prev;
    }
    return count;
}

int ge_original_prop_state_is_active(const void *opaque_prop)
{
    const PropRecord *prop = g_ActivePropsTail;

    if (!ge_prop_pointer_in_pool(opaque_prop)) {
        return 0;
    }
    while (prop != NULL) {
        if (prop == opaque_prop) {
            return 1;
        }
        prop = prop->prev;
    }
    return 0;
}

int ge_original_prop_state_snapshot_active(GeOriginalPropActiveSet *set)
{
    _Static_assert(sizeof(set->active) == MAX_PROPS, "active-set pool size");
    const PropRecord *prop = g_ActivePropsTail;
    size_t visited = 0U;
    if (set == NULL) return 0;
    memset(set, 0, sizeof(*set));
    while (prop != NULL) {
        if (!ge_prop_pointer_in_pool(prop) || visited++ >= MAX_PROPS) return 0;
        set->active[prop - g_Props] = 1U;
        prop = prop->prev;
    }
    return 1;
}

int ge_original_prop_state_active_set_contains(
    const GeOriginalPropActiveSet *set, const void *opaque_prop)
{
    if (set == NULL) return ge_original_prop_state_is_active(opaque_prop);
    if (!ge_prop_pointer_in_pool(opaque_prop)) return 0;
    return set->active[(const PropRecord *)opaque_prop - g_Props] != 0U;
}

int ge_original_prop_state_is_enabled(const void *opaque_prop)
{
    const PropRecord *prop = opaque_prop;

    return ge_prop_pointer_in_pool(opaque_prop)
        && (prop->flags & PROPFLAG_ENABLED) != 0U;
}

int ge_original_prop_state_room_contains(int16_t room, const void *opaque_prop)
{
    s16 block;
    s16 prop_index;

    if (ge_prop_state == NULL || !ge_prop_pointer_in_pool(opaque_prop)
            || room < 0 || (uint32_t)room >= ge_prop_state->room_capacity) {
        return 0;
    }
    prop_index = (s16)((const PropRecord *)opaque_prop - g_Props);
    block = RoomPropListBlockIndices[room];
    while (block >= 0) {
        uint32_t i;
        for (i = 0; i < 15U; i++) {
            if (RoomPropListBlocks[block].propnums[i] == prop_index) {
                return 1;
            }
        }
        block = RoomPropListBlocks[block].propnums[15];
    }
    return 0;
}

int ge_original_prop_state_object_scene_transform(
    const void *opaque_definition, const void *opaque_prop,
    float matrix[4][4], float position[3], uint8_t *room)
{
    const ObjectRecord *definition = opaque_definition;
    const PropRecord *prop = opaque_prop;

    if (definition == NULL || !ge_prop_pointer_in_pool(prop)
            || prop->obj != definition || matrix == NULL
            || position == NULL || room == NULL || prop->rooms[0] == UINT8_MAX)
        return 0;
    memcpy(matrix, definition->mtx.m, sizeof(definition->mtx.m));
    memcpy(position, definition->runtime_pos.f,
           sizeof(definition->runtime_pos.f));
    *room = prop->rooms[0];
    return 1;
}

int ge_original_prop_state_object_scene_matrix_bank(
    const void *opaque_definition, const void *opaque_prop,
    const float (**matrices)[4][4], size_t *matrix_count)
{
    const ObjectRecord *definition = opaque_definition;
    const PropRecord *prop = opaque_prop;
    const Model *model;

    if (matrices == NULL || matrix_count == NULL) return 0;
    *matrices = NULL;
    *matrix_count = 0U;
    if (definition == NULL || !ge_prop_pointer_in_pool(prop)
            || prop->obj != definition
            || (prop->flags & PROPFLAG_ONSCREEN) == 0U
            || (model = definition->model) == NULL || model->obj == NULL
            || model->render_pos == NULL || model->obj->numMatrices <= 0)
        return 0;

    _Static_assert(sizeof(RenderPosView) == sizeof(float[4][4]),
                   "RenderPosView must remain one native matrix");
    *matrices = (const float (*)[4][4])(const void *)model->render_pos;
    *matrix_count = (size_t)model->obj->numMatrices;
    return 1;
}

int ge_original_prop_state_publish_scene_visibility(
    void *opaque_prop, int visible, const float world_to_view[4][4])
{
    return ge_original_prop_state_publish_scene_visibility_with_active_set(
        opaque_prop, visible, world_to_view, NULL);
}

int ge_original_prop_state_publish_scene_visibility_with_active_set(
    void *opaque_prop, int visible, const float world_to_view[4][4],
    const GeOriginalPropActiveSet *active)
{
    PropRecord *prop = opaque_prop;
    ObjectRecord *object;
    coord3d view_position;
    Mtxf view;

    if (!ge_prop_pointer_in_pool(prop) || world_to_view == NULL) return 0;
    prop->flags &= (u8)~PROPFLAG_ONSCREEN;
    if (!visible || !ge_original_prop_state_active_set_contains(active, prop)
            || (prop->flags & PROPFLAG_ENABLED) == 0U)
        return 1;
    view_position = prop->pos;
    memcpy(view.m, world_to_view, sizeof(view.m));
    mtx4TransformVecInPlace(&view, &view_position);
    if (!isfinite(view_position.z)) return 0;
    prop->zDepth = -view_position.z;

    /* This is the unchanged ordinary-object matrix publication order from
     * objTick: copy the authored orientation, replace its translation with
     * runtime_pos, then multiply world-to-view by that object matrix.  PP7
     * object hits and projectile attachment both
     * consume Model.render_pos; publishing only PropRecord.zDepth made the
     * authored objects visible but left collision rays at zero/stale model
     * matrices.  Doors keep their separate door7F0526EC dynamic matrix path. */
    if ((prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_WEAPON)
            && prop->obj != NULL) {
        Mtxf object_matrix;
        uint8_t object_type;
        object = prop->obj;
        if (!ge_dam_setup_world_definition_header(
                object, NULL, NULL, &object_type)) {
            object_type = ((PropDefHeaderRecord *)object)->type;
        }
        if (object_type != PROPDEF_DOOR
                && object->model != NULL
                && object->model->obj != NULL
                && object->model->render_pos != NULL
                && object->model->obj->numMatrices > 0) {
            matrix_4x4_copy(&object->mtx, &object_matrix);
            matrix_4x4_set_position(&object->runtime_pos, &object_matrix);
            matrix_4x4_multiply_homogeneous(
                &view, &object_matrix, &object->model->render_pos[0].pos);
            prop->zDepth = -object->model->render_pos[0].pos.m[3][2];
        }
    }
    prop->flags |= PROPFLAG_ONSCREEN;
    return 1;
}

int ge_original_prop_state_observe_character_scene_state(
    const void *opaque_prop, GeOriginalCharacterSceneState *state)
{
    const PropRecord *prop = opaque_prop;

    if (!ge_prop_pointer_in_pool(prop) || state == NULL
            || (prop->type != PROP_TYPE_CHR
                && prop->type != PROP_TYPE_VIEWER)) return 0;
    state->flags = prop->flags;
    state->zdepth = prop->zDepth;
    return 1;
}

void *ge_original_prop_state_room_object_at_position(
    const float position[3], int16_t room, float *top, float *bottom)
{
    coord3d native_position;

    if (position == NULL || top == NULL || bottom == NULL || room < 0)
        return NULL;
    memcpy(native_position.f, position, sizeof(native_position.f));
    return ge_original_room_object_at_position_slice(
        &native_position, room, top, bottom);
}
