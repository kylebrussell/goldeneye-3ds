#!/usr/bin/env python3
"""Keep canonical retraces while preventing duplicate submissions per VBlank."""

from pathlib import Path
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "platform/3ds/source/main.c").read_text()


def main() -> None:
    # The frontend deliberately waits for a VI after its short standalone
    # frame. Anchor this audit to the live gameplay loop.
    gameplay_loop = SOURCE.index("simulation_start = osGetTime();")
    frame_end = SOURCE.index("C3D_FrameEnd(0);", gameplay_loop)
    result_write = SOURCE.index("write_input_probe_result(", frame_end)
    pace_block = SOURCE[frame_end:result_write]
    assert "svcSleepThread" not in pace_block
    assert "gspWaitForVBlank" not in pace_block
    assert "SYSCLOCK_ARM11" not in pace_block
    assert "ge_retrace_scheduler_pump" in SOURCE
    sample = SOURCE[SOURCE.index("static GePortInput input_probe_sample"):
                    SOURCE.index("static const GeStageAssetDescriptor")]
    assert "input_probe_route_frame(runtime)" in sample
    assert "runtime->displayed_frames % target->pulse_period" not in sample
    assert "input_probe_route_frame(&input_probe)" in SOURCE
    assert '"level_id=%ld\\n"' in SOURCE
    assert "if (dam_stage && !visual_probe_tour.enabled\n" not in SOURCE
    assert "if (!dam_stage && input_probe.target_count != 0U)" in SOURCE
    idle_gate = SOURCE.index("if (ticks == 0U && !original_frontend_runtime.ramrom_active)",
        gameplay_loop)
    gameplay_begin = SOURCE.index("C3D_FrameBegin(0);", gameplay_loop)
    assert SOURCE.index("ge_port_advance_retraces(", gameplay_loop) < idle_gate
    assert SOURCE.index("ge_3ds_audio_pump();", gameplay_loop) < idle_gate < gameplay_begin
    assert "svcSleepThread(1000000LL);" in SOURCE[idle_gate:gameplay_begin]
    assert "continue;" in SOURCE[idle_gate:gameplay_begin]
    # Execute the actual pacing block with a simulated display clock. In
    # particular, an already-late frame must not wait for yet another VBlank.
    start = SOURCE.index("if (!original_frontend_runtime.ramrom_active) {", gameplay_begin)
    end = SOURCE.index("fine_start = svcGetSystemTick();", start)
    pacing = SOURCE[start:end]
    assert "timing->retraces = elapsed_retraces;" in SOURCE
    harness = r'''
#include <stdint.h>
#include <assert.h>
static uint32_t counter, waits;
static uint64_t svcGetSystemTick(void) { return 0; }
static uint32_t C3D_FrameCounter(int id) { assert(id == 0); return counter; }
static void gspWaitForVBlank(void) { ++counter; ++waits; }
static void check(uint32_t previous, uint32_t current, int ramrom,
                  uint32_t expected_waits, uint32_t expected_counter) {
    struct { int ramrom_active; } original_frontend_runtime = {ramrom};
    uint32_t last_submit_vblank = previous;
    uint64_t present_wait_ticks = 0;
    counter = current; waits = 0;
''' + pacing + r'''
    assert(waits == expected_waits && counter == expected_counter);
    assert(last_submit_vblank == counter);
    (void)present_wait_ticks;
}
int main(void) {
    check(12, 12, 0, 1, 13); /* short work: wait for a new interval */
    check(12, 13, 0, 0, 13); /* one interval already passed */
    check(12, 15, 0, 0, 15); /* overloaded: don't add another delay */
    check(UINT32_MAX, UINT32_MAX, 0, 1, 0); /* counter wraps */
    check(UINT32_MAX, 0, 0, 0, 0);
    check(12, 12, 1, 0, 12); /* RAMROM owns presentation cadence */
}
'''
    with tempfile.TemporaryDirectory(prefix="ge-pacer-") as temporary:
        directory = Path(temporary)
        (directory / "pacer.c").write_text(harness)
        subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                        str(directory / "pacer.c"), "-o", str(directory / "pacer")], check=True)
        subprocess.run([str(directory / "pacer")], check=True)
    print("3DS frame pacing: one submission per VBlank, late-frame/RAMROM bypass, canonical retraces")


if __name__ == "__main__":
    main()
