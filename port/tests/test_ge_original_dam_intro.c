#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bondconstants.h"
#include "bondtypes.h"
#include "assets/animationtable_data.h"
#include "ge_original_dam_intro.h"
#include "ge_original_dam_world.h"
#include "ge_original_player_spawn.h"

extern stagesetup UsetupdamZ;
extern s32 sizepropdef(PropDefHeaderRecord *pdef);

/* The focused intro slice does not link bondview.c, which owns this canonical
 * zero-terminated table in the full game.  Keep the exact production table in
 * the fixture so spawn initialization also proves the native death-animation
 * count contract. */
s32 g_bondviewBondDeathAnimations[] = {
    PTR_ANIM_death_forward_face_down,
    PTR_ANIM_death_forward_spin_face_up,
    PTR_ANIM_death_backward_fall_face_up1,
    PTR_ANIM_death_backward_spin_face_down_right,
    PTR_ANIM_death_backward_spin_face_up_right,
    PTR_ANIM_death_backward_spin_face_down_left,
    PTR_ANIM_death_backward_spin_face_up_left,
    PTR_ANIM_death_forward_face_down_hard,
    PTR_ANIM_death_forward_face_down_soft,
    PTR_ANIM_death_fetal_position_right,
    PTR_ANIM_death_fetal_position_left,
    PTR_ANIM_death_backward_fall_face_up2,
    0,
};
s32 g_bondviewBondDeathAnimationsCount;

static int inventory_reset_calls;
static ITEM_IDS added_items[4];
static int added_item_count;
static int ammo_types[4];
static int ammo_amounts[4];
static int ammo_call_count;
static int equipped_weapons[2] = {-1, -1};
static int projectile_model_load_calls;

void bondinvReinitInv(void)
{
    inventory_reset_calls++;
}

int bondinvAddInvItem(ITEM_IDS item)
{
    assert(added_item_count < 4);
    added_items[added_item_count++] = item;
    return 1;
}

int bondinvAddDoublesInvItem(ITEM_IDS right, ITEM_IDS left)
{
    (void)right;
    (void)left;
    assert(!"normal Dam intro must not add a dual weapon");
    return 0;
}

void give_cur_player_ammo(s32 ammo_type, s32 ammo_amount)
{
    assert(ammo_call_count < 4);
    ammo_types[ammo_call_count] = ammo_type;
    ammo_amounts[ammo_call_count++] = ammo_amount;
}

s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 next_weapon)
{
    assert(hand == GUNRIGHT || hand == GUNLEFT);
    equipped_weapons[hand] = next_weapon;
    return 0;
}

s32 modelLoad(s32 modelid)
{
    assert(modelid == PROJECTILES_TYPE_BUG);
    projectile_model_load_calls++;
    return 1;
}

typedef struct IntroHarness {
    float level_scale;
    float eye_height;
    uint32_t resolve_calls;
    struct {
        uint32_t id : 24;
        uint8_t room;
        int16_t mid;
        int16_t tail;
    } stan_token;
    PropRecord player_prop;
    GeOriginalPlayerViewState player;
    uint32_t allocation_calls;
    uint32_t activation_calls;
    uint32_t enable_calls;
    uint32_t deregistration_calls;
    uint32_t registration_calls;
    int16_t deregistered_room;
    int16_t registered_room;
    union {
        max_align_t alignment;
        unsigned char bytes[sizeof(DoorRecord)];
    } world_definitions[9];
    PropRecord world_props[9];
    uint32_t world_definition_count;
    uint32_t world_prop_count;
    uint32_t world_room_calls;
} IntroHarness;

static void *allocate_world_definition(void *context, uint8_t type,
                                       size_t size_bytes)
{
    IntroHarness *harness = context;
    (void)type;
    assert(size_bytes <= sizeof(harness->world_definitions[0].bytes));
    assert(harness->world_definition_count < 9U);
    return &harness->world_definitions[harness->world_definition_count++];
}

static void *allocate_world_prop(void *context, void *definition)
{
    IntroHarness *harness = context;
    assert(definition != NULL);
    assert(harness->world_prop_count < 9U);
    return &harness->world_props[harness->world_prop_count++];
}

static void world_prop_service(void *context, void *prop)
{
    (void)context;
    assert(prop != NULL);
}

static void world_room_service(void *context, void *prop, int16_t room)
{
    IntroHarness *harness = context;
    assert(prop != NULL);
    assert(room == 135);
    harness->world_room_calls++;
}

static void *allocate_prop(void *context)
{
    IntroHarness *harness = context;
    harness->allocation_calls++;
    memset(&harness->player_prop, 0, sizeof(harness->player_prop));
    return &harness->player_prop;
}

static void activate_prop(void *context, void *prop)
{
    IntroHarness *harness = context;
    assert(prop == &harness->player_prop);
    harness->activation_calls++;
}

static void enable_prop(void *context, void *prop)
{
    IntroHarness *harness = context;
    assert(prop == &harness->player_prop);
    harness->enable_calls++;
}

static void deregister_room(void *context, void *prop, int16_t room)
{
    IntroHarness *harness = context;
    assert(prop == &harness->player_prop);
    harness->deregistration_calls++;
    harness->deregistered_room = room;
}

static void register_room(void *context, void *prop, int16_t room)
{
    IntroHarness *harness = context;
    assert(prop == &harness->player_prop);
    harness->registration_calls++;
    harness->registered_room = room;
}

static stagesetup *load_setup(void *context, int32_t stage_id)
{
    (void)context;
    return stage_id == LEVELID_DAM ? &UsetupdamZ : NULL;
}

static float get_room_scale_reciprocal(void *context)
{
    IntroHarness *harness = context;
    return 1.0f / harness->level_scale;
}

static void *resolve_stan(void *context, const char *name)
{
    IntroHarness *harness = context;
    assert(name != NULL);
    harness->resolve_calls++;
    return &harness->stan_token;
}

static int32_t get_demo_slot(void *context)
{
    (void)context;
    return 0;
}

static float get_floor_y(void *context, void *stan, float x, float z)
{
    IntroHarness *harness = context;
    const float inverse_scale = 1.0f / harness->level_scale;

    assert(stan == &harness->stan_token);
    assert(fabsf(x - 4719.0f * inverse_scale) < 0.01f);
    assert(fabsf(z - 3949.0f * inverse_scale) < 0.01f);
    return -25.0f * inverse_scale;
}

static float get_eye_height(void *context)
{
    IntroHarness *harness = context;
    return harness->eye_height;
}

static int32_t commit_player_spawn(void *context,
                                   const float position[3],
                                   float floor_y,
                                   float eye_height,
                                   float look_angle_radians,
                                   void *stan)
{
    (void)context;
    GeOriginalPlayerSpawnConfig config = {
        .position = {position[0], position[1], position[2]},
        .floor_y = floor_y,
        .eye_height = eye_height,
        .look_angle_radians = look_angle_radians,
        .stan = stan,
        .room = 135,
    };
    return ge_original_player_spawn_commit(&config);
}

int main(void)
{
    IntroHarness harness = {
        .level_scale = 0.23363999f,
        .eye_height = 159.0f,
    };
    GeOriginalSetupPadProviders setup_providers = {
        .context = &harness,
        .load_setup = load_setup,
        .get_room_scale_reciprocal = get_room_scale_reciprocal,
        .resolve_stan = resolve_stan,
    };
    GeOriginalIntroProviders intro_providers = {
        .context = &harness,
        .get_demo_slot = get_demo_slot,
        .get_floor_y = get_floor_y,
        .get_eye_height = get_eye_height,
        .commit_player_spawn = commit_player_spawn,
    };
    GeOriginalPlayerSpawnProviders player_providers = {
        .context = &harness,
        .allocate_prop = allocate_prop,
        .activate_prop = activate_prop,
        .enable_prop = enable_prop,
        .deregister_room = deregister_room,
        .register_room = register_room,
    };
    GeOriginalDamWorldProviders world_providers = {
        .context = &harness,
        .allocate_definition = allocate_world_definition,
        .allocate_prop = allocate_world_prop,
        .activate_prop = world_prop_service,
        .enable_prop = world_prop_service,
        .register_room = world_room_service,
    };
    GeOriginalDamWorldState world_state;
    PropDefHeaderRecord size_header = {0};
    GeOriginalSetupPadState setup_state;
    GeOriginalIntroSpawnState spawn_state;
    GeOriginalIntroLoadoutState loadout_state;
    const float inverse_scale = 1.0f / harness.level_scale;
    const float expected_y = harness.eye_height - 25.0f * inverse_scale;

    harness.stan_token.room = 135;
    ge_original_setup_pad_bind(&setup_providers, &setup_state);
    ge_original_setup_pad_load(LEVELID_DAM);

    assert(setup_state.loaded);
    assert(setup_state.stage_id == LEVELID_DAM);
    assert(setup_state.pad_count > 270U);
    assert(setup_state.resolved_pad_count == setup_state.pad_count);
    assert(harness.resolve_calls > setup_state.pad_count);
    assert(fabsf(setup_state.room_scale_reciprocal - inverse_scale) < 0.0001f);

    ge_dam_setup_world_materializer_bind(&world_providers, &world_state);
    ge_dam_setup_world_materialize_first_authored();
    size_header.type = PROPDEF_WATCH_MENU_OBJECTIVE_TEXT;
    assert(sizepropdef(&size_header) == 4);
    assert(world_state.loaded);
    assert(world_state.definitions_materialized == 3U);
    assert(world_state.rooms_registered == 3U);
    assert(world_state.first_glass.command_index == 107);
    assert(world_state.first_glass.propdef_type == PROPDEF_GLASS);
    assert(world_state.first_glass.pad_id == 10076);
    assert(world_state.first_glass.stan == UsetupdamZ.boundpads[76].stan);
    assert(fabsf(world_state.first_glass.position[0]
                 - UsetupdamZ.boundpads[76].pos.f[0]) < 0.001f);
    assert(fabsf(world_state.first_glass.position[1]
                 - UsetupdamZ.boundpads[76].pos.f[1]) < 0.001f);
    assert(fabsf(world_state.first_glass.position[2]
                 - UsetupdamZ.boundpads[76].pos.f[2]) < 0.001f);
    assert(world_state.first_object.command_index == 122);
    assert(world_state.first_object.propdef_type == PROPDEF_PROP);
    assert(world_state.first_object.object_id == 62);
    assert(world_state.first_object.pad_id == 358);
    assert(world_state.first_door.command_index == 267);
    assert(world_state.first_door.propdef_type == PROPDEF_DOOR);
    assert(world_state.first_door.object_id == 178);
    assert(world_state.first_door.pad_id == 6);
    assert(world_state.first_glass.room == 135);
    assert(world_state.first_object.room == 135);
    assert(world_state.first_door.room == 135);
    assert(harness.world_definition_count == 3U);
    assert(harness.world_prop_count == 3U);
    assert(harness.world_room_calls == 3U);
    assert(ge_dam_setup_world_materialize_linked_door());
    assert(world_state.second_door.command_index == 268);
    assert(world_state.second_door.propdef_type == PROPDEF_DOOR);
    assert(world_state.second_door.object_id == 178);
    assert(world_state.second_door.pad_id == 9);
    assert(world_state.second_door.room == 135);
    assert(world_state.definitions_materialized == 4U);
    assert(harness.world_definition_count == 4U);
    assert(harness.world_prop_count == 4U);
    assert(harness.world_room_calls == 4U);
    assert(ge_dam_setup_world_link_authored_doors());
    assert(ge_dam_setup_world_materialize_spawn_windows()
           == GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT);
    for (size_t index = 0U;
            index < GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT; ++index) {
        const GeOriginalDamWorldEntry *window =
            &world_state.spawn_windows[index];
        assert(window->command_index == (int32_t)(117U + index));
        assert(window->propdef_type == PROPDEF_GLASS);
        assert(window->object_id == 104);
        assert(window->pad_id == (int16_t)(10090U + index));
        assert(window->stan == UsetupdamZ.boundpads[90U + index].stan);
        assert(fabsf(window->position[0]
                     - UsetupdamZ.boundpads[90U + index].pos.f[0]) < 0.001f);
        assert(fabsf(window->position[1]
                     - UsetupdamZ.boundpads[90U + index].pos.f[1]) < 0.001f);
        assert(fabsf(window->position[2]
                     - UsetupdamZ.boundpads[90U + index].pos.f[2]) < 0.001f);
    }
    assert(world_state.definitions_materialized == 9U);
    assert(harness.world_definition_count == 9U);
    assert(harness.world_prop_count == 9U);
    assert(harness.world_room_calls == 9U);
    assert(ge_dam_setup_world_materialize_spawn_windows()
           == GE_ORIGINAL_DAM_SPAWN_WINDOW_COUNT);
    assert(harness.world_definition_count == 9U);

    ge_original_player_spawn_bind(&player_providers, &harness.player);
    ge_original_bond_intro_bind(&intro_providers, &spawn_state);
    bondviewLoadSetupIntroSpawnSlice();

    assert(spawn_state.loaded);
    assert(spawn_state.matching_spawn_count == 1U);
    assert(spawn_state.pad_index == 33);
    assert(strcmp(spawn_state.stan_name, "p6g1") == 0);
    assert(spawn_state.stan == &harness.stan_token);
    assert(fabsf(spawn_state.position[0] - 4719.0f * inverse_scale) < 0.01f);
    assert(fabsf(spawn_state.position[1] - expected_y) < 0.01f);
    assert(fabsf(spawn_state.position[2] - 3949.0f * inverse_scale) < 0.01f);
    assert(fabsf(spawn_state.floor_y - (-25.0f * inverse_scale)) < 0.01f);
    assert(spawn_state.look_angle_degrees > 449.9f);
    assert(spawn_state.look_angle_degrees < 450.2f);
    assert(spawn_state.player_committed);
    assert(g_bondviewBondDeathAnimationsCount == 12);
    assert(harness.player.initialized);
    assert(harness.player.publication_generation == 1U);
    assert(harness.player.current_stan == &harness.stan_token);
    assert(harness.player.portal_stan == &harness.stan_token);
    assert(harness.player.prop == &harness.player_prop);
    assert(harness.player.room == 135);
    assert(fabsf(harness.player.collision_radius - 30.0f) < 0.001f);
    assert(fabsf(harness.player.collision_position[0]
                 - spawn_state.position[0]) < 0.01f);
    assert(fabsf(harness.player.camera_position[1]
                 - spawn_state.position[1]) < 0.01f);
    assert(fabsf(harness.player.heading[0] + 1.0f) < 0.001f);
    assert(fabsf(harness.player.heading[1]) < 0.001f);
    assert(fabsf(harness.player.heading[2]) < 0.001f);
    assert(fabsf(harness.player.camera_look[0] - 1.0f) < 0.001f);
    assert(fabsf(harness.player.camera_up[1] - 1.0f) < 0.001f);
    assert(harness.allocation_calls == 1U);
    assert(harness.activation_calls == 1U);
    assert(harness.enable_calls == 1U);
    assert(harness.deregistration_calls == 1U);
    assert(harness.registration_calls == 1U);
    assert(harness.deregistered_room == -1);
    assert(harness.registered_room == 135);
    assert(harness.player_prop.type == PROP_TYPE_VIEWER);
    assert(harness.player_prop.obj == NULL);
    assert(harness.player_prop.stan == (StandTile *)&harness.stan_token);
    assert(fabsf(harness.player_prop.pos.x
                 - spawn_state.position[0]) < 0.01f);

    assert(bondviewLoadSetupIntroLoadoutSlice(&loadout_state));
    assert(loadout_state.loaded);
    assert(loadout_state.item_records == 2U);
    assert(loadout_state.ammo_records == 2U);
    assert(loadout_state.projectile_model_requests == 2U);
    assert(loadout_state.starting_weapon[GUNRIGHT] == ITEM_WPPKSIL);
    assert(loadout_state.starting_weapon[GUNLEFT] == ITEM_UNARMED);
    assert(loadout_state.bondtype == CUFF_BOILER);
    assert(inventory_reset_calls == 1);
    assert(added_item_count == 3);
    assert(added_items[0] == ITEM_WPPKSIL);
    assert(added_items[1] == ITEM_BUG);
    assert(added_items[2] == ITEM_FIST);
    assert(ammo_call_count == 2);
    assert(ammo_types[0] == AMMO_9MM);
    assert(ammo_amounts[0] == 100);
    assert(ammo_types[1] == AMMO_BUG);
    assert(ammo_amounts[1] == 1);
    assert(equipped_weapons[GUNRIGHT] == ITEM_WPPKSIL);
    assert(equipped_weapons[GUNLEFT] == ITEM_UNARMED);
    assert(projectile_model_load_calls == 1);

    puts("decompiled Dam intro/player/loadout: ok (pad 33, silenced PP7, bug)");
    return 0;
}
