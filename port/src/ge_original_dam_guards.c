#include "ge_original_dam_guards.h"

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/chrai.h"
#include "game/chrobjdata.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/stan.h"

#include "ge_original_dam_guard_model.h"
#include "ge_original_dam_guard_weapon_model.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_dam_setup.h"

#include <math.h>
#include <string.h>

#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif

enum {
    GE_DAM_GUARD_FIRST_COMMAND = 23,
    GE_DAM_GUARD_BODY_GREATGUARD2 = 37,
    GE_DAM_GUARD_EXTRA_SLOTS = 10,
    GE_DAM_GUARD_RW_WORD_CAPACITY = 512,
    GE_DAM_GUARD_WEAPON_FIRST_COMMAND = 59,
    GE_DAM_GUARD_WEAPON_LAST_COMMAND = 62,
    GE_DAM_GUARD_WEAPON_MODEL = PROP_CHRKALASH,
    GE_DAM_GUARD_WEAPON_ITEM = ITEM_AK47,
    GE_DAM_GUARD_WEAPON_RW_WORD_CAPACITY = 1
};

typedef struct GeDamGuardSlot {
    Model model;
    u32 rwdata[GE_DAM_GUARD_RW_WORD_CAPACITY];
    RenderPosView matrices[0x14];
    PropRecord *prop;
    ChrRecord *chr;
    s32 command_index;
} GeDamGuardSlot;

typedef struct GeDamGuardWeaponSlot {
    WeaponObjRecord weapon;
    Model model;
    u32 rwdata[GE_DAM_GUARD_WEAPON_RW_WORD_CAPACITY];
    RenderPosView matrices[1];
    PropRecord *prop;
    s32 command_index;
} GeDamGuardWeaponSlot;

static GeDamGuardSlot ge_guard_slots[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY];
static GeDamGuardWeaponSlot
    ge_guard_weapon_slots[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY];
static ChrRecord ge_guard_records[GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY
                                  + GE_DAM_GUARD_EXTRA_SLOTS];
static GeOriginalDamGuardStats ge_guard_stats;
static size_t ge_guard_count;

extern stagesetup g_CurrentSetup;
extern ChrRecord *g_ChrSlots;
extern s32 g_NumChrSlots;
extern PropRecord *chrpropAllocate(void);
extern void chrpropActivate(PropRecord *prop);
extern void chrpropEnable(PropRecord *prop);
extern void chrpropRegisterRooms(PropRecord *prop);
extern void modelCalculateRwDataLen(ModelFileHeader *header);
extern void modelInit(Model *model, ModelFileHeader *header, u32 *rwdata);
extern void subcalcmatrices(ModelRenderData *renderdata, Model *model);
extern void modelSetScale(Model *model, f32 scale);
extern void ge_door_collision_roomGetProps(s32 *rooms);
extern void ge_door_collision_chraiGetCollisionBounds(
    PropRecord *prop, struct rect4f **polygon, s32 *edges,
    f32 *top, f32 *bottom);
extern f32 ge_door_collision_stanGetSignedPointLineDistance(
    f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz);
extern f32 ge_door_collision_distBetweenPoints2d(
    f32 ax, f32 az, f32 bx, f32 bz);
extern bool ge_door_collision_stanPointProjectsOntoEdge(
    f32 ax, f32 az, f32 bx, f32 bz, f32 px, f32 pz);
extern s16 *ptr_list_object_lookup_indices;
extern PropRecord g_Props[MAX_PROPS];

static s32 ge_setup_record_words(u8 type)
{
    switch (type) {
    case PROPDEF_GUARD: return 7;
    case PROPDEF_DOOR: return 64;
    case PROPDEF_DOOR_SCALE: return 2;
    case PROPDEF_PROP: case PROPDEF_GLASS: case PROPDEF_SAFE:
    case PROPDEF_GAS_RELEASING: case PROPDEF_ALARM: case PROPDEF_RACK:
    case PROPDEF_HAT: return 32;
    case PROPDEF_KEY: return 33;
    case PROPDEF_TINTED_GLASS: return 36;
    case PROPDEF_CCTV: return 0x3b;
    case PROPDEF_MAGAZINE: return 0x21;
    case PROPDEF_COLLECTABLE: return 0x22;
    case PROPDEF_MONITOR: return 0x40;
    case PROPDEF_MULTI_MONITOR: return 0x95;
    case PROPDEF_AUTOGUN: return 0x36;
    case PROPDEF_LINK: return 3;
    case PROPDEF_GUARD_ATTRIBUTE: return 3;
    case PROPDEF_SWITCH: return 4;
    case PROPDEF_SAFE_ITEM: return 5;
    case PROPDEF_AMMO: return 0x2d;
    case PROPDEF_ARMOUR: return 0x22;
    case PROPDEF_TAG: return 4;
    case PROPDEF_RENAME: return 10;
    case PROPDEF_OBJECTIVE_START: return 4;
    case PROPDEF_OBJECTIVE_END: return 1;
    case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
    case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
    case PROPDEF_OBJECTIVE_FAIL_CONDITION:
    case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT: return 2;
    case PROPDEF_OBJECTIVE_PHOTOGRAPH:
    case PROPDEF_OBJECTIVE_ENTER_ROOM: return 4;
    case PROPDEF_OBJECTIVE_NULL: case PROPDEF_OBJECTIVE_COPY_ITEM: return 1;
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM: return 5;
    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT: case PROPDEF_LOCK_DOOR: return 4;
    case PROPDEF_VEHICHLE: return 0x2c;
    case PROPDEF_AIRCRAFT: return 0x2d;
    case PROPDEF_TANK: return 0x38;
    case PROPDEF_CAMERAPOS: return 7;
    default: return 1;
    }
}

static AIRecord *ge_ailist_find_by_id(s32 id)
{
    s32 index;
    if (g_CurrentSetup.ailists == NULL) return NULL;
    for (index = 0; g_CurrentSetup.ailists[index].ailist != NULL; index++) {
        if (g_CurrentSetup.ailists[index].ID == id)
            return g_CurrentSetup.ailists[index].ailist;
    }
    return NULL;
}

/* Exact getposstan body for the canonical expand_09_characters call. */
static s32 ge_guard_prop_is_cdtype(PropRecord *prop, s32 cdtypes)
{
    if (prop->type == PROP_TYPE_VIEWER || prop->type == PROP_TYPE_PLAYER)
        return (cdtypes & CDTYPE_PLAYERS) != 0;
    if (prop->type == PROP_TYPE_CHR)
        return (cdtypes & CDTYPE_CHRS) != 0;
    /* expand_09_characters runs before the object setup cases in prop.c.
     * Reject an unexpected setup order instead of guessing ObjectRecord
     * header semantics here. */
    return FALSE;
}

/* Exact dynamic-prop suffix of stanTestVolume's
 * stanCircleLegalXFObjTypeY path for the character/player prop types that can
 * precede PROPDEF_GUARD in the canonical setup order. */
static s32 ge_guard_dynamic_volume(StandTile **tile, f32 x, f32 z,
                                   f32 radius, s32 cdtypes)
{
    s32 rooms[21];
    s32 room_count = 0;
    s16 *prop_index;
    s32 result;

    result = sub_GAME_7F0B21B0(tile, x, z, radius, rooms,
                              &room_count, 20);
    if (result >= 0) return result;
    if (room_count > 20) room_count = 20;
    rooms[room_count] = -1;
    ge_door_collision_roomGetProps(rooms);

    for (prop_index = ptr_list_object_lookup_indices;
            *prop_index >= 0; prop_index++) {
        PropRecord *prop = &g_Props[*prop_index];
        struct rect4f *polygon = NULL;
        s32 edges = 0;
        f32 top = 0.0f;
        f32 bottom = 0.0f;
        f32 closest = -1.0f;
        s32 edge;

        if (!ge_guard_prop_is_cdtype(prop, cdtypes)) continue;
        ge_door_collision_chraiGetCollisionBounds(
            prop, &polygon, &edges, &top, &bottom);
        if (edges <= 0 || polygon == NULL) continue;
        for (edge = 0; edge < edges; edge++) {
            const s32 next = (edge + 1) % edges;
            f32 edge_distance = ge_door_collision_stanGetSignedPointLineDistance(
                polygon->points[edge].x, polygon->points[edge].y,
                polygon->points[next].x, polygon->points[next].y, x, z);
            f32 point_a;
            f32 point_b;
            if (edge_distance < 0.0f) edge_distance = -edge_distance;
            if (closest >= edge_distance) continue;
            point_a = ge_door_collision_distBetweenPoints2d(
                polygon->points[edge].x, polygon->points[edge].y, x, z);
            point_b = ge_door_collision_distBetweenPoints2d(
                polygon->points[next].x, polygon->points[next].y, x, z);
            if (edge_distance < radius
                    && (point_a < radius || point_b < radius
                        || ge_door_collision_stanPointProjectsOntoEdge(
                            polygon->points[edge].x, polygon->points[edge].y,
                            polygon->points[next].x, polygon->points[next].y,
                            x, z)))
                closest = edge_distance;
        }
        if (closest > -1.0f) return STAN_COLLISION_FOUND;
    }
    return STAN_COLLISION_NONE;
}

static s32 ge_getposstan(coord3d *pos, StandTile *stan, f32 radius,
                         coord3d *result, StandTile **result_stan)
{
    const s32 cdtypes = CDTYPE_OBJS | CDTYPE_DOORS | CDTYPE_PLAYERS
        | CDTYPE_CHRS | CDTYPE_PATHBLOCKER;
    *result = *pos;
    *result_stan = stan;
    if (stan == NULL) return FALSE;
    if (radius > 0.0f) {
        if (stanTestVolume(result_stan, result->x, result->z,
                radius, 0, 0.0f, 1.0f) >= 0)
            return FALSE;
        if (ge_guard_dynamic_volume(result_stan, result->x, result->z,
                radius, cdtypes) >= 0)
            return FALSE;
    }
    return TRUE;
}

static void ge_model_set_root(Model *model, const coord3d *position,
                              f32 angle)
{
    ModelRwData_HeaderRecord *root =
        &modelGetNodeRwData(model, model->obj->RootNode)->Header;
    root->pos = *position;
    root->unk14 = angle;
    root->unk20 = angle;
    root->unk30 = angle;
}

static void ge_initialize_chr(GeDamGuardSlot *slot, PropRecord *prop,
                              ChrRecord *chr, ModelFileHeader *header,
                              const coord3d *position, f32 angle,
                              StandTile *stan, AIRecord *ailist)
{
    Model *model = &slot->model;
    memset(model, 0, sizeof(*model));
    memset(slot->rwdata, 0, sizeof(slot->rwdata));
    modelInit(model, header, slot->rwdata);
    model->scale = 0.1f;
    model->anim = NULL;
    model->anim2 = NULL;
    model->playspeed = 1.0f;
    model->anim_translation_scale = 1.0f;
    model->endframe = -1.0f;
    model->unk6c = -1.0f;
    ge_model_set_root(model, position, angle);

    memset(chr, 0, sizeof(*chr));
    prop->type = PROP_TYPE_CHR;
    prop->chr = chr;
    prop->pos = *position;
    prop->stan = stan;
    prop->rooms[0] = stan->room;
    prop->rooms[1] = prop->rooms[2] = 0xffU;
    chr->prop = prop;
    chr->model = model;
    model->unk00 = 0x0a;
    model->chr = chr;
    chr->field_20 = NULL;
    chr->lastknowntargetpos.x = 0.0f;
    chr->lastknowntargetpos.y = 0.0f;
    chr->lastknowntargetpos.z = 0.0f;
    chr->targetTile = NULL;
    chr->maxdamage = 4.0f;
    chr->nextcol.r = chr->nextcol.g = chr->nextcol.b = 0xffU;
    chr->nextcol.a = 0xffU;
    chr->shadecol = chr->nextcol;
    chr->fadealpha = 0xffU;
    chr->chrflags = CHRFLAG_INIT;
    chr->hidden = CHRHIDDEN_NONE;
    chr->prevpos = *position;
    chr->ailist = ailist;
    chr->aireturnlist = -1;
    chr->padpreset1 = -1;
    chr->chrseeshot = -1;
    chr->chrseedie = -1;
    chr->chrpreset1 = -1;
    chr->beams[0].unk00 = -1;
    chr->beams[1].unk00 = -1;
    chr->flinchcnt = -1;
    chr->chrwidth = 20.0f;
    chr->chrheight = 185.0f;
    chr->ground = position->y;
    model->chr = chr;
}

static ChrRecord *ge_guard_find_chrnum(s16 chrnum, size_t *slot_index)
{
    size_t index;
    for (index = 0U; index < ge_guard_count; index++) {
        if (ge_guard_slots[index].chr != NULL
                && ge_guard_slots[index].chr->chrnum == chrnum) {
            if (slot_index != NULL) *slot_index = index;
            return ge_guard_slots[index].chr;
        }
    }
    return NULL;
}

static void ge_guard_weapon_set_gunfire_visible(PropRecord *prop, s32 firing)
{
    ObjectRecord *object = prop->obj;
    Model *model = object->model;
    ModelNode *node;
    if (model != NULL && model->obj->Skeleton == &skeleton_prop_weapon) {
        node = model->obj->Switches[0];
        if (node != NULL)
            modelGetNodeRwData(model, node)->Gunfire.visible = firing;
        node = model->obj->Switches[2];
        if (node != NULL)
            modelGetNodeRwData(model, node)->BSP.visible = firing;
    }
}

static void ge_guard_weapon_reparent(PropRecord *new_child,
                                     PropRecord *host)
{
    /* Exact chrpropReparent body; retained locally because the bounded prop
     * allocator slice does not publish that canonical function to all host
     * constructor fixtures. */
    new_child->parent = host;
    if (host->child != NULL) host->child->next = new_child;
    new_child->prev = host->child;
    new_child->next = NULL;
    new_child->stan = NULL;
    host->child = new_child;
}

static void ge_copy_authored_weapon(GeDamGuardWeaponSlot *slot,
                                    const u32 *command)
{
    WeaponObjRecord *weapon = &slot->weapon;
    ObjectRecord *object = (ObjectRecord *)(void *)weapon;
    size_t row;
    size_t column;
    memset(weapon, 0, sizeof(*weapon));
    object->extrascale = (u16)(command[0] >> 16);
    object->state = (u8)(command[0] >> 8);
    object->type = (u8)command[0];
    object->obj = (s16)(command[1] >> 16);
    object->pad = (s16)command[1];
    object->flags = command[2];
    object->flags2 = command[3];
    object->runtime_bitflags = command[4];
    for (row = 0U; row < 4U; row++) {
        for (column = 0U; column < 4U; column++) {
            memcpy(&object->mtx.m[row][column],
                   &command[6U + row * 4U + column], sizeof(f32));
        }
    }
    memcpy(&object->runtime_pos.x, &command[22], sizeof(f32));
    memcpy(&object->runtime_pos.y, &command[23], sizeof(f32));
    memcpy(&object->runtime_pos.z, &command[24], sizeof(f32));
    memcpy(&object->maxdamage, &command[28], sizeof(f32));
    /* Exact weaponAssignToHome -> sub_GAME_7F052030 fixed-point conversion. */
    object->damage = (f32)(s32)command[29] / M_U16_MAX_VALUE_F;
    memcpy(&object->shadecol, &command[30], sizeof(object->shadecol));
    memcpy(&object->nextcol, &command[31], sizeof(object->nextcol));
    weapon->weaponnum = (s8)(command[32] >> 24);
    weapon->LinkedWeaponType = (s8)(command[32] >> 16);
    weapon->timer = (s16)command[32];
    weapon->dualweapon = NULL;
}

static GeOriginalDamGuardStatus ge_expand_guard_weapon(
    const u32 *command, s32 command_index)
{
    const u16 extrascale = (u16)(command[0] >> 16);
    const u8 type = (u8)command[0];
    const s16 model_id = (s16)(command[1] >> 16);
    const s16 chrnum = (s16)command[1];
    const u32 flags = command[2];
    const s8 item_id = (s8)(command[32] >> 24);
    const GUNHAND hand = (flags & PROPFLAG_WEAPON_LEFTHANDED)
        ? GUNLEFT : GUNRIGHT;
    GeDamGuardWeaponSlot *slot;
    ModelFileHeader *header;
    ChrRecord *chr;
    PropRecord *prop;
    size_t slot_index;

    ge_guard_stats.authored_weapons++;
    chr = ge_guard_find_chrnum(chrnum, &slot_index);
    if (chr == NULL) return GE_ORIGINAL_DAM_GUARD_OK;
    if (type != PROPDEF_COLLECTABLE || extrascale != 0x0100U
            || model_id != GE_DAM_GUARD_WEAPON_MODEL
            || item_id != GE_DAM_GUARD_WEAPON_ITEM
            || (flags & PROPFLAG_ASSIGNEDTOCHR) == 0U)
        return GE_ORIGINAL_DAM_GUARD_WEAPON_ABI_UNAVAILABLE;
    if (chr->weapons_held[hand] != NULL)
        return GE_ORIGINAL_DAM_GUARD_WEAPON_ABI_UNAVAILABLE;
    header = ge_original_dam_guard_weapon_model_header();
    if (header == NULL)
        return GE_ORIGINAL_DAM_GUARD_WEAPON_UNAVAILABLE;
    if (header->numRecords > GE_DAM_GUARD_WEAPON_RW_WORD_CAPACITY
            || chr->model == NULL || chr->model->obj == NULL
            || chr->model->obj->numSwitches <= (hand == GUNLEFT ? 5 : 3)
            || chr->model->obj->Switches[hand == GUNLEFT ? 5 : 3] == NULL)
        return GE_ORIGINAL_DAM_GUARD_WEAPON_ABI_UNAVAILABLE;

    prop = chrpropAllocate();
    if (prop == NULL) return GE_ORIGINAL_DAM_GUARD_PROP_UNAVAILABLE;
    slot = &ge_guard_weapon_slots[slot_index];
    ge_copy_authored_weapon(slot, command);
    memset(&slot->model, 0, sizeof(slot->model));
    memset(slot->rwdata, 0, sizeof(slot->rwdata));
    modelInit(&slot->model, header, slot->rwdata);
    if (ge_original_objInitPreallocatedSlice(
            (ObjectRecord *)(void *)&slot->weapon, header, prop, &slot->model,
            0.1f, NULL) == NULL)
        return GE_ORIGINAL_DAM_GUARD_WEAPON_UNAVAILABLE;
    prop->type = PROP_TYPE_WEAPON;
    ge_guard_weapon_set_gunfire_visible(prop, FALSE);
    modelSetScale(&slot->model,
                  slot->model.scale * ((f32)extrascale / 256.0f));

    /* Exact chrEquipWeapon successful arm: authored hand, model relation,
     * held-prop publication, then child-prop reparenting. */
    slot->model.attachedto = chr->model;
    slot->model.attachedto_objinst =
        chr->model->obj->Switches[hand == GUNLEFT ? 5 : 3];
    chr->weapons_held[hand] = prop;
    ge_guard_weapon_reparent(prop, chr->prop);
    slot->prop = prop;
    slot->command_index = command_index;
    ge_guard_stats.attached_weapons++;
    return GE_ORIGINAL_DAM_GUARD_OK;
}

static GeOriginalDamGuardStatus ge_expand_guard(const u32 *command,
                                                 s32 command_index,
                                                 size_t slot_index)
{
    const u16 chrnum = (u16)(command[1] >> 16);
    const u16 pad_id = (u16)command[1];
    const u16 body_id = (u16)(command[2] >> 16);
    const u16 ai_list_id = (u16)command[2];
    const u16 preset = (u16)(command[3] >> 16);
    const u16 chrpreset = (u16)command[3];
    const u16 health = (u16)(command[4] >> 16);
    const u16 reaction = (u16)command[4];
    const u16 bitflags = (u16)(command[5] >> 16);
    const s16 head_id = (s16)command[5];
    PadRecord *pad;
    coord3d position;
    StandTile *stan;
    ModelFileHeader *header;
    PropRecord *prop;
    GeDamGuardSlot *slot;
    ChrRecord *chr;
    f32 angle;

    if (body_id != GE_DAM_GUARD_BODY_GREATGUARD2 || head_id != -1)
        return GE_ORIGINAL_DAM_GUARD_MODEL_UNAVAILABLE;
    pad = &g_CurrentSetup.pads[pad_id];
    if (!ge_getposstan(&pad->pos, pad->stan, 20.0f, &position, &stan))
        return GE_ORIGINAL_DAM_GUARD_ILLEGAL_STAN;
    header = ge_original_dam_guard_model_header();
    if (header == NULL) return GE_ORIGINAL_DAM_GUARD_MODEL_UNAVAILABLE;
    modelCalculateRwDataLen(header);
    if (header->numRecords > GE_DAM_GUARD_RW_WORD_CAPACITY)
        return GE_ORIGINAL_DAM_GUARD_MODEL_ABI_UNAVAILABLE;
    prop = chrpropAllocate();
    if (prop == NULL) return GE_ORIGINAL_DAM_GUARD_PROP_UNAVAILABLE;
    slot = &ge_guard_slots[slot_index];
    chr = &ge_guard_records[slot_index];
    angle = atan2f(pad->look.x, pad->look.z);
    ge_initialize_chr(slot, prop, chr, header, &position, angle, stan,
                      ge_ailist_find_by_id(ai_list_id));
    chr->chrnum = (s16)chrnum;
    chr->hearingscale = (f32)health / 1000.0f;
    chr->visionrange = (f32)reaction;
    chr->padpreset1 = (s16)preset;
    chr->chrpreset1 = (s16)chrpreset;
    chr->headnum = (s8)head_id;
    chr->bodynum = (s8)body_id;
    if (bitflags & 4U) chr->chrflags |= CHRFLAG_CLONE;
    if (bitflags & 8U) chr->chrflags |= CHRFLAG_INVINCIBLE;
    chrpropActivate(prop);
    chrpropEnable(prop);
    chrpropRegisterRooms(prop);
    slot->prop = prop;
    slot->chr = chr;
    slot->command_index = command_index;
    return GE_ORIGINAL_DAM_GUARD_OK;
}

void ge_original_dam_guards_reset(void)
{
    memset(ge_guard_slots, 0, sizeof(ge_guard_slots));
    memset(ge_guard_weapon_slots, 0, sizeof(ge_guard_weapon_slots));
    memset(ge_guard_records, 0, sizeof(ge_guard_records));
    memset(&ge_guard_stats, 0, sizeof(ge_guard_stats));
    ge_guard_stats.first_command_index = GE_DAM_GUARD_FIRST_COMMAND;
    ge_guard_stats.last_command_index = GE_DAM_GUARD_FIRST_COMMAND - 1;
    ge_guard_count = 0U;
    g_ChrSlots = ge_guard_records;
    g_NumChrSlots = (s32)(GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY
                           + GE_DAM_GUARD_EXTRA_SLOTS);
}

GeOriginalDamGuardStatus ge_original_dam_guards_construct_initial(void)
{
    const u32 *command;
    s32 command_index = 0;
    GeOriginalDamGuardStatus status = GE_ORIGINAL_DAM_GUARD_OK;

    if (g_CurrentSetup.propDefs == NULL || g_CurrentSetup.pads == NULL) {
        ge_guard_stats.last_status = GE_ORIGINAL_DAM_GUARD_NO_SETUP;
        return GE_ORIGINAL_DAM_GUARD_NO_SETUP;
    }
    command = (const u32 *)g_CurrentSetup.propDefs;
    while ((command[0] & 0xffU) != PROPDEF_END) {
        const u8 type = (u8)(command[0] & 0xffU);
        const s32 words = ge_setup_record_words(type);
        if (type == PROPDEF_GUARD) {
            ge_guard_stats.authored_guards++;
            if (ge_guard_count < GE_ORIGINAL_DAM_INITIAL_GUARD_CAPACITY) {
                ge_guard_stats.attempted_guards++;
                status = ge_expand_guard(command, command_index,
                                         ge_guard_count);
                if (status != GE_ORIGINAL_DAM_GUARD_OK) break;
                ge_guard_stats.constructed_guards++;
                ge_guard_stats.last_command_index = (u16)command_index;
                ge_guard_count++;
            }
        } else if (type == PROPDEF_COLLECTABLE
                && command_index >= GE_DAM_GUARD_WEAPON_FIRST_COMMAND
                && command_index <= GE_DAM_GUARD_WEAPON_LAST_COMMAND) {
            status = ge_expand_guard_weapon(command, command_index);
            if (status != GE_ORIGINAL_DAM_GUARD_OK) break;
        }
        if (words <= 0) break;
        command += words;
        command_index++;
    }
    ge_guard_stats.last_status = (uint8_t)status;
    return status;
}

static GeOriginalDamGuardStatus ge_original_dam_guards_update_matrices_impl(
    const float world_to_view[4][4], int visible_only)
{
    size_t index;
    if (world_to_view == NULL) return GE_ORIGINAL_DAM_GUARD_MODEL_ABI_UNAVAILABLE;
    for (index = 0U; index < ge_guard_count; index++) {
        ModelRenderData renderdata;
        Mtxf base;
        if (!ge_original_dam_guard_is_live(index)) continue;
        if (visible_only
                && (ge_guard_slots[index].prop->flags
                    & PROPFLAG_ONSCREEN) == 0U)
            continue;
        if (visible_only
                && ge_guard_slots[index].model.render_pos != NULL
                && ge_guard_slots[index].model.render_pos
                    != ge_guard_slots[index].matrices) {
            ChrRecord *chr = ge_guard_slots[index].chr;
            GUNHAND hand;

            /* The unchanged chrTick has just run subcalcmatrices with
             * chrHandleJointPositioned installed.  That callback applies the
             * live ACT_ATTACK shoulder/torso/head aim to the transient frame
             * arena matrices.  Preserve that canonical result verbatim in
             * the renderer-owned slots: recalculating after chrTick (when the
             * callback has been cleared) discarded the attack pose and made
             * attached weapon/body geometry span unrelated joints. */
            if (ge_guard_slots[index].model.obj->numMatrices
                    > sizeof(ge_guard_slots[index].matrices)
                        / sizeof(ge_guard_slots[index].matrices[0]))
                return GE_ORIGINAL_DAM_GUARD_MODEL_ABI_UNAVAILABLE;
            memcpy(ge_guard_slots[index].matrices,
                   ge_guard_slots[index].model.render_pos,
                   (size_t)ge_guard_slots[index].model.obj->numMatrices
                       * sizeof(ge_guard_slots[index].matrices[0]));
            ge_guard_slots[index].model.render_pos =
                ge_guard_slots[index].matrices;

            /* chrRenderHeldWeapon is in the same canonical chrTick branch
             * and publishes its attached matrix into the frame arena after
             * the body matrices.  The initial Dam AK is the native slot
             * owned here, so retain that exact matrix as well. */
            for (hand = GUNRIGHT; hand <= GUNLEFT; hand++) {
                PropRecord *held_prop = chr->weapons_held[hand];
                ObjectRecord *held_object;
                Model *held_model;
                if (held_prop == NULL || held_prop->obj == NULL) continue;
                held_object = held_prop->obj;
                held_model = held_object->model;
                if (held_model != &ge_guard_weapon_slots[index].model
                        || held_model->render_pos == NULL
                        || held_model->render_pos
                            == ge_guard_weapon_slots[index].matrices)
                    continue;
                if (held_model->obj == NULL
                        || held_model->obj->numMatrices
                            > sizeof(
                                ge_guard_weapon_slots[index].matrices)
                                / sizeof(ge_guard_weapon_slots[index]
                                    .matrices[0]))
                    return GE_ORIGINAL_DAM_GUARD_MODEL_ABI_UNAVAILABLE;
                memcpy(ge_guard_weapon_slots[index].matrices,
                       held_model->render_pos,
                       (size_t)held_model->obj->numMatrices
                           * sizeof(
                               ge_guard_weapon_slots[index].matrices[0]));
                held_model->render_pos =
                    ge_guard_weapon_slots[index].matrices;
            }
            ge_guard_stats.matrix_updates++;
            continue;
        }
        memcpy(base.m, world_to_view, sizeof(base.m));
        memset(&renderdata, 0, sizeof(renderdata));
        renderdata.basemtx = &base;
        renderdata.mtxlist = &ge_guard_slots[index].matrices[0].pos;
        ge_guard_slots[index].prop->flags |= PROPFLAG_ONSCREEN;
        subcalcmatrices(&renderdata, &ge_guard_slots[index].model);
        {
            Mtxf *submatrix = modelFindNodeMtx(
                &ge_guard_slots[index].model,
                ge_guard_slots[index].model.obj->RootNode, 0);
            ge_guard_slots[index].prop->zDepth = submatrix != NULL
                ? -submatrix->m[3][2] : 0.0f;
        }
        {
            ChrRecord *chr = ge_guard_slots[index].chr;
            GUNHAND hand;
            for (hand = GUNRIGHT; hand <= GUNLEFT; hand++) {
                PropRecord *held_prop = chr->weapons_held[hand];
                ObjectRecord *held_object;
                Model *held_model;
                Mtxf rotation;
                Mtxf *base_matrix;

                if (held_prop == NULL || held_prop->obj == NULL) continue;
                held_object = held_prop->obj;
                held_model = held_object->model;
                if ((held_object->runtime_bitflags
                            & RUNTIMEBITFLAG_00000800) != 0U
                        || (s32)(held_object->flags2 << 12) < 0
                        || held_model == NULL || held_model->obj == NULL
                        || held_model->attachedto != chr->model
                        || held_model->attachedto_objinst == NULL)
                    continue;

                /* Unchanged matrix-publication arm of chrRenderHeldWeapon:
                 * the authored attachment node supplies the base matrix and
                 * the left hand receives the original pi rotation. */
                base_matrix = modelFindNodeMtx(
                    chr->model, held_model->attachedto_objinst, 0);
                if (base_matrix == NULL) continue;
                if (hand == GUNLEFT) {
                    matrix_4x4_set_rotation_around_z(3.1415927f, &rotation);
                    matrix_4x4_multiply_in_place(base_matrix, &rotation);
                    base_matrix = &rotation;
                }
                held_prop->flags |= PROPFLAG_ONSCREEN;
                /* PchrkalashZ has one GROUPSIMPLE root (matrix 0); its only
                 * other nodes are BBOX, DL and GUNFIRE and publish no model
                 * matrices. This is therefore the exact unchanged
                 * instcalcmatrices -> modelUpdateMatrices ->
                 * process_15_subposition body for this authored model, kept
                 * local so focused host slices need no unrelated full-model
                 * renderer linkage. */
                {
                    ModelRoData_GroupSimpleRecord *group =
                        &held_model->obj->RootNode->Data->GroupSimple;
                    Mtxf local;
                    held_model->render_pos =
                        &ge_guard_weapon_slots[index].matrices[0];
                    matrix_4x4_set_identity_and_position(
                        &group->Origin, &local);
                    matrix_4x4_multiply_homogeneous(
                        base_matrix, &local,
                        &held_model->render_pos[group->Group1].pos);
                }
            }
        }
        ge_guard_stats.matrix_updates++;
    }
    return GE_ORIGINAL_DAM_GUARD_OK;
}

GeOriginalDamGuardStatus ge_original_dam_guards_update_matrices(
    const float world_to_view[4][4])
{
    return ge_original_dam_guards_update_matrices_impl(world_to_view, 0);
}

GeOriginalDamGuardStatus ge_original_dam_guards_update_visible_matrices(
    const float world_to_view[4][4])
{
    return ge_original_dam_guards_update_matrices_impl(world_to_view, 1);
}

size_t ge_original_dam_guards_count(void)
{
    return ge_guard_count;
}

int ge_original_dam_guard_is_live(size_t index)
{
    const GeDamGuardSlot *slot;
    if (index >= ge_guard_count) return 0;
    slot = &ge_guard_slots[index];
    return slot->prop != NULL && slot->chr != NULL
        && slot->chr->model == &slot->model
        && slot->chr->prop == slot->prop
        && slot->model.chr == slot->chr
        && slot->prop->type == PROP_TYPE_CHR
        && slot->prop->chr == slot->chr
        && (slot->prop->flags & PROPFLAG_ENABLED) != 0U;
}

size_t ge_original_dam_guards_live_count(void)
{
    size_t index;
    size_t count = 0U;
    for (index = 0U; index < ge_guard_count; index++)
        if (ge_original_dam_guard_is_live(index)) count++;
    return count;
}

void *ge_original_dam_guard_prop(size_t index)
{
    return index < ge_guard_count ? ge_guard_slots[index].prop : NULL;
}

void *ge_original_dam_guard_chr(size_t index)
{
    return index < ge_guard_count ? ge_guard_slots[index].chr : NULL;
}

void *ge_original_dam_guard_weapon_prop(size_t index)
{
    return index < ge_guard_count ? ge_guard_weapon_slots[index].prop : NULL;
}

void ge_original_dam_guards_snapshot(GeOriginalDamGuardStats *stats)
{
    if (stats != NULL) *stats = ge_guard_stats;
}
