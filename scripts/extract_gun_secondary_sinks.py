#!/usr/bin/env python3
"""Extract unchanged non-Dam gun sinks referenced by gunUpdateAndFire."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from extract_gun_update_and_fire_slice import extract_function


FUNCTIONS = (
    ("src/game/gun.c", "generate_player_thrown_grenade"),
    ("src/game/gun.c", "generate_player_thrown_knife"),
    ("src/game/gun.c", "gunSpawnGLGrenade"),
    ("src/game/gun.c", "gunUpdateAttachedRocket"),
    ("src/game/gunfire.c", "gunFireTankShell"),
)


def generate(repo: Path) -> str:
    cache: dict[str, str] = {}
    bodies: list[str] = []
    for relative, name in FUNCTIONS:
        source = cache.setdefault(relative, (repo / relative).read_text())
        bodies.append(extract_function(source, name))
    digest = hashlib.sha256("\n\n".join(bodies).encode()).hexdigest()
    preamble = [
        "/* Generated mechanically from canonical gun sources; do not edit. */",
        f"/* Canonical body SHA-256: {digest} */",
        "#include <math.h>",
        "#include <limits.h>",
        '#include "include/math.h"',
        "#include <ultra64.h>",
        "#include <PR/gu.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        '#include "game/bondview.h"',
        '#include "game/chr.h"',
        '#include "game/dyn.h"',
        '#include "game/explosion.h"',
        '#include "game/gun.h"',
        '#include "game/matrixmath.h"',
        '#include "game/model.h"',
        '#include "game/propobj.h"',
        '#include "random.h"',
        '#include "snd.h"',
        '#include "ge_original_bond_input_internal.h"',
        "#define THROWN_ITEM_REFRESH_RATE 60",
        "#define THROWN_ITEM_TIMER_DEFAULT 240",
        "#define GLGRENADE_TIMER 1200",
        "#ifndef RUNTIMEBITFLAG_HASPROJECTILE",
        "#define RUNTIMEBITFLAG_HASPROJECTILE RUNTIMEBITFLAG_00000080",
        "#endif",
        "#ifndef RUNTIMEBITFLAG_THROWING_KNIFE_RELATED",
        "#define RUNTIMEBITFLAG_THROWING_KNIFE_RELATED 0x00000020",
        "#endif",
        "extern void *g_musicSfxBufferPtr;",
        "extern void ge_original_random_throw_transform_exact(coord3d *, Mtxf *);",
        "extern void ge_original_bullet_path_from_screen_center_exact(coord3d *, coord3d *, GUNHAND);",
        "extern void ge_original_gun_init_projectile_from_player_exact(ObjectRecord *, coord3d *, Mtxf *, coord3d *, Mtxf *);",
        "extern Mtxf *ge_original_gun_current_player_view_to_world(void);",
        "#define sub_GAME_7F057C14 ge_original_random_throw_transform_exact",
        "#define bullet_path_from_screen_center ge_original_bullet_path_from_screen_center_exact",
        "#define gunInitProjectileFromPlayer ge_original_gun_init_projectile_from_player_exact",
        "#define currentPlayerGetViewToWorldMtxf ge_original_gun_current_player_view_to_world",
        "#define getCurrentPlayerPrevPos ge_original_get_current_player_prev_pos_exact",
        "extern coord3d *ge_original_get_current_player_prev_pos_exact(void);",
        "extern void gunInitProjectileObject(ObjectRecord *, coord3d *, StandTile *, Mtxf *, coord3d *, Mtxf *, PropRecord *);",
        "extern void setSixExplosionAndSmokeEntries(void);",
        "const f32 ge_g_gl_grenade_launch_unk8c[1] = {0.3f};",
        "const f32 ge_g_gl_grenade_launch_unk94[1] = {0.13333333f};",
        "const f32 ge_g_tank_shell_speed[1] = {66.666664f};",
        "const f32 ge_d_80053ddc[1] = {1.111111f};",
        "#define g_GLGrenadeLaunchUnk8C ge_g_gl_grenade_launch_unk8c[0]",
        "#define g_GLGrenadeLaunchUnk94 ge_g_gl_grenade_launch_unk94[0]",
        "#define g_TankShellSpeed ge_g_tank_shell_speed[0]",
        "#define D_80053DDC ge_d_80053ddc[0]",
    ]
    return "\n".join(preamble) + "\n\n" + "\n\n".join(bodies) + "\n"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    print(f"generated {len(FUNCTIONS)} exact secondary gun sinks -> {args.output}")


if __name__ == "__main__":
    main()
