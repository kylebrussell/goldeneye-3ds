#!/usr/bin/env python3
"""Exercise the live inventory boundary with unchanged canonical bodies."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from extract_bond_input_live_state_slice import find_function

FUNCTIONS = {
    "src/game/bondinv.c": (
        "bondinvReinitInv", "bondinvSortInv", "bondinvInsertItem",
        "bondinvRemoveItem", "bondinvGetNextAvailItem", "bondinvGetInvItem",
        "bondinvHasInvItem", "bondinvGetDualWeapon", "bondinvHasDualWeapon",
        "bondinvItemAvailable", "bondinvAddInvItem", "bondinvAddDoublesInvItem",
        "bondinvRemoveItemByID", "bondinvAddPropToInv", "bondinvCycleForward",
        "bondinvCycleBackward", "bondinvCountTotalItemsInInv",
        "bondinvGetItemByIndex", "bondinvGetTextbyObj", "bondinvGetTextbyWeaponID",
        "bondinvGetTextbyInvIndex", "bondinvGetNameByIndex",
        "bondinvDetermineEquippedItem", "bondinvSetCurEquippedItem",
    ),
    "src/game/gun.c": (
        "getCurrentPlayerWeaponId", "get_next_weapon_in_cycle_for_hand",
        "gunRequestHandWeaponChange", "currentPlayerUnEquipWeaponWrapper",
        "sub_GAME_7F05FB00", "advance_through_inventory", "backstep_through_inventory",
    ),
    "src/game/options.c": ("sub_GAME_7F0A8378",),
}

prefix = r'''
#include <ultra64.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondinv.h"
#include "game/gun.h"
#include "game/language.h"
#include "joy.h"
#include "snd.h"
#include "os_extension.h"
extern struct player *g_CurrentPlayer;
extern int g_curWatchItemIndex, D_800409C4;
extern ALBank *g_musicSfxBufferPtr;
extern int getPlayerCount(void);
extern s32 get_ammo_type_for_weapon(ITEM_IDS weapon);
extern void sub_GAME_7F09B368(GUNHAND hand);
extern void place_item_in_hand_swap_and_make_visible(GUNHAND hand, ITEM_IDS weapon);
extern void objFreePermanently(ObjectRecord *obj, s32 free_prop);
extern void bondinvDetermineEquippedItem(void);
extern void bondinvSetCurEquippedItem(int index);
extern s32 bondinvGetTextbyInvIndex(s32 index);
extern void sub_GAME_7F05FB00(GUNHAND hand);
'''

with tempfile.TemporaryDirectory(prefix="ge-inventory-") as temporary:
    path = Path(temporary)
    original = path / "original.c"
    original.write_text(prefix + "\n".join(
        find_function((ROOT / source).read_text(), name)
        for source, names in FUNCTIONS.items() for name in names))
    compiler = os.environ.get("CC", "cc")
    subprocess.run([
        compiler, "-std=gnu11", "-Wall", "-Wextra", "-Wno-unused-parameter",
        "-Wno-incompatible-pointer-types", "-Wno-sign-compare",
        "-Wno-int-conversion", "-Wno-return-type",
        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-D_LANGUAGE_C", "-DGE_PORT_USE_ORIGINAL_TYPES", "-DAIPARSE",
        "-DGE_PORT_SETUP_DATA", "-DVERSION_US", "-DBUGFIX_R0",
        "-DGE_PORT_MS_INHERITS", "-fms-extensions",
        "-I" + str(ROOT), "-I" + str(ROOT / "port/include"),
        "-idirafter", str(ROOT / "include"),
        "-idirafter", str(ROOT / "include/PR"),
        "-idirafter", str(ROOT / "src"),
        str(original), str(ROOT / "port/src/ge_original_inventory.c"),
        str(ROOT / "port/src/ge_original_bond_input_provider.c"),
        str(ROOT / "port/tests/test_ge_original_inventory.c"),
        "-o", str(path / "test"),
    ], check=True)
    subprocess.run([str(path / "test")], check=True, env={
        **os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
        "UBSAN_OPTIONS": "halt_on_error=1",
    })
print("Inventory: canonical watch/ammo/dual gadget semantics, names, stale rows, state gates and bounds passed")
