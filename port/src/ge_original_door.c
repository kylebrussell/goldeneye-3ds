#include "ge_original_door_internal.h"
#include "ge_original_door_runtime_internal.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static GeOriginalDoorProviders providers;
static GeOriginalDoorPrepared *published;
static GeOriginalDoorRuntimeProviders runtime_providers;
static GeOriginalDoorRuntimeState *runtime_state;
typedef struct GeOriginalDoorNativeSlot {
    ObjectRecord *definition;
    DoorRecord *runtime;
#if !defined(GE_PORT_MS_INHERITS)
    /* Some host-only legacy tests intentionally compile without the anonymous
     * inheritance ABI.  They cannot alias a DoorRecord over ObjectRecord and
     * retain the old isolated storage solely in that configuration. */
    DoorRecord runtime_storage;
#endif
    Vertex *clipped_vertices;
    uint32_t clipped_vertex_capacity;
    uint32_t clipped_vertex_count;
    uint32_t publication_generation;
    uint32_t last_open_position_bits;
    uint64_t clipped_vertex_hash;
    uint8_t open_position_published;
    uint8_t clipped_vertex_published;
} GeOriginalDoorNativeSlot;
static GeOriginalDoorNativeSlot
    native_doors[GE_ORIGINAL_DOOR_NATIVE_CAPACITY];

#if defined(GE_PORT_MS_INHERITS)
_Static_assert(offsetof(DoorRecord, linkedDoorOffset) == sizeof(ObjectRecord),
               "DoorRecord tail must immediately follow ObjectRecord");
#endif

static GeOriginalDoorNativeSlot *find_native_slot_by_runtime(DoorRecord *door)
{
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_DOOR_NATIVE_CAPACITY; index++)
        if (native_doors[index].runtime == door) return &native_doors[index];
    return NULL;
}

static DoorRecord *find_native_door(void *definition)
{
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_DOOR_NATIVE_CAPACITY; index++)
        if (native_doors[index].definition == definition)
            return native_doors[index].runtime;
    return NULL;
}

static DoorRecord *install_native_door(
    ObjectRecord *object, const GeOriginalDamDoorSetup *setup,
    const GeOriginalDoorPrepared *prepared)
{
    GeOriginalDoorNativeSlot *slot = NULL;
    ModelRoData_BoundingBoxRecord *bbox;
#if defined(GE_PORT_MS_INHERITS)
    s32 authored_tint_dist;
    s16 authored_cull_dist;
    s8 authored_sound_type;
    s8 authored_fade_time;
#endif
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_DOOR_NATIVE_CAPACITY; index++) {
        if (native_doors[index].definition == object
                || native_doors[index].definition == NULL) {
            slot = &native_doors[index];
            break;
        }
    }
    if (slot == NULL) return NULL;
    slot->definition = object;
    /* The authored allocation is already a full DoorRecord on the live ABI.
     * Keep the exact
     * objTick/prop->door ABI by making that record the sole live runtime
     * owner; a parallel tail record makes canonical objTick pass a pointer
     * that the door services cannot resolve. */
#if defined(GE_PORT_MS_INHERITS)
    slot->runtime = (DoorRecord *)object;
    authored_tint_dist = slot->runtime->TintDist;
    authored_cull_dist = slot->runtime->CullDist;
    authored_sound_type = slot->runtime->soundType;
    authored_fade_time = slot->runtime->fadeTime60;
    memset((unsigned char *)slot->runtime
               + offsetof(DoorRecord, linkedDoorOffset),
           0, sizeof(*slot->runtime)
               - offsetof(DoorRecord, linkedDoorOffset));
#else
    slot->runtime = &slot->runtime_storage;
    memset(slot->runtime, 0, sizeof(*slot->runtime));
#endif
    slot->runtime->linkedDoorOffset = setup->linked_door_offset;
    slot->runtime->maxFrac = prepared->max_frac;
    slot->runtime->perimFrac = prepared->perim_frac;
    slot->runtime->accel = prepared->accel;
    slot->runtime->decel = prepared->decel;
    slot->runtime->maxSpeed = prepared->max_speed;
    slot->runtime->doorFlags = prepared->door_flags;
    slot->runtime->doorType = prepared->door_type;
    slot->runtime->keyflags = setup->key_flags;
    slot->runtime->autoCloseFrames = setup->auto_close_frames;
    slot->runtime->doorOpenSound = setup->door_open_sound;
    slot->runtime->frac = prepared->travel[0];
    slot->runtime->unkac = prepared->travel[1];
    slot->runtime->unkb0 = prepared->travel[2];
    slot->runtime->openPosition = prepared->open_position;
    slot->runtime->openstate = DOORSTATE_STATIONARY;
    slot->runtime->portalNumber = prepared->portal_number;
#if defined(GE_PORT_MS_INHERITS)
    /* These visual/audio distance fields are authored in setup words 48/49;
     * unlike the intervening motion state they are not runtime scratch. */
    slot->runtime->TintDist = authored_tint_dist;
    slot->runtime->CullDist = authored_cull_dist;
    slot->runtime->soundType = authored_sound_type;
    slot->runtime->fadeTime60 = authored_fade_time;
#endif
    bbox = (ModelRoData_BoundingBoxRecord *)
        object->model->obj->RootNode->Child->Data;
    slot->runtime->bbox = *bbox;
    if (slot->runtime->doorFlags & DOORFLAG_CLIP_TO_BBOX) {
        if (slot->runtime->doorType == DOORTYPE_VERTICAL)
            slot->runtime->bbox.Bounds.ymax = bbox->Bounds.ymax
                + (bbox->Bounds.ymin - bbox->Bounds.ymax)
                    * slot->runtime->openPosition;
        else slot->runtime->bbox.Bounds.xmin = bbox->Bounds.xmin
                + (bbox->Bounds.xmax - bbox->Bounds.xmin)
                    * slot->runtime->openPosition;
    }
    return slot->runtime;
}

void ge_original_door_bind(const GeOriginalDoorProviders *p,
                           GeOriginalDoorPrepared *out)
{
    size_t index;
    for (index = 0U; index < GE_ORIGINAL_DOOR_NATIVE_CAPACITY; index++)
        free(native_doors[index].clipped_vertices);
    memset(&providers,0,sizeof(providers)); if(p)providers=*p;
    memset(native_doors,0,sizeof(native_doors));
    published=out; if(out){memset(out,0,sizeof(*out));out->pad_id=-1;out->portal_number=-1;}
}

static int ge_original_door_native_setup(
    ObjectRecord *object, GeOriginalDamDoorSetup *setup)
{
#if defined(GE_PORT_MS_INHERITS)
    const PropDefHeaderRecord *header = (const void *)object;
    const DoorRecord *door = (const void *)object;
    if (header == NULL || setup == NULL || header->type != PROPDEF_DOOR)
        return 0;
    memset(setup, 0, sizeof(*setup));
    setup->linked_door_offset = door->linkedDoorOffset;
    memcpy(&setup->max_frac_fixed, &door->maxFrac,
           sizeof(setup->max_frac_fixed));
    memcpy(&setup->perim_frac_fixed, &door->perimFrac,
           sizeof(setup->perim_frac_fixed));
    memcpy(&setup->accel_fixed, &door->accel,
           sizeof(setup->accel_fixed));
    memcpy(&setup->decel_fixed, &door->decel,
           sizeof(setup->decel_fixed));
    memcpy(&setup->max_speed_fixed, &door->maxSpeed,
           sizeof(setup->max_speed_fixed));
    setup->door_flags = door->doorFlags;
    setup->door_type = door->doorType;
    setup->key_flags = door->keyflags;
    setup->auto_close_frames = door->autoCloseFrames;
    setup->door_open_sound = door->doorOpenSound;
    return 1;
#else
    (void)object;
    (void)setup;
    return 0;
#endif
}

GeOriginalDoorStatus ge_original_door_construct(void *definition,
                                                 int32_t command_index)
{
    ObjectRecord *object=definition; GeOriginalDamDoorSetup setup;
    ModelFileHeader *header=NULL; Model *model=NULL; float scale=0.0f;
    void *collision;
    if(!object||!published||command_index<0)return GE_ORIGINAL_DOOR_INVALID_ARGUMENT;
    if(!ge_dam_setup_world_door_setup(object,&setup)
       &&!ge_original_door_native_setup(object,&setup))
        return GE_ORIGINAL_DOOR_INVALID_SETUP;
    if(!providers.model_load||!providers.resolve_model_instance
       ||!providers.allocate_collision||!providers.walk_tiles
       ||!providers.get_tile_rgb)return GE_ORIGINAL_DOOR_MISSING_PROVIDER;
    providers.model_load(providers.context,object->obj); published->model_load_calls++;
    if(!providers.resolve_model_instance(providers.context,object->obj,
        (void **)&header,(void **)&model,&scale)||!header||!model||model->obj!=header)
        return GE_ORIGINAL_DOOR_MODEL_UNAVAILABLE;
    collision=providers.allocate_collision(providers.context,0x50U);
    if(!collision)return GE_ORIGINAL_DOOR_INIT_FAILED;
    switch(ge_original_setup_door_slice(object,command_index,header,model,
                                        scale,collision,&setup)) {
    case 1:
        if(install_native_door(object,&setup,published)==NULL)
            return GE_ORIGINAL_DOOR_INIT_FAILED;
        return GE_ORIGINAL_DOOR_OK;
    case -1:return GE_ORIGINAL_DOOR_POSITION_FAILED;
    case -2:return GE_ORIGINAL_DOOR_WALK_UNAVAILABLE;
    default:return GE_ORIGINAL_DOOR_INIT_FAILED;
    }
}

int ge_original_door_release(void *definition)
{
    size_t index;
    if (definition == NULL) return 0;
    for (index = 0U; index < GE_ORIGINAL_DOOR_NATIVE_CAPACITY; ++index) {
        if (native_doors[index].definition == definition) {
            free(native_doors[index].clipped_vertices);
            memset(&native_doors[index], 0, sizeof(native_doors[index]));
            return 1;
        }
    }
    return 0;
}

uint32_t ge_original_door_capacity(void)
{
    return GE_ORIGINAL_DOOR_NATIVE_CAPACITY;
}

s32 ge_port_door_walk(StandTile **stan,f32 sx,f32 sz,f32 dx,f32 dz)
{ return providers.walk_tiles?providers.walk_tiles(providers.context,(void **)stan,sx,sz,dx,dz):-1; }
s32 ge_port_door_tile_rgb(StandTile *stan,f32 x,f32 z,u8 rgb[3])
{ return providers.get_tile_rgb?providers.get_tile_rgb(providers.context,stan,x,z,rgb):-1; }
s32 ge_port_door_portal_rooms(const BoundPadRecord *pad,s32 *a,s32 *b,
    coord3d *pa,coord3d *pb)
{
    int32_t room_a=-1,room_b=-1;
    int result;
    if(!providers.portal_rooms)return -1;
    result=providers.portal_rooms(providers.context,pad,&room_a,&room_b,
                                  pa->f,pb->f);
    *a=(s32)room_a;*b=(s32)room_b;
    return (s32)result;
}
s32 ge_port_door_find_portal(s32 a,s32 b,const coord3d *pa,const coord3d *pb)
{ return providers.find_portal?providers.find_portal(providers.context,a,b,pa->f,pb->f):-1; }
void ge_port_door_set_portal_open(s32 portal,s32 open)
{ if(providers.set_portal_open)providers.set_portal_open(providers.context,portal,open); }
void ge_port_door_register_room(PropRecord *prop,s16 room)
{ if(providers.register_room)providers.register_room(providers.context,prop,(int16_t)room); }
void ge_port_door_publish(const GeOriginalDoorPrepared *value)
{ if(published)*published=*value; }

const char *ge_original_door_status_name(GeOriginalDoorStatus s)
{
    switch(s){case GE_ORIGINAL_DOOR_OK:return "ok";
    case GE_ORIGINAL_DOOR_INVALID_ARGUMENT:return "invalid argument";
    case GE_ORIGINAL_DOOR_INVALID_SETUP:return "invalid setup";
    case GE_ORIGINAL_DOOR_MISSING_PROVIDER:return "missing provider";
    case GE_ORIGINAL_DOOR_MODEL_UNAVAILABLE:return "model unavailable";
    case GE_ORIGINAL_DOOR_POSITION_FAILED:return "position failed";
    case GE_ORIGINAL_DOOR_WALK_UNAVAILABLE:return "walk unavailable";
    case GE_ORIGINAL_DOOR_INIT_FAILED:return "init failed";default:return "unknown";}
}

void ge_original_door_runtime_bind(
    const GeOriginalDoorRuntimeProviders *p, GeOriginalDoorRuntimeState *state)
{
    memset(&runtime_providers, 0, sizeof(runtime_providers));
    if (p != NULL) runtime_providers = *p;
    runtime_state = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->status = GE_ORIGINAL_DOOR_RUNTIME_OK;
    }
}

s32 ge_port_door_runtime_global_timer(void)
{
    return runtime_providers.global_timer != NULL
        ? runtime_providers.global_timer(runtime_providers.context) : 0;
}

s32 ge_port_door_runtime_clock_timer(void)
{
    return runtime_providers.clock_timer != NULL
        ? runtime_providers.clock_timer(runtime_providers.context) : 0;
}

ObjectRecord *ge_port_door_runtime_object(DoorRecord *door)
{
    GeOriginalDoorNativeSlot *slot = find_native_slot_by_runtime(door);
    return slot != NULL ? slot->definition : NULL;
}

DoorRecord *ge_port_door_runtime_native_definition(void *definition)
{
    return find_native_door(definition);
}

s32 ge_port_door_runtime_test_collision(PropRecord *prop)
{
    if (runtime_state != NULL) runtime_state->collision_tests++;
    if (runtime_providers.test_collision == NULL) {
        if (runtime_state != NULL)
            runtime_state->status =
                GE_ORIGINAL_DOOR_RUNTIME_MISSING_COLLISION_PROVIDER;
        return 0;
    }
    return runtime_providers.test_collision(
        runtime_providers.context, prop);
}

void ge_port_door_runtime_update_shade(PropRecord *prop, rgba_u8 *color)
{
    if (runtime_providers.update_shade != NULL)
        runtime_providers.update_shade(
            runtime_providers.context, prop, color->rgba);
}

Vertex *ge_port_door_runtime_acquire_vertices(DoorRecord *door, s32 count)
{
    GeOriginalDoorNativeSlot *slot;
    Vertex *vertices;
    if (count <= 0) {
        if (runtime_state != NULL)
            runtime_state->status =
                GE_ORIGINAL_DOOR_RUNTIME_MISSING_VERTEX_STORAGE;
        return NULL;
    }
    if (runtime_providers.acquire_vertices != NULL) {
        vertices = runtime_providers.acquire_vertices(
            runtime_providers.context, door, (uint32_t)count,
            (uint32_t)sizeof(Vertex));
    } else {
        slot = find_native_slot_by_runtime(door);
        if (slot == NULL) {
            vertices = NULL;
        } else if ((uint32_t)count <= slot->clipped_vertex_capacity) {
            vertices = slot->clipped_vertices;
        } else {
            vertices = realloc(slot->clipped_vertices,
                               (size_t)count * sizeof(*vertices));
            if (vertices != NULL) {
                slot->clipped_vertices = vertices;
                slot->clipped_vertex_capacity = (uint32_t)count;
            }
        }
    }
    if (vertices == NULL && runtime_state != NULL)
        runtime_state->status =
            GE_ORIGINAL_DOOR_RUNTIME_MISSING_VERTEX_STORAGE;
    return vertices;
}

void ge_port_door_runtime_publish_vertices(
    DoorRecord *door, const Vertex *vertices, s32 count)
{
    GeOriginalDoorNativeSlot *slot = find_native_slot_by_runtime(door);
    if (slot != NULL && vertices != NULL && count > 0) {
        const unsigned char *bytes = (const unsigned char *)vertices;
        const size_t byte_count = (size_t)count * sizeof(*vertices);
        uint64_t hash = UINT64_C(14695981039346656037);
        size_t index;
        for (index = 0U; index < byte_count; ++index) {
            hash ^= bytes[index];
            hash *= UINT64_C(1099511628211);
        }
        if (!slot->clipped_vertex_published
                || slot->clipped_vertex_count != (uint32_t)count
                || slot->clipped_vertex_hash != hash) {
            slot->clipped_vertex_hash = hash;
            slot->clipped_vertex_published = 1U;
            slot->publication_generation++;
        }
        slot->clipped_vertex_count = (uint32_t)count;
    }
    if (runtime_providers.publish_vertices != NULL)
        runtime_providers.publish_vertices(
            runtime_providers.context, door, vertices, (uint32_t)count);
}

void ge_port_door_runtime_sound(
    DoorRecord *door, GeOriginalDoorSoundEvent event)
{
    if (runtime_state != NULL) runtime_state->sound_events++;
    if (runtime_providers.sound_event != NULL)
        runtime_providers.sound_event(
            runtime_providers.context, door, event);
}

void ge_port_door_runtime_note_bbox(void)
{
    if (runtime_state != NULL) runtime_state->bbox_rebuilds++;
}

void ge_port_door_runtime_note_clipped(void)
{
    if (runtime_state != NULL) runtime_state->clipped_vertex_rebuilds++;
}

void ge_port_door_runtime_note_portal(s32 open)
{
    if (runtime_state == NULL) return;
    if (open) runtime_state->portal_open_events++;
    else runtime_state->portal_close_events++;
}

void ge_port_door_runtime_note_completed(s32 open)
{
    if (runtime_state == NULL) return;
    if (open) runtime_state->completed_opens++;
    else runtime_state->completed_closes++;
}

GeOriginalDoorRuntimeStatus ge_original_door_runtime_activate(
    void *opaque_door, int32_t state)
{
    DoorRecord *door = find_native_door(opaque_door);
    if (door == NULL || state < DOORSTATE_STATIONARY
            || state > DOORSTATE_WAITING) {
        if (runtime_state != NULL)
            runtime_state->status = GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT;
        return GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT;
    }
    ge_original_door_activate_slice(door, state);
    return runtime_state != NULL ? runtime_state->status
                                 : GE_ORIGINAL_DOOR_RUNTIME_OK;
}

GeOriginalDoorRuntimeStatus ge_original_door_runtime_tick(void *opaque_door)
{
    DoorRecord *door = find_native_door(opaque_door);
    ObjectRecord *object = opaque_door;
    s32 global_timer;
    s32 clock_timer;
    DoorRecord *linked;
    s32 all_closed;

    if (door == NULL || object->prop == NULL) {
        if (runtime_state != NULL)
            runtime_state->status = GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT;
        return GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT;
    }
    global_timer = ge_port_door_runtime_global_timer();
    clock_timer = ge_port_door_runtime_clock_timer();
    if (runtime_state != NULL) {
        runtime_state->ticks++;
        runtime_state->last_global_timer = global_timer;
        runtime_state->last_clock_timer = clock_timer;
    }
    /* Exact US propTick auto-close and interlocked-door waiting tranche. */
    if ((s32)door->openedTime > 0
            && (s32)door->openedTime
                < global_timer - (s32)door->autoCloseFrames
            && door->openstate == DOORSTATE_STATIONARY
            && !(object->flags & PROPFLAG_DOOR_KEEPOPEN))
        ge_original_door_activate_slice(door, DOORSTATE_CLOSING);
    if (door->openstate == DOORSTATE_WAITING) {
        linked = door->linkedDoor;
        all_closed = 1;
        while (linked != NULL && linked != door) {
            if (linked->openstate != DOORSTATE_STATIONARY
                    || linked->openPosition > 0.0f) all_closed = 0;
            linked = linked->linkedDoor;
        }
        if (all_closed != 0)
            ge_original_door_set_open_state_slice(
                door, DOORSTATE_OPENING);
    }
    if (door->lastcalc60i < global_timer || clock_timer == 0)
        ge_original_door_runtime_tick_slice(door);
    if (runtime_state != NULL && runtime_state->status
            == GE_ORIGINAL_DOOR_RUNTIME_MISSING_COLLISION_PROVIDER)
        runtime_state->collision_rollbacks++;
    return runtime_state != NULL ? runtime_state->status
                                 : GE_ORIGINAL_DOOR_RUNTIME_OK;
}

int ge_original_door_runtime_link_pair(
    void *first_definition, void *second_definition)
{
    DoorRecord *first = find_native_door(first_definition);
    DoorRecord *second = find_native_door(second_definition);
    if (first == NULL || second == NULL || first->linkedDoorOffset == 0)
        return 0;
    /* setupDoor assigns the authored relation in its own direction. Most
     * double doors are reciprocal, while Bunker flexi groups are directed
     * chains; only synthesize the reverse pointer when its exact relative
     * command offset names the first record. */
    first->linkedDoor = second;
    if (second->linkedDoorOffset == -first->linkedDoorOffset)
        second->linkedDoor = first;
    return 1;
}

int ge_original_door_runtime_snapshot(
    const void *opaque_door, GeOriginalDoorRuntimePublication *out)
{
    DoorRecord *door = find_native_door((void *)opaque_door);
    GeOriginalDoorNativeSlot *slot;
    ObjectRecord *object;
    const struct collision_data *collision;
    Mtxf matrix;
    coord3d position;
    uint32_t open_position_bits;
    int32_t edge;

    object = (ObjectRecord *)opaque_door;
    if (door == NULL || out == NULL || object->prop == NULL
            || object->ptr_allocated_collisiondata_block == NULL) return 0;
    slot = find_native_slot_by_runtime(door);
    if (slot == NULL) return 0;
    collision = object->ptr_allocated_collisiondata_block;
    if (collision->edges < 0
            || collision->edges
                > (int32_t)GE_ORIGINAL_DOOR_COLLISION_EDGE_CAPACITY) return 0;

    memset(out, 0, sizeof(*out));
    ge_original_door_matrix_slice(door, &matrix);
    position.x = matrix.m[3][0];
    position.y = matrix.m[3][1];
    position.z = matrix.m[3][2];
    memcpy(out->matrix, matrix.m, sizeof(out->matrix));
    memcpy(out->matrices[0], matrix.m, sizeof(out->matrices[0]));
    out->matrix_count = 1U;
    if (door->doorType == DOORTYPE_EYE
            && object->model->obj->numMatrices >= 3
            && object->model->obj->numSwitches >= 3
            && object->model->obj->Switches[1] != NULL
            && object->model->obj->Switches[2] != NULL) {
        Mtxf joint;
        coord3d *origin;
        f32 angle = M_TAU_F
            - door->openPosition * M_TAU_F / 360.0f;
        origin = (coord3d *)object->model->obj->Switches[1]->Data;
        matrix_4x4_set_rotation_around_x(angle, &joint);
        matrix_4x4_set_position(origin, &joint);
        matrix_4x4_multiply_in_place(&matrix, &joint);
        memcpy(out->matrices[1], joint.m, sizeof(out->matrices[1]));
        origin = (coord3d *)object->model->obj->Switches[2]->Data;
        matrix_4x4_set_rotation_around_x(M_TAU_F - angle, &joint);
        matrix_4x4_set_position(origin, &joint);
        matrix_4x4_multiply_in_place(&matrix, &joint);
        memcpy(out->matrices[2], joint.m, sizeof(out->matrices[2]));
        out->matrix_count = 3U;
        out->articulated = 1U;
    } else if (door->doorType == DOORTYPE_IRIS
            && object->model->obj->numMatrices >= 13
            && object->model->obj->numSwitches >= 13) {
        f32 outer_angle = 0.0f;
        f32 inner_angle = door->openPosition * M_TAU_F / 360.0f;
        f32 threshold = door->maxFrac * 0.3f;
        uint16_t pair;
        if (threshold < door->openPosition)
            outer_angle = door->maxFrac
                * (door->openPosition - threshold)
                / (door->maxFrac - threshold) * M_TAU_F / 360.0f;
        for (pair = 0U; pair < 6U; ++pair) {
            const uint16_t outer_index = (uint16_t)(pair * 2U + 1U);
            const uint16_t inner_index = (uint16_t)(outer_index + 1U);
            Mtxf outer;
            Mtxf inner;
            coord3d *origin;
            if (object->model->obj->Switches[outer_index] == NULL
                    || object->model->obj->Switches[inner_index] == NULL)
                return 0;
            origin = (coord3d *)
                object->model->obj->Switches[outer_index]->Data;
            matrix_4x4_set_rotation_around_z(outer_angle, &outer);
            matrix_4x4_set_position(origin, &outer);
            matrix_4x4_multiply_in_place(&matrix, &outer);
            memcpy(out->matrices[outer_index], outer.m,
                   sizeof(out->matrices[outer_index]));
            origin = (coord3d *)
                object->model->obj->Switches[inner_index]->Data;
            matrix_4x4_set_rotation_around_z(inner_angle, &inner);
            matrix_4x4_set_position(origin, &inner);
            matrix_4x4_multiply_in_place(&outer, &inner);
            memcpy(out->matrices[inner_index], inner.m,
                   sizeof(out->matrices[inner_index]));
        }
        out->matrix_count = 13U;
        out->articulated = 1U;
    }
    memcpy(out->position, position.f, sizeof(out->position));
    out->bbox[0] = door->bbox.Bounds.xmin;
    out->bbox[1] = door->bbox.Bounds.xmax;
    out->bbox[2] = door->bbox.Bounds.ymin;
    out->bbox[3] = door->bbox.Bounds.ymax;
    out->bbox[4] = door->bbox.Bounds.zmin;
    out->bbox[5] = door->bbox.Bounds.zmax;
    out->collision_edges = collision->edges;
    for (edge = 0; edge < collision->edges; edge++) {
        out->collision_polygon[edge][0] = collision->polygon[edge].x;
        out->collision_polygon[edge][1] = collision->polygon[edge].y;
    }
    out->collision_top = collision->top;
    out->collision_bottom = collision->bottom;
    out->open_position = door->openPosition;
    out->max_frac = door->maxFrac;
    out->speed = door->speed;
    out->clipped_vertices = door->unkcc;
    out->clipped_vertex_count = slot->clipped_vertex_count;
    out->clipped_vertex_stride = sizeof(Vertex);
    memcpy(&open_position_bits, &door->openPosition,
           sizeof(open_position_bits));
    if (!slot->open_position_published
            || slot->last_open_position_bits != open_position_bits) {
        slot->last_open_position_bits = open_position_bits;
        slot->open_position_published = 1U;
        slot->publication_generation++;
    }
    out->generation = slot->publication_generation;
    out->open_state = door->openstate;
    out->portal_number = door->portalNumber;
    out->room = (int16_t)object->prop->rooms[0];
    return 1;
}

const char *ge_original_door_runtime_status_name(
    GeOriginalDoorRuntimeStatus status)
{
    switch (status) {
    case GE_ORIGINAL_DOOR_RUNTIME_OK: return "ok";
    case GE_ORIGINAL_DOOR_RUNTIME_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_DOOR_RUNTIME_MISSING_COLLISION_PROVIDER:
        return "missing collision provider";
    case GE_ORIGINAL_DOOR_RUNTIME_MISSING_VERTEX_STORAGE:
        return "missing vertex storage";
    default: return "unknown";
    }
}
