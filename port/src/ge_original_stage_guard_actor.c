#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

#include "ge_original_stage_guard_actor.h"
#include "ge_original_default_object_internal.h"

#include <limits.h>

extern ChrRecord *ge_original_stage_guard_actor_chr_slots;
extern s32 ge_original_stage_guard_actor_num_chr_slots;
extern s32 ge_original_stage_guard_actor_next_chr_id;
extern f32 ge_original_stage_guard_actor_animation_rate;
extern PropRecord *ge_original_stage_guard_actor_init_exact(
    PropRecord *prop, Model *model, coord3d *position, f32 angle,
    StandTile *stan, AIRecord *ailist);
extern bool ge_original_stage_guard_weapon_equip_exact(
    WeaponObjRecord *weapon, ChrRecord *chr);
extern void ge_original_stage_guard_weapon_gunfire_exact(
    PropRecord *prop, s32 visible);
extern void ge_original_stage_guard_weapon_reparent_exact(
    PropRecord *child, PropRecord *parent);
extern PropRecord *ge_original_stage_guard_hat_apply_exact(
    HatRecord *hat, ChrRecord *chr, ModelFileHeader *header,
    PropRecord *prop, Model *model);
extern void ge_original_stage_guard_lighting_sample_exact(
    PropRecord *prop, u8 rgba[4]);
extern void ge_original_stage_guard_lighting_step_exact(
    rgba_u8 *current, rgba_u8 *target);

static f32 ge_stage_guard_hat_pitem_scale;
static GeOriginalStageGuardTileRgbFn ge_stage_guard_tile_rgb;
static void *ge_stage_guard_tile_rgb_context;
static int ge_stage_guard_tile_rgb_ok;

PropRecord *ge_original_stage_guard_hat_obj_init_exact(
    ObjectRecord *object, ModelFileHeader *header, PropRecord *prop,
    Model *model)
{
    return ge_original_objInitPreallocatedSlice(
        object, header, prop, model, ge_stage_guard_hat_pitem_scale, NULL);
}

void ge_original_stage_guard_lighting_tile_rgb_exact(
    StandTile *stan, f32 x, f32 z, u8 rgb[4])
{
    if (rgb == NULL) return;
    ge_stage_guard_tile_rgb_ok = ge_stage_guard_tile_rgb != NULL
        && ge_stage_guard_tile_rgb(
            ge_stage_guard_tile_rgb_context, stan, x, z, rgb) > 0;
    if (!ge_stage_guard_tile_rgb_ok) {
        rgb[0] = rgb[1] = rgb[2] = 0U;
    }
}

int ge_original_stage_guard_actor_pool_begin(ChrRecord *chrs,
                                              size_t chr_capacity)
{
    if (chrs == NULL || chr_capacity == 0U || chr_capacity > INT_MAX) return 0;
    ge_original_stage_guard_actor_chr_slots = chrs;
    ge_original_stage_guard_actor_num_chr_slots = (s32)chr_capacity;
    ge_original_stage_guard_actor_next_chr_id = 0x1388;
    ge_original_stage_guard_actor_animation_rate = 1.0f;
    return 1;
}

void ge_original_stage_guard_actor_pool_end(ChrRecord *chrs)
{
    if (chrs == ge_original_stage_guard_actor_chr_slots) {
        ge_original_stage_guard_actor_chr_slots = NULL;
        ge_original_stage_guard_actor_num_chr_slots = 0;
        ge_stage_guard_tile_rgb = NULL;
        ge_stage_guard_tile_rgb_context = NULL;
        ge_stage_guard_tile_rgb_ok = 0;
    }
}

PropRecord *ge_original_stage_guard_actor_construct_exact(
    PropRecord *prop, Model *model, coord3d *position, float angle,
    StandTile *stan, AIRecord *ailist)
{
    if (prop == NULL || model == NULL || position == NULL || stan == NULL
            || ge_original_stage_guard_actor_chr_slots == NULL
            || ge_original_stage_guard_actor_num_chr_slots <= 0) return NULL;
    return ge_original_stage_guard_actor_init_exact(
        prop, model, position, angle, stan, ailist);
}

int ge_original_stage_guard_actor_apply_setup(
    PropRecord *prop, int32_t chr_id, int32_t body_id, int32_t head_id,
    uint16_t health, uint16_t reaction, uint16_t preset,
    uint16_t chrpreset, uint16_t appearance_flags)
{
    ChrRecord *chr;
    if (prop == NULL || prop->type != PROP_TYPE_CHR || prop->chr == NULL) return 0;
    chr = prop->chr;
    chr->chrnum = (s16)chr_id;
    chr->hearingscale = (f32)health / 1000.0f;
    chr->visionrange = (f32)reaction;
    chr->padpreset1 = (s16)preset;
    chr->chrpreset1 = (s16)chrpreset;
    chr->headnum = (s8)head_id;
    chr->bodynum = (s8)body_id;
    if ((appearance_flags & 4U) != 0U) chr->chrflags |= CHRFLAG_CLONE;
    if ((appearance_flags & 8U) != 0U) chr->chrflags |= CHRFLAG_INVINCIBLE;
    prop->flags |= PROPFLAG_ENABLED;
    return 1;
}

int ge_original_stage_guard_actor_equip_weapon(
    WeaponObjRecord *weapon, ChrRecord *chr)
{
    if (weapon == NULL || chr == NULL || weapon->prop == NULL
            || weapon->model == NULL || chr->prop == NULL
            || chr->model == NULL || chr->model->obj == NULL) return 0;
    return ge_original_stage_guard_weapon_equip_exact(weapon, chr) != FALSE;
}

void ge_original_stage_guard_actor_set_gunfire_visible(
    PropRecord *prop, int visible)
{
    if (prop != NULL && prop->obj != NULL)
        ge_original_stage_guard_weapon_gunfire_exact(prop, visible != 0);
}

void ge_original_stage_guard_actor_reparent_prop(
    PropRecord *child,PropRecord *parent)
{
    if(child!=NULL&&parent!=NULL)
        ge_original_stage_guard_weapon_reparent_exact(child,parent);
}

PropRecord *ge_original_stage_guard_actor_apply_hat(
    HatRecord *hat, ChrRecord *chr, void *model_header,
    PropRecord *prop, Model *model, float pitem_scale)
{
    if (hat == NULL || chr == NULL || model_header == NULL || prop == NULL
            || model == NULL || chr->prop == NULL || chr->model == NULL
            || chr->model->obj == NULL || chr->model->obj->numSwitches <= 6
            || chr->model->obj->Switches[6] == NULL || pitem_scale <= 0.0f)
        return NULL;
    ge_stage_guard_hat_pitem_scale = pitem_scale;
    return ge_original_stage_guard_hat_apply_exact(
        hat, chr, model_header, prop, model);
}

void ge_original_stage_guard_actor_set_lighting_service(
    GeOriginalStageGuardTileRgbFn callback, void *context)
{
    ge_stage_guard_tile_rgb = callback;
    ge_stage_guard_tile_rgb_context = context;
}

int ge_original_stage_guard_actor_sample_lighting(
    PropRecord *prop, uint8_t rgba[4])
{
    if (prop == NULL || rgba == NULL || prop->stan == NULL
            || ge_stage_guard_tile_rgb == NULL) return 0;
    ge_stage_guard_tile_rgb_ok = 0;
    ge_original_stage_guard_lighting_sample_exact(prop, rgba);
    return ge_stage_guard_tile_rgb_ok;
}

void ge_original_stage_guard_actor_step_lighting(
    uint8_t current[4], const uint8_t target[4])
{
    if (current == NULL || target == NULL) return;
    ge_original_stage_guard_lighting_step_exact(
        (rgba_u8 *)(void *)current, (rgba_u8 *)(void *)target);
}
