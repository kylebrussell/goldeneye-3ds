#ifndef GE_ORIGINAL_STAGE_GUARD_ACTOR_H
#define GE_ORIGINAL_STAGE_GUARD_ACTOR_H

#include <stddef.h>
#include <stdint.h>

struct ChrRecord;
struct PropRecord;
struct Model;
struct coord3d;
struct StandTile;
struct AIRecord;
struct WeaponObjRecord;
struct HatRecord;

/* Isolated owner for the unchanged init_GUARDdata_with_set_values body.
 * Its canonical globals are renamed into this adapter so loading a streamed
 * stage does not replace the live level's g_ChrSlots binding. */
int ge_original_stage_guard_actor_pool_begin(struct ChrRecord *chrs,
                                              size_t chr_capacity);
void ge_original_stage_guard_actor_pool_end(struct ChrRecord *chrs);
struct PropRecord *ge_original_stage_guard_actor_construct_exact(
    struct PropRecord *prop, struct Model *model, struct coord3d *position,
    float angle, struct StandTile *stan, struct AIRecord *ailist);
float ge_original_stage_guard_model_get_root_angle_exact(struct Model *model);
void ge_original_stage_guard_model_set_root_offset_exact(
    struct Model *model, struct coord3d *offset);
void ge_original_stage_guard_model_set_root_angle_exact(
    struct Model *model, float angle);
void ge_original_stage_guard_model_set_scale_exact(
    struct Model *model, float scale);

/* Authored assignments made by expand_09_characters after chrAllocate. */
int ge_original_stage_guard_actor_apply_setup(
    struct PropRecord *prop, int32_t chr_id, int32_t body_id, int32_t head_id,
    uint16_t health, uint16_t reaction, uint16_t preset,
    uint16_t chrpreset, uint16_t appearance_flags);

/* Unchanged propobj/chrprop bodies used by setup's ASSIGNEDTOCHR branch. */
int ge_original_stage_guard_actor_equip_weapon(
    struct WeaponObjRecord *weapon, struct ChrRecord *chr);
void ge_original_stage_guard_actor_set_gunfire_visible(
    struct PropRecord *prop, int visible);
void ge_original_stage_guard_actor_reparent_prop(
    struct PropRecord *child, struct PropRecord *parent);
struct PropRecord *ge_original_stage_guard_actor_apply_hat(
    struct HatRecord *hat, struct ChrRecord *chr, void *model_header,
    struct PropRecord *prop, struct Model *model, float pitem_scale);

/* Exact set_color_shading_from_tile/update_color_shading bodies. The tile
 * callback is the platform/STAN service that supplies the original tile RGB. */
typedef int (*GeOriginalStageGuardTileRgbFn)(
    void *context, void *stan, float x, float z, uint8_t rgb[3]);
void ge_original_stage_guard_actor_set_lighting_service(
    GeOriginalStageGuardTileRgbFn callback, void *context);
int ge_original_stage_guard_actor_sample_lighting(
    struct PropRecord *prop, uint8_t rgba[4]);
void ge_original_stage_guard_actor_step_lighting(
    uint8_t current[4], const uint8_t target[4]);

#endif
