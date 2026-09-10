#!/usr/bin/env python3
"""Build UI state tests against actual canonical arena/config validation."""
import os
from pathlib import Path
import platform
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from extract_bond_move_explosion_slice import extract_data
from extract_bond_input_live_state_slice import find_function

prefix = r'''
#include "ge_original_multiplayer_rules.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/front.h"
#include "assets/obseg/text/LtitleE.h"
#include "assets/oddtextures.h"
#include <stdio.h>
static char ge_mp_setup_keys[GE_ORIGINAL_MP_ARENAS][32];
'''
front = (ROOT / "src/game/front.c").read_text()
rules = (ROOT / "port/src/ge_original_multiplayer_rules.c").read_text()
source = prefix + extract_data(front, "mp_player_counts") + "\n"
source += extract_data(front, "multi_stage_setups") + "\n"
source += extract_data((ROOT / "src/game/chraidata.c").read_text(), "setup_text_pointers").replace("setup_text_pointers", "ge_mp_setup_text_pointers") + "\n"
source += find_function(rules, "ge_original_multiplayer_arena") + "\n"
source += find_function(rules, "ge_original_multiplayer_validate") + "\n"

with tempfile.TemporaryDirectory(prefix="ge-mp-frontend-") as temporary:
    directory = Path(temporary)
    (directory / "rules.c").write_text(source)
    flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-parameter",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
        "-ffunction-sections", "-fdata-sections", "-D_LANGUAGE_C", "-DVERSION_US",
        "-DBUGFIX_R0", "-DGE_PORT_USE_ORIGINAL_TYPES", "-DAIPARSE", "-DGE_PORT_SETUP_DATA",
        "-DGE_PORT_MS_INHERITS", "-fms-extensions", "-I" + str(ROOT),
        "-I" + str(ROOT / "port/include"), "-idirafter", str(ROOT / "include"),
        "-idirafter", str(ROOT / "include/PR"), "-idirafter", str(ROOT / "src")]
    strip = ["-Wl,-dead_strip"] if platform.system() == "Darwin" else ["-Wl,--gc-sections"]
    subprocess.run([os.environ.get("CC", "cc"), *flags,
        str(directory / "rules.c"), str(ROOT / "port/src/ge_stage_assets.c"),
        str(ROOT / "port/src/ge_original_multiplayer_frontend.c"),
        str(ROOT / "port/tests/test_ge_original_multiplayer_frontend.c"),
        *strip, "-o", str(directory / "test")], check=True)
    subprocess.run([str(directory / "test")], check=True, env={**os.environ,
        "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
        "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1"})
