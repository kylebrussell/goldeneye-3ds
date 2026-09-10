#!/usr/bin/env python3
"""Generate fixture's unchanged canonical health, death and inventory closure."""
from pathlib import Path
import argparse
from extract_stage_pickup_slice import function_text
from extract_bond_move_explosion_slice import extract_data

FUNCTIONS = {
    "src/game/player.c": ("set_cur_player",),
    "src/game/bondview.c": ("init_player_BONDdata",),
    "src/game/bondview2.c": ("change_player_pos_to_target", "bondviewKillCurrentPlayer", "record_damage_kills", "bondviewAddCurrentPlayerArmor"),
    "src/game/gunfire.c": ("give_cur_player_ammo","increment_num_kills_display_text_in_MP", "increment_num_times_killed_MwtGC", "increment_num_deaths", "increment_num_suicides_display_MP"),
    "src/game/bondinv.c": ("bondinvReinitInv", "bondinvGetNextAvailItem", "bondinvGetInvItem", "bondinvHasInvItem", "bondinvHasGoldenGun", "bondinvSortInv", "bondinvInsertItem", "bondinvAddInvItem", "bondinvAddDoublesInvItem", "bondinvGetDualWeapon", "bondinvHasDualWeapon"),
}

def render(repo):
    s = '''/* Generated canonical fixture closure, no replacement damage or inventory. */
#include <ultra64.h>
#include <stdio.h>
#include <math.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondview.h"
#include "game/bondinv.h"
#include "game/front.h"
#include "game/gun.h"
#include "game/lv.h"
#include "game/language.h"
#include "game/options.h"
#include "snd.h"
#include "assets/obseg/text/LgunE.h"
#define AMMO_RELATED_MAX 30
extern s32 g_stopPlayFlag, g_gameOverFlag, g_PlayerIsInTank, g_ExplodeTankOnDeathFlag, g_isBondKIA;
extern bool g_PlayerInvincible;
extern PropRecord *g_PlayerTankProp;
extern ALBank *g_musicSfxBufferPtr;
extern s32 get_player_control_style(s32);
extern void hudMakeDamageSegments(void *, s32, s32, f32);
extern void joyRumblePakStart(s32, f32);
extern void increment_num_kills_display_text_in_MP(void);
extern void increment_num_times_killed_MwtGC(void);
extern void increment_num_deaths(void);
extern void increment_num_suicides_display_MP(void);
extern void drop_inventory(void);
extern void trigger_solo_watch_menu(s32);
extern void hudmsgBottomShow(char *);
extern s32 bondinvHasInvItem(ITEM_IDS);
extern InvItem *bondinvGetInvItem(ITEM_IDS);
extern InvItem *bondinvGetNextAvailItem(void);
extern InvItem *bondinvGetDualWeapon(ITEM_IDS, ITEM_IDS);
extern bool bondinvHasDualWeapon(ITEM_IDS, ITEM_IDS);
extern void bondinvInsertItem(InvItem *);
extern void bondinvSortInv(InvItem *);
extern s32 bondinvAddDoublesInvItem(ITEM_IDS, ITEM_IDS);
static const char aSD[] = "%s %d";
static const char aSD_0[] = "%s %d";
static const char g_GunDeathCountFormat[] = "%s %d %s";
'''
    s += "\n" + extract_data((repo / "src/game/gun.c").read_text(), "ammo_related") + "\n"
    for path, names in FUNCTIONS.items():
        source = (repo / path).read_text()
        for name in names:
            s += "\n" + function_text(source, name) + "\n"
    return s
if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
