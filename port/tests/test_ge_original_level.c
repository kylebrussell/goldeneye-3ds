#include "ge_original_level.h"

#include <assert.h>
#include <stddef.h>

typedef struct ProviderState {
    int32_t paused;
    int32_t tlb_resets;
    int32_t pause_queries;
    uint16_t buttons_pressed;
    GeOriginalLevelSubsystem events[96];
    int32_t event_count;
} ProviderState;

static int32_t test_is_paused(void *context)
{
    ProviderState *state = context;
    state->pause_queries++;
    return state->paused;
}

static void test_reset_tlb(void *context)
{
    ProviderState *state = context;
    state->tlb_resets++;
}

static uint16_t test_buttons_pressed(void *context)
{
    ProviderState *state = context;
    return state->buttons_pressed;
}

static void test_tick_subsystem(GeOriginalLevelSubsystem subsystem, void *context)
{
    ProviderState *state = context;
    assert(state->event_count < (int32_t)(sizeof(state->events) / sizeof(state->events[0])));
    state->events[state->event_count++] = subsystem;
}

static void expect_world_tick_order(const ProviderState *state,
                                    int32_t first_event,
                                    int32_t includes_initial_cheats)
{
    int32_t i;
    int32_t event = first_event;

    if (includes_initial_cheats) {
        assert(state->events[event++] == GE_ORIGINAL_LEVEL_SUBSYSTEM_INITIAL_CHEATS);
    }

    for (i = GE_ORIGINAL_LEVEL_SUBSYSTEM_VI_ZBUF;
         i <= GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE;
         i++) {
        assert(state->events[event++] == (GeOriginalLevelSubsystem)i);
    }
    assert(state->event_count == event);
}

static void expect_timer(const GeOriginalLevelTimerState *timer,
                         int32_t locked,
                         int32_t delta,
                         int32_t global,
                         int32_t active)
{
    assert(timer->controls_locked == locked);
    assert(timer->clock_timer == delta);
    assert(timer->global_timer_delta == (float)delta);
    assert(timer->global_timer == global);
    assert(timer->active_frame_updates == active);
}

int main(void)
{
    ProviderState provider_state = {0};
    GeOriginalLevelProviders providers = {
        .is_paused = test_is_paused,
        .reset_tlb_entries = test_reset_tlb,
        .context = &provider_state,
        .buttons_pressed = test_buttons_pressed,
        .tick_subsystem = test_tick_subsystem,
    };
    GeOriginalLevelTimerState timer;

    ge_original_level_init(&providers);
    ge_original_level_set_stage(33);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 0, 0, 0, 0);

    ge_original_level_tick(2);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 0, 2, 2, 1);
    assert(provider_state.tlb_resets == 1);
    assert(provider_state.pause_queries == 1);
    expect_world_tick_order(&provider_state, 0, 1);

    ge_original_level_timer_tick(3);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 0, 3, 5, 2);
    expect_world_tick_order(&provider_state, GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE + 1, 0);

    ge_original_level_set_controls_locked(1);
    ge_original_level_timer_tick(4);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 1, 0, 5, 2);
    assert(provider_state.tlb_resets == 3);
    /* The original short-circuit does not query pause while controls are locked. */
    assert(provider_state.pause_queries == 2);
    expect_world_tick_order(&provider_state,
                            GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE * 2 + 1,
                            0);

    ge_original_level_set_controls_locked(0);
    provider_state.paused = 1;
    ge_original_level_timer_tick(4);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 0, 0, 5, 2);
    assert(provider_state.tlb_resets == 4);
    assert(provider_state.pause_queries == 3);
    expect_world_tick_order(&provider_state,
                            GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE * 3 + 1,
                            0);

    provider_state.paused = 0;
    ge_original_level_timer_tick(0);
    ge_original_level_timer_snapshot(&timer);
    /* D_80048380 counts active updates even when speedgraphframes is zero. */
    expect_timer(&timer, 0, 0, 5, 3);
    expect_world_tick_order(&provider_state,
                            GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE * 4 + 1,
                            0);

    /* The title branch keeps VI setup but does not tick gameplay subsystems. */
    ge_original_level_set_stage(90);
    ge_original_level_tick(1);
    assert(provider_state.events[provider_state.event_count - 4] ==
           GE_ORIGINAL_LEVEL_SUBSYSTEM_VI_ZBUF);
    assert(provider_state.events[provider_state.event_count - 3] ==
           GE_ORIGINAL_LEVEL_SUBSYSTEM_TITLE_CHEATS);
    assert(provider_state.events[provider_state.event_count - 2] ==
           GE_ORIGINAL_LEVEL_SUBSYSTEM_TITLE_MENU);
    assert(provider_state.events[provider_state.event_count - 1] ==
           GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE);
    assert(provider_state.event_count ==
           GE_ORIGINAL_LEVEL_SUBSYSTEM_LANGUAGE * 5 + 5);

    /* A button edge clears the original inactivity timer. */
    ge_original_level_set_stage(33);
    provider_state.buttons_pressed = 1;
    ge_original_level_tick(1);
    ge_original_level_timer_snapshot(&timer);
    assert(timer.stage_id == 33);
    assert(timer.idle_frames == 0);
    assert(timer.idle_latched == 0);
    assert(timer.multiplayer_timer == 7);
    assert(timer.active_stage_frames == 7);
    assert(timer.stage_seconds == 7.0f / 60.0f);

    /* Preserve the original 30-second inactivity latch and stage-time freeze. */
    provider_state.buttons_pressed = 0;
    provider_state.event_count = 0;
    ge_original_level_init(&providers);
    ge_original_level_set_stage(33);
    ge_original_level_tick(0x707);
    ge_original_level_timer_snapshot(&timer);
    assert(timer.idle_frames == 0x707);
    assert(timer.idle_latched == 0);
    assert(timer.active_stage_frames == 0x707);
    ge_original_level_tick(1);
    ge_original_level_timer_snapshot(&timer);
    assert(timer.idle_frames == 0x708);
    assert(timer.idle_latched == 1);
    assert(timer.active_stage_frames == 0x707);

    ge_original_level_timer_snapshot(NULL);
    ge_original_level_init(NULL);
    ge_original_level_timer_tick(1);
    ge_original_level_timer_snapshot(&timer);
    expect_timer(&timer, 0, 1, 1, 1);

    return 0;
}
