#include "ge_original_gameplay_services.h"
#include "ge_original_covert_modem_fire.h"
#include "ge_original_pp7_fire.h"
#include <assert.h>
#include <stdio.h>

static unsigned pp7_calls, modem_calls;
GeOriginalPp7FireStatus ge_original_pp7_fire_tick(void)
{ ++pp7_calls; return GE_ORIGINAL_PP7_FIRE_IDLE; }
GeOriginalCovertModemFireStatus ge_original_covert_modem_fire_tick(void)
{ ++modem_calls; return GE_ORIGINAL_COVERT_MODEM_FIRE_IDLE; }

int main(void)
{
    GeAudioOutput output;
    GeOriginalGameplayServiceStats stats;
    int16_t storage[4000];
    assert(ge_audio_output_init(&output, storage, 2000, 12000) == 0);
    ge_original_gameplay_services_bind_audio(NULL, &output);
    ge_original_gameplay_services_set_exact_gun_dispatch(0);
    ge_original_gameplay_services_set_exact_held_item_dispatch(0);
    ge_original_gameplay_services_tick();
    assert(pp7_calls == 1 && modem_calls == 1 && output.frames_written == 200);
    ge_original_gameplay_services_set_exact_gun_dispatch(1);
    ge_original_gameplay_services_tick();
    assert(pp7_calls == 2 && modem_calls == 1 && output.frames_written == 400);
    ge_original_gameplay_services_set_exact_held_item_dispatch(1);
    ge_original_gameplay_services_tick();
    ge_original_gameplay_services_set_exact_gun_dispatch(0);
    ge_original_gameplay_services_tick();
    assert(pp7_calls == 2 && modem_calls == 1 && output.frames_written == 800);
    ge_original_gameplay_services_set_exact_held_item_dispatch(0);
    ge_original_gameplay_services_tick();
    assert(pp7_calls == 3 && modem_calls == 2 && output.frames_written == 1000);
    ge_original_gameplay_services_snapshot(&stats);
    assert(stats.mixed_audio_frames == 1000);
    puts("held items: explicit fire ownership suppresses fallback dispatch while real audio mixer advances");
    return 0;
}
