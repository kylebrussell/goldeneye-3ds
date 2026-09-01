#!/usr/bin/env python3
"""Extract unchanged GoldenEye guard bullet-damage and reaction bodies."""

from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def load_module(path: Path, name: str):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    hit = load_module(repo / "scripts/extract_guard_bullet_hit_slice.py",
                      "guard_hit_extract")
    explosion = load_module(repo / "scripts/extract_bond_move_explosion_slice.py",
                            "explosion_extract")
    chr_source = (repo / "src/game/chr.c").read_text()
    action_source = (repo / "src/game/chraction.c").read_text()
    gun_source = (repo / "src/game/gun.c").read_text()
    gunfire_source = (repo / "src/game/gunfire.c").read_text()
    file_source = (repo / "src/game/file.c").read_text()
    front_source = (repo / "src/game/front.c").read_text()
    lv_source = (repo / "src/game/lv.c").read_text()
    propobj_source = (repo / "src/game/propobj.c").read_text()
    player_source = (repo / "src/game/player.c").read_text()
    debug_source = (repo / "src/game/debugmenu_handler.c").read_text()
    glass_source = (repo / "src/game/glass2.c").read_text()
    image_source = (repo / "src/game/image.c").read_text()
    image_bank_source = (repo / "src/game/image_bank.c").read_text()
    tex_source = (repo / "src/game/tex.c").read_text()
    explosion_source = (repo / "src/game/explosion.c").read_text()
    bondview2_source = (repo / "src/game/bondview2.c").read_text()
    chr_header = (repo / "src/game/chr.h").read_text()

    pieces = [
        "/* Generated from canonical guard damage bodies; do not hand-edit. */",
        "#include <limits.h>",
        "#include <math.h>",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <joy.h>",
        '#include "bondaicommands.h"',
        "typedef int PLAYERFLAG;",
        '#include "assets/animationtable_data.h"',
        '#include "game/chraction.h"',
        '#include "game/chr.h"',
        '#include "game/chr_b.h"',
        '#include "game/debugmenu_handler.h"',
        '#include "game/bg.h"',
        '#include "game/bgroomtrans.h"',
        '#include "game/bondinv.h"',
        '#include "game/cheat.h"',
        '#include "game/explosion.h"',
        '#include "game/file.h"',
        '#include "game/front.h"',
        '#include "game/glass.h"',
        '#include "game/gun.h"',
        '#include "game/image_bank.h"',
        '#include "game/image.h"',
        '#include "game/initanitable.h"',
        '#include "game/lv.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/mpmenu.h"',
        '#include "game/player.h"',
        '#include "game/propobj.h"',
        '#include "game/stan.h"',
        '#include "game/tex.h"',
        '#include "game/vtxstore.h"',
        "#include <music.h>",
        "#include <random.h>",
        "#include <snd.h>",
        "#if defined(GE_PORT_DAMAGE_HOST_ANIMATION_OFFSETS) || "
        "defined(GE_PORT_DAMAGE_ANIMATION_OFFSETS)",
        "/* Recreate the N64 absolute-offset animation symbol ABI on hosts",
        " * whose object format cannot emit linker-absolute data symbols. */",
        "#define ANIM_DATA_death_neck "
        "(*(s32 *)(uintptr_t)PTR_ANIM_death_neck)",
        "#define ANIM_DATA_hit_butt_long "
        "(*(s32 *)(uintptr_t)PTR_ANIM_hit_butt_long)",
        "#define ANIM_DATA_hit_butt_short "
        "(*(s32 *)(uintptr_t)PTR_ANIM_hit_butt_short)",
        "#endif",
        "#ifdef GE_PORT_DAMAGE_HOST_ANIMATION_OFFSETS",
        "#define vtxstore_allocate ge_port_vtxstore_allocate_ptr",
        "extern intptr_t ge_port_vtxstore_allocate_ptr(s32, s32, s32, s32);",
        "#endif",
        "extern f32 chrlvGetGuard007ArghRating(ChrRecord *, f32, f32);",
        "extern f32 chrlvPathingCollisionRelated7F0264B0(PropRecord *, f32, f32);",
        "extern s32 sub_GAME_7F053894(coord3d *, f32, f32);",
        "extern void chrStopFiring(ChrRecord *);",
        "extern void play_sound_for_shot_actor(ChrRecord *);",
        "extern void chrCreateHitPuffs(PropRecord *, s32, coord3d *, coord3d *);",
        "extern void increment_num_deaths(void);",
        "extern void increment_num_suicides_display_MP(void);",
        "extern void increment_num_times_killed_MwtGC(void);",
        "extern void bondviewKillCurrentPlayer(void);",
        "extern s_impacttype g_ImpactTypes[];",
        "#define BULLET_SPARKS_MAX 20",
        "#ifndef M_U32_MAX_VALUE_F",
        "#define M_U32_MAX_VALUE_F 4294967296.0f",
        "#endif",
        "extern struct animation_table_data *ptr_animation_table;",
        "",
    ]

    struck_names = re.findall(
        r"extern struct StruckAnim\s+([A-Za-z0-9_]+)\[\];", chr_header)
    for name in struck_names:
        pieces.append(explosion.extract_data(chr_source, name))
    pieces.append(explosion.extract_data(chr_source, "g_HitReactionTable"))
    pieces.append(explosion.extract_data(chr_source, "D_80030078"))
    pieces.append(explosion.extract_data(chr_source, "g_AiHealthModifier"))
    pieces.append(explosion.extract_data(action_source, "metal_ricochet_SFX"))
    pieces.append(explosion.extract_data(front_source,
                                          "slider_007_mode_reaction"))
    pieces.append(explosion.extract_data(lv_source, "g_SelectedDifficulty"))
    pieces.append(explosion.extract_data(lv_source, "D_800483C4"))
    pieces.append(explosion.extract_data(gun_source, "g_ImpactSfxStates"))
    pieces.append(explosion.extract_data(gun_source, "ricochet_sounds_small"))
    pieces.append(explosion.extract_data(gun_source, "punch_sounds"))
    pieces.append(explosion.extract_data(gun_source, "bullet_flesh_sounds"))
    pieces.append(explosion.extract_data(glass_source, "g_BulletSparkColors"))
    pieces.append(explosion.extract_data(glass_source, "g_BulletSparkArray"))
    for name in ("impactimages", "explosion_smokeimages",
                 "scattered_explosions", "flareimage2"):
        pieces.append(explosion.extract_data(image_bank_source, name))
    tex_data_names = re.findall(
        r"(?m)^(?:u16|u8|struct image_sound)\s+"
        r"([A-Za-z0-9_]+)(?:\[\])?\s*=", tex_source)
    for name in tex_data_names:
        if name.endswith(("_hit_sfx", "_impact_types")) \
                or name.startswith("isnd_"):
            pieces.append(explosion.extract_data(tex_source, name))
    pieces.append(explosion.extract_data(tex_source, "g_HitTypeSounds"))
    pieces.extend((
        "#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) "
        "{HS, HT, SZ, F3, F4, F5, F6 },",
        explosion.extract_data(image_source, "g_Textures"),
        "#undef IMAGE",
    ))
    # g_ImpactTypes is already owned by the exact shared explosion slice.
    for name in ("g_NumImpactEntries", "g_BulletImpactDefaultVertex"):
        pieces.append(explosion.extract_data(explosion_source, name))

    functions = (
        (chr_source, "chrSetHiddenToRandom"),
        (action_source, "get_distance_actor_to_position"),
        (action_source, "chrlvGetGuard007ArghRating"),
        (action_source, "chrlvLineLineIntersection"),
        (action_source, "chrlvStanLineDirIntersection"),
        (action_source, "chrlvPathingCollisionRelated"),
        (action_source, "chrlvPathingCollisionRelated7F0264B0"),
        (action_source, "chrlvAttackAnimationRelated7F026F30"),
        (action_source, "triggered_on_shot_hit"),
        (action_source, "handles_shot_actors"),
        (chr_source, "chrHandleBulletHit"),
        (chr_source, "chrCreateHitPuffs"),
        (chr_source, "chrCreateBloodStain"),
        (gun_source, "gunItemGetDestructionAmount"),
        (gun_source, "bondwalkItemGetForceOfImpact"),
        (gunfire_source, "gunSetTracerTarget"),
        (gunfire_source, "inc_curplayer_hitcount_with_weapon"),
        (gunfire_source, "gunGetFreeSfxState"),
        (gunfire_source, "recall_joy2_hits_edit_detail_edit_flag"),
        (file_source, "get_007_reaction_speed"),
        (lv_source, "lvlGetSelectedDifficulty"),
        (propobj_source, "get_hat_model"),
        (player_source, "set_cur_player"),
        (debug_source, "get_debug_joy2hitsedit_flag"),
        (debug_source, "get_debug_joy2detailedit_flag"),
        (propobj_source, "sub_GAME_7F0539E4"),
        (glass_source, "bullet_sparks_init"),
        (glass_source, "bullet_spark_create"),
        (explosion_source, "explosionRoundFloat"),
        (explosion_source, "explosionSetBulletImpactAlpha"),
        (explosion_source, "explosionCreateBulletImpact"),
        (bondview2_source, "record_damage_kills"),
    )
    for source, name in functions:
        body = hit.find_function(source, name)
        # Native compilers reject IDO's aggregate-copy initializer. This is a
        # literal expansion of the canonical three-entry table.
        body = body.replace(
            "s16 mrs[3] = metal_ricochet_SFX;",
            "s16 mrs[3] = {HIT_BULLET_METAL_A3_SFX, "
            "HIT_BULLET_METAL_A_SFX, HIT_BULLET_METAL_B_SFX};")
        pieces.append(body)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n\n".join(pieces) + "\n")


if __name__ == "__main__":
    main()
