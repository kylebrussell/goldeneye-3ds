#!/usr/bin/env python3
"""Extract canonical multiplayer bodies, with native pointer-width adaptation."""
from pathlib import Path
import argparse
import hashlib
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

FUNCTIONS = {
    "src/game/ramromreplay.c": ("get_is_ramrom_flag", "get_recording_ramrom_flag", "check_ramrom_flags"),
    "src/game/mpmenu.c": ("get_points_for_mp_player",),
    "src/game/bondview.c": ("bondviewGetRandomSpawnPadIndex", "bondviewPlayerBeginLife"),
    "src/game/bondview2.c": ("mp_respawn_handler", "currentPlayerStartChrFade"),
}

def render(repo):
    text = '''/* Generated canonical multiplayer rules. Do not edit. */
#include <ultra64.h>
#include <stdint.h>
#include <math.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/lv.h"
#include "game/mp_weapon.h"
#include "game/ramromreplay.h"
#include "random.h"
#include "assets/obseg/text/LtitleE.h"
#include "assets/oddtextures.h"
#define BONDVIEW_HUD_MSG_TOP_BUFFER_LENGTH 0x97
#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65
#include "game/bondview_internal.h"
extern stagesetup g_CurrentSetup;
extern u32 check_ramrom_flags(void);
ramromfilestructure *ptr_active_demofile;
s32 is_ramrom_flag;
s32 recording_ramrom_flag;
extern void hudmsgsSetOn(s32);
extern void bondviewClearUpperTextDisplayFlag(s32);
extern void give_cur_player_ammo(s32, s32);
extern void solo_char_load(void);
extern void init_player_BONDdata(void);
extern void currentPlayerStartChrFade(f32, f32);
extern s32 bondviewGetRandomSpawnPadIndex(void);
extern void bondviewPlayerBeginLife(void);
extern void mp_respawn_handler(void);
coord3d ZeroCoordSpawnPos = {0};
PadRecord *g_Startpad[16];
s32 startpadcount;
ITEM_IDS starting_weapon[2];
'''
    for path, names in FUNCTIONS.items():
        source = (repo / path).read_text()
        for name in names:
            body = function_text(source, name)
            text += f"\n/* {path}:{name} source sha256 {hashlib.sha256(body.encode()).hexdigest()} */\n"
            if name == "mp_respawn_handler":
                # The N64 ABI uses 32-bit pointers. Preserve values on 64-bit hosts.
                body = body.replace("s32 start_stan;", "StandTile *start_stan;")
                body = body.replace("(s32)intro_record", "(uintptr_t)intro_record")
            text += body + "\n"
    front = (repo / "src/game/front.c").read_text()
    text += "\n" + extract_data(front, "mp_player_counts") + "\n"
    # The commented-out unsupported arenas remain comments in generated data.
    text += "\n" + extract_data(front, "multi_stage_setups") + "\n"
    setup = (repo / "src/game/chraidata.c").read_text()
    text += "\n" + extract_data(setup, "setup_text_pointers").replace("setup_text_pointers", "ge_mp_setup_text_pointers") + "\n"
    level = (repo / "src/game/lv.c").read_text()
    start = level.index("        // when playing with a kill limit")
    end = level.index("        } // end YOLT", start) + len("        } // end YOLT")
    body = level[start:end]
    text += "\n/* Exact lv.c point-limit and YOLT decisions; terminal actions are returned. */\n"
    text += "int ge_original_mp_end_decision_exact(s32 g_ClockTimer, s32 g_MpPoint)\n{\n    int decision = 0;\n"
    text += "#define mpCalculateAwards(delay) (decision |= 2)\n#define mpwatchSetStopPlayFlag() (decision |= 1)\n"
    text += body + "\n#undef mpCalculateAwards\n#undef mpwatchSetStopPlayFlag\n    return decision;\n}\n"
    return text

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(render(args.repo))
