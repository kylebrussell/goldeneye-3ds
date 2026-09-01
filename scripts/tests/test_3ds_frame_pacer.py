#!/usr/bin/env python3
"""Keep presentation nonblocking while the exact retrace scheduler owns 60 Hz."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / "platform/3ds/source/main.c").read_text()


def main() -> None:
    # The frontend deliberately waits for a VI after its short standalone
    # frame. Anchor this audit to the live gameplay loop, whose presentation
    # must remain nonblocking while the exact retrace scheduler owns cadence.
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
    print("3DS frame pacing: nonblocking presentation, canonical 60 Hz retraces")


if __name__ == "__main__":
    main()
