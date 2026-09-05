#!/usr/bin/env python3
"""Pin the decompiled startup/menu music sequence to live CSeq switching."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
FRONTEND = (ROOT / "port/src/ge_original_frontend_start.c").read_text()
MAIN = (ROOT / "platform/3ds/source/main.c").read_text()


assert "frontend->current_menu==MENU_LEGAL_SCREEN" in FRONTEND
assert "frontend->services.stop_music(frontend->services.context)" in FRONTEND
assert "frontend->current_menu==MENU_NINTENDO_LOGO" in FRONTEND
assert "frontend->services.context,M_INTROSWOOSH" in FRONTEND
assert "frontend->current_menu==MENU_EYE_INTRO" in FRONTEND
assert "frontend->services.context,M_INTRO" in FRONTEND
assert "frontend->services.context,M_FOLDERS" in FRONTEND

assert ".stop_music = original_frontend_stop_music" in MAIN
assert "ge_original_music_track_asset_path(" in MAIN
assert "GeOriginalMusicRuntime **music_runtime" in MAIN
assert "run_start_frontend\n            ? NULL" in MAIN

# Diagnostic boots enter gameplay directly. They must make that decision
# before music initialization; otherwise every input/visual probe silently
# omits synthesis and understates the gameplay frame budget.
startup = MAIN[MAIN.index("start_stage_runtime:"):]
decision = "run_start_frontend = run_start_frontend\n        && !runtime_diagnostics_requested(stage_assets);"
assert startup.index(decision) < startup.index("ge_audio_output_init(")
assert "if (run_start_frontend) {" in startup
assert startup.count("runtime_diagnostics_requested(stage_assets)") == 1

print("canonical frontend music switching test passed")
