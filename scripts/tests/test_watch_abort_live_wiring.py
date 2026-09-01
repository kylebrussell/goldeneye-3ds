#!/usr/bin/env python3
"""Pin the native call sites around the tested watch-abort adapter."""

from pathlib import Path


repo = Path(__file__).resolve().parents[2]
source = (repo / "platform/3ds/source/main.c").read_text()


def require(fragment: str, start: int = 0) -> int:
    offset = source.find(fragment, start)
    assert offset >= 0, f"missing watch-abort wiring fragment: {fragment}"
    return offset


require('#include "ge_original_watch_mission_abort_services.h"')
require("set_missionstate(MISSION_STATE_0);")
require("bossRunTitleStage();")
require("mission_failed_or_aborted = TRUE;")
require("ge_original_gameplay_services_play_sfx(CAMERA_BEEP1_SFX);")
require("ge_3ds_save_provider_persist_settings(")
normal_watch_bind = require(
    "ge_original_gameplay_services_bind_settings_persistence(")
normal_watch_unbind = require(
    "ge_original_gameplay_services_bind_settings_persistence(NULL, NULL);",
    normal_watch_bind,
)
save_close = require("ge_3ds_save_provider_close(", normal_watch_unbind)
assert normal_watch_bind < normal_watch_unbind < save_close

# Exact fileSaveSettingsForFolder packing remains data-driven by canonical
# getters/provider state; all nine flag groups and both >>7 volume reductions
# must reach the durable slot owner.
for option in (
    "GE_3DS_SAVE_OPTION_INVERTLOOK",
    "GE_3DS_SAVE_OPTION_AUTOAIM",
    "GE_3DS_SAVE_OPTION_AIMCONTROL",
    "GE_3DS_SAVE_OPTION_SIGHTONSCREEN",
    "GE_3DS_SAVE_OPTION_LOOKAHEAD",
    "GE_3DS_SAVE_OPTION_DISPLAYAMMO",
    "GE_3DS_SAVE_OPTION_SCREENWIDE",
    "GE_3DS_SAVE_OPTION_SCREENCINEMA",
    "GE_3DS_SAVE_OPTION_SCREENRATIO",
    "GE_3DS_SAVE_OPTION_CONTROLTYPE",
):
    require(option)
require("(uint8_t)(get_mTrack2Vol() >> 7)")
require("(uint8_t)(call_sndGetSfxSlotFirstNaturalVolume() >> 7)")

bind = require("ge_original_watch_mission_abort_services_bind(")
reset = require("ge_original_watch_mission_abort_reset(")
assert bind < reset

# The retained MoveBond/options navigation runs first. The missing
# draw_watch_current_page input-owned phase follows only at the fully-open
# mission-status page and precedes post-MoveBond mission-exit processing.
move = require("(void)ge_original_bond_move_live_tick(")
gate_state = require("== WATCH_ANIMATION_0x5", move)
gate_page = require(
    "== GE_ORIGINAL_WATCH_ABORT_MISSION_STATUS_PAGE", gate_state
)
stick = require("ge_original_input_read_bond_frame(", gate_page)
frame = require("ge_original_watch_mission_abort_frame_tick(", stick)
pressed = require("port.original_buttons_pressed", frame)
held = require("port.original_buttons", pressed + len("port.original_buttons_pressed"))
exit_tail = require("ge_original_dam_mission_exit_process_input_exact(", held)
assert move < gate_state < gate_page < stick < frame < pressed < held < exit_tail

# Confirm remains adapter-owned until the original GBI watch page is linked;
# only the focus bit is shared with the already-retained navigation body.
share_in = require("watch_abort.item_selected =", gate_page)
share_out = require("watch_item_is_actively_selected =", frame)
assert share_in < frame < share_out

print(
    "Watch abort live wiring: exact owner order, save packing and "
    "MoveBond->presentation->exit placement pinned"
)
