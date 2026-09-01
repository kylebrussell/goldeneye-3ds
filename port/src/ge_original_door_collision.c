#include "ge_original_door_collision.h"
#include "ge_original_door_collision_internal.h"

#include <string.h>

#include "game/chrai.h"
#include "ge_original_dam_world.h"
#include "ge_original_stage_prop_materializer.h"

#if defined(__GNUC__)
/* Dam-only focused binaries predate the campaign-generic setup adapter. The
 * live 3DS link supplies the strong definition; this weak, explicitly-failing
 * seam keeps the original Dam header side table independently testable. */
__attribute__((weak)) int ge_original_stage_prop_native_definition_header(
    const void *definition, uint16_t *extrascale,
    uint8_t *state, uint8_t *type)
{
    (void)definition;
    (void)extrascale;
    (void)state;
    (void)type;
    return 0;
}
#endif

extern PropRecord g_Props[MAX_PROPS];

PropRecord *ge_port_stan_prop_at_index(s16 index)
{
    return &g_Props[index];
}

static GeOriginalDoorCharacterCollisionProviders collision_providers;
static GeOriginalDoorCollisionState *collision_state;
static int character_provider_missing;
static int object_metadata_missing;
static s16 collision_prop_indices[MAX_PROPS + 1U];

s16 *ptr_list_object_lookup_indices = collision_prop_indices;
u32 num_obj_position_data_entries;

static void note_missing_character_provider(void)
{
    character_provider_missing = 1;
    if (collision_state != NULL) {
        collision_state->missing_character_calls++;
        collision_state->status =
            GE_ORIGINAL_DOOR_COLLISION_MISSING_CHARACTER_PROVIDER;
    }
}

void ge_original_door_collision_bind(
    const GeOriginalDoorCharacterCollisionProviders *providers,
    GeOriginalDoorCollisionState *state)
{
    memset(&collision_providers, 0, sizeof(collision_providers));
    if (providers != NULL) collision_providers = *providers;
    collision_state = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->status = GE_ORIGINAL_DOOR_COLLISION_OK;
    }
    collision_prop_indices[0] = -1;
    num_obj_position_data_entries = 1U;
}

void ge_port_door_collision_chr_update_bounds(
    PropRecord *prop, struct rect4f **polygon, s32 *edges,
    f32 *top, f32 *bottom)
{
    void *opaque_polygon = NULL;
    int32_t native_edges = 0;
    if (prop == NULL || prop->chr == NULL) {
        *polygon = NULL;
        *edges = 0;
        note_missing_character_provider();
        return;
    }
    if (collision_providers.polygon_bounds == NULL) {
        ge_original_door_chrUpdateCollisionBounds_exact(
            prop, polygon, edges, top, bottom);
        return;
    }
    if (!collision_providers.polygon_bounds(
                collision_providers.context, prop, &opaque_polygon,
                &native_edges, top, bottom)) {
        *polygon = NULL;
        *edges = 0;
        note_missing_character_provider();
        return;
    }
    *polygon = opaque_polygon;
    *edges = (s32)native_edges;
}

void ge_port_door_collision_chr_width_height(
    PropRecord *prop, f32 *radius, f32 *height, f32 *lower_offset)
{
    if (prop == NULL || prop->chr == NULL) {
        *radius = *height = *lower_offset = 0.0f;
        note_missing_character_provider();
        return;
    }
    if (collision_providers.cylinder_bounds == NULL) {
        ge_original_door_chrGetChrWidthHeight_exact(
            prop, radius, height, lower_offset);
        return;
    }
    if (!collision_providers.cylinder_bounds(
                collision_providers.context, prop, radius, height,
                lower_offset)) {
        *radius = *height = *lower_offset = 0.0f;
        note_missing_character_provider();
    }
}

f32 ge_port_door_collision_chr_ground(PropRecord *prop)
{
    float ground = 0.0f;
    if (prop == NULL || prop->chr == NULL) {
        note_missing_character_provider();
        return 0.0f;
    }
    if (collision_providers.ground == NULL)
        return ge_original_door_chrGetChrGround_exact(prop);
    if (!collision_providers.ground(
                collision_providers.context, prop, &ground)) {
        note_missing_character_provider();
        return 0.0f;
    }
    return ground;
}

u32 ge_port_door_collision_character_flags(PropRecord *prop)
{
    if (prop == NULL || prop->chr == NULL) {
        note_missing_character_provider();
        return 0U;
    }
    /* Canonical sub_GAME_7F0448A8 observes this word through the 32-bit
     * PropRecord union's ObjectRecord::model offset.  Name the same native
     * ChrRecord field explicitly on hosts whose pointer alignment differs. */
    return (u32)prop->chr->chrflags;
}

static u8 object_header_value(ObjectRecord *object, int want_type)
{
    uint8_t state = 0U;
    uint8_t type = 0U;
    if (!ge_dam_setup_world_definition_header(
            object, NULL, &state, &type)
            && !ge_original_stage_prop_native_definition_header(
                object, NULL, &state, &type)) {
        object_metadata_missing = 1;
        if (collision_state != NULL) {
            collision_state->missing_object_metadata_calls++;
            collision_state->status =
                GE_ORIGINAL_DOOR_COLLISION_MISSING_OBJECT_METADATA;
        }
    }
    return want_type ? type : state;
}

u8 ge_port_door_collision_object_state(ObjectRecord *object)
{
    return object_header_value(object, 0);
}

u8 ge_port_door_collision_object_type(ObjectRecord *object)
{
    return object_header_value(object, 1);
}

int ge_original_door_collision_test(void *context, void *opaque_prop)
{
    PropRecord *prop = opaque_prop;
    const uintptr_t address = (uintptr_t)opaque_prop;
    const uintptr_t first = (uintptr_t)&g_Props[0];
    const uintptr_t end = (uintptr_t)&g_Props[MAX_PROPS];
    s32 result;
    (void)context;
    if (prop == NULL || address < first || address >= end
            || (address - first) % sizeof(g_Props[0]) != 0U) {
        if (collision_state != NULL)
            collision_state->status =
                GE_ORIGINAL_DOOR_COLLISION_INVALID_ARGUMENT;
        return 0;
    }
    character_provider_missing = 0;
    object_metadata_missing = 0;
    if (collision_state != NULL) {
        collision_state->tests++;
        collision_state->status = GE_ORIGINAL_DOOR_COLLISION_OK;
    }
    result = ge_original_door_collision_exact_slice(prop);
    if (character_provider_missing || object_metadata_missing) result = 0;
    if (collision_state != NULL) {
        if (result != 0) collision_state->clear_results++;
        else collision_state->blocked_results++;
    }
    return result != 0;
}

const char *ge_original_door_collision_status_name(
    GeOriginalDoorCollisionStatus status)
{
    switch (status) {
    case GE_ORIGINAL_DOOR_COLLISION_OK: return "ok";
    case GE_ORIGINAL_DOOR_COLLISION_INVALID_ARGUMENT:
        return "invalid argument";
    case GE_ORIGINAL_DOOR_COLLISION_MISSING_CHARACTER_PROVIDER:
        return "missing character provider";
    case GE_ORIGINAL_DOOR_COLLISION_MISSING_OBJECT_METADATA:
        return "missing object metadata";
    default: return "unknown";
    }
}
