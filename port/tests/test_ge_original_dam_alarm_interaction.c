#include "ge_asset_pack.h"
#include "ge_original_stage_alarm_interaction.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"

#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

s32 alarm_timer;
static size_t linked_switch_calls;
static unsigned interaction_events[8];
static size_t interaction_event_count;

enum {
    EVENT_SFX = 1U,
    EVENT_IS_ACTIVE,
    EVENT_ACTIVATE,
    EVENT_DEACTIVATE,
    EVENT_PICKUP,
    EVENT_LINKED_SWITCH
};

static void record_event(unsigned event)
{
    assert(interaction_event_count < sizeof(interaction_events)
           / sizeof(interaction_events[0]));
    interaction_events[interaction_event_count++] = event;
}

void alarmActivate(void)
{
    record_event(EVENT_ACTIVATE);
    if (alarm_timer < 1) alarm_timer = 1;
}

void alarmDeactivate(void)
{
    record_event(EVENT_DEACTIVATE);
    alarm_timer = 0;
}

bool alarmIsActive(void)
{
    record_event(EVENT_IS_ACTIVE);
    return alarm_timer > 0;
}

void sub_GAME_7F03E6A0(PropRecord *prop)
{
    assert(prop != NULL && prop->obj != NULL);
    record_event(EVENT_LINKED_SWITCH);
    ++linked_switch_calls;
}

typedef struct AlarmHarness {
    size_t sfx_calls;
    size_t pickup_calls;
    uint32_t last_sfx;
    void *last_prop;
} AlarmHarness;

static void play_sfx(void *context, uint32_t sfx_id)
{
    AlarmHarness *harness = context;
    record_event(EVENT_SFX);
    ++harness->sfx_calls;
    harness->last_sfx = sfx_id;
}

static int pickup(void *context, void *prop, int immediate)
{
    AlarmHarness *harness = context;
    assert(immediate == TRUE);
    record_event(EVENT_PICKUP);
    ++harness->pickup_calls;
    harness->last_prop = prop;
    return TICKOP_GIVETOPLAYER;
}

int main(int argc, char **argv)
{
    static const size_t alarm_commands[4] = {310U, 312U, 314U, 316U};
    static const int32_t alarm_pads[4] = {10070, 10071, 10072, 10074};
    GeAssetPack pack;
    GeOriginalStageSetupRuntime setup;
    const GeStageAssetDescriptor *dam;
    AlarmHarness harness = {0};
    GeOriginalStageAlarmInteractionProviders providers = {
        .context = &harness,
        .play_sfx = play_sfx,
        .pickup_by_player = NULL,
    };
    size_t index;

    assert(argc == 2);
    dam = ge_stage_asset_descriptor_by_key("dam");
    assert(dam != NULL && ge_asset_pack_open(&pack, argv[1])
            == GE_ASSET_PACK_OK);
    assert(ge_original_stage_setup_load(&pack, dam, &setup)
           == GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_prop_type_count(&setup, PROPDEF_ALARM)
           == 4U);
    for (index = 0U; index < 4U; ++index) {
        const size_t command_index = alarm_commands[index];
        const GeOriginalStagePropRecord *record =
            &setup.prop_records[command_index];
        GeOriginalStagePropConstructionRequest request;
        size_t definition_size;
        ObjectRecord *object;
        PropRecord prop;
        GeOriginalStageAlarmInteractionResult result;
        uint32_t before_bits;

        assert(record->type == PROPDEF_ALARM
               && record->pad_id == alarm_pads[index]
               && (record->words[2] & PROPFLAG_00080000) == 0U
               && ge_original_stage_prop_construction_request(
                    &setup, command_index, &request));
        definition_size =
            ge_original_stage_prop_native_definition_size(&request);
        assert(definition_size == sizeof(ObjectRecord));
        object = calloc(1U, definition_size);
        assert(object != NULL
               && ge_original_stage_prop_native_definition_init(
                    &request, object, definition_size));
        memset(&prop, 0, sizeof(prop));
        prop.obj = object;
        object->prop = &prop;

        alarm_timer = 0;
        interaction_event_count = 0U;
        before_bits = object->runtime_bitflags;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK);
        assert(result.was_active == 0U && result.is_active == 1U
               && result.tick_operation == TICKOP_NONE
               && result.pickup_requested == 0U
               && result.activation_published == 1U
               && result.sfx_id == (uint32_t)ALARM_SWITCH_SFX
               && alarm_timer == 1
               && (object->runtime_bitflags
                    & GE_ORIGINAL_STAGE_ALARM_RUNTIME_ACTIVATED) != 0U
               && (object->runtime_bitflags
                    & ~GE_ORIGINAL_STAGE_ALARM_RUNTIME_ACTIVATED)
                    == before_bits);
        assert(interaction_event_count == 4U
               && interaction_events[0] == EVENT_SFX
               && interaction_events[1] == EVENT_IS_ACTIVE
               && interaction_events[2] == EVENT_ACTIVATE
               && interaction_events[3] == EVENT_LINKED_SWITCH);
        interaction_event_count = 0U;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK);
        assert(result.was_active == 1U && result.is_active == 0U
               && alarm_timer == 0);
        assert(interaction_event_count == 4U
               && interaction_events[0] == EVENT_SFX
               && interaction_events[1] == EVENT_IS_ACTIVE
               && interaction_events[2] == EVENT_DEACTIVATE
               && interaction_events[3] == EVENT_LINKED_SWITCH);
        free(object);
    }
    assert(harness.sfx_calls == 8U
           && harness.last_sfx == (uint32_t)ALARM_SWITCH_SFX
           && harness.pickup_calls == 0U && linked_switch_calls == 8U);

    /* ai_21's two authored backup terminals are ordinary props, not alarms.
     * The exact common propobjInteract tail must publish ACTIVATED for both
     * without toggling alarm state or requesting a platform service. */
    for (index = 0U; index < 2U; ++index) {
        static const size_t terminal_commands[2] = {262U, 264U};
        const GeOriginalStagePropRecord *record =
            &setup.prop_records[terminal_commands[index]];
        GeOriginalStagePropConstructionRequest request;
        GeOriginalStageAlarmInteractionResult result;
        ObjectRecord *object;
        PropRecord prop;
        size_t definition_size;
        assert(record->type == PROPDEF_PROP
            && (record->words[2] & PROPFLAG_00080000) == 0U
            && ge_original_stage_prop_construction_request(
                &setup, terminal_commands[index], &request));
        definition_size =
            ge_original_stage_prop_native_definition_size(&request);
        object = calloc(1U, definition_size);
        assert(object != NULL
            && ge_original_stage_prop_native_definition_init(
                &request, object, definition_size));
        memset(&prop, 0, sizeof(prop));
        prop.obj = object;
        object->prop = &prop;
        alarm_timer = 0;
        interaction_event_count = 0U;
        assert(ge_original_stage_object_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK);
        assert(result.sfx_id == 0U && result.was_active == 0U
            && result.is_active == 0U
            && result.tick_operation == TICKOP_NONE
            && result.pickup_requested == 0U
            && result.activation_published == 1U
            && alarm_timer == 0
            && (object->runtime_bitflags
                & GE_ORIGINAL_STAGE_ALARM_RUNTIME_ACTIVATED) != 0U);
        assert(interaction_event_count == 1U
            && interaction_events[0] == EVENT_LINKED_SWITCH);
        free(object);
    }
    assert(linked_switch_calls == 10U);

    /* Preserve the common propobjInteract pickup branch even though none of
     * Dam's four authored alarm switches sets it. Missing services are
     * rejected before SFX, alarm state, or activation bits mutate. */
    {
        ObjectRecord object;
        PropRecord prop;
        GeOriginalStageAlarmInteractionResult result;
        memset(&object, 0, sizeof(object));
        memset(&prop, 0, sizeof(prop));
        object.type = PROPDEF_ALARM;
        object.flags = PROPFLAG_00080000;
        object.prop = &prop;
        prop.obj = &object;
        alarm_timer = 0;
        interaction_event_count = 0U;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_PICKUP_SERVICE);
        assert(alarm_timer == 0 && object.runtime_bitflags == 0U
               && harness.sfx_calls == 8U
               && interaction_event_count == 0U);
        providers.pickup_by_player = pickup;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK);
        assert(result.tick_operation == TICKOP_GIVETOPLAYER
               && result.pickup_requested == 1U
               && harness.pickup_calls == 1U && harness.last_prop == &prop);
        assert(interaction_event_count == 5U
               && interaction_events[0] == EVENT_SFX
               && interaction_events[1] == EVENT_IS_ACTIVE
               && interaction_events[2] == EVENT_ACTIVATE
               && interaction_events[3] == EVENT_PICKUP
               && interaction_events[4] == EVENT_LINKED_SWITCH);
        providers.play_sfx = NULL;
        alarm_timer = 0;
        object.runtime_bitflags = 0U;
        interaction_event_count = 0U;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_MISSING_SFX_SERVICE);
        assert(alarm_timer == 0 && object.runtime_bitflags == 0U);
        assert(interaction_event_count == 0U);
        object.type = PROPDEF_PROP;
        assert(ge_original_stage_alarm_interact_exact(
            &prop, &providers, &result)
            == GE_ORIGINAL_STAGE_ALARM_INTERACTION_NOT_ALARM);
    }
    assert(strcmp(ge_original_stage_alarm_interaction_status_name(
        GE_ORIGINAL_STAGE_ALARM_INTERACTION_OK), "ok") == 0);
    ge_original_stage_setup_close(&setup);
    ge_asset_pack_close(&pack);
    puts("Dam canonical alarm interaction passed");
    return 0;
}
