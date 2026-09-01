#!/usr/bin/env python3
"""Pin Dam's unchanged viewport-option and auto-aim publication order."""

from __future__ import annotations

import importlib.util
import sys
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def load_generator():
    path = REPO / "scripts/extract_gun_pose_helpers_slice.py"
    sys.path.insert(0, str(path.parent))
    spec = importlib.util.spec_from_file_location("gun_pose", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> None:
    generator = load_generator()
    options = (REPO / "src/game/options.c").read_text()
    generated = generator.generate(REPO)
    for name in (
        "cur_player_get_autoaim",
        "cur_player_get_lookahead",
        "cur_player_get_ammo_onscreen_setting",
        "cur_player_get_sight_onscreen_control",
    ):
        assert generator.extract_function(options, name) in generated

    main_source = (REPO / "platform/3ds/source/main.c").read_text()
    move = main_source.index("ge_original_bond_move_live_tick(")
    auto_y = main_source.rindex("currentPlayerSetYAutoAimEnabled(", 0, move)
    auto_x = main_source.rindex("currentPlayerSetXAutoAimEnabled(", 0, move)
    look = main_source.rindex("currentPlayerSetLookAheadSetting(", 0, move)
    ammo = main_source.rindex("gunSetGunAmmoVisible(", 0, move)
    sight = main_source.rindex("gunSetSightVisible(", 0, move)
    assert auto_y < auto_x < look < ammo < sight < move

    active_tick = "ge_original_stage_active_props_tick_exact("
    active_positions = []
    cursor = 0
    while True:
        cursor = main_source.find(active_tick, cursor)
        if cursor < 0:
            break
        active_positions.append(cursor)
        cursor += len(active_tick)
    assert len(active_positions) == 2
    for active in active_positions:
        onscreen = main_source.index("chraiUpdateOnscreenPropCount();", active)
        target = main_source.index("chrpropUpdateAutoaimTarget();", onscreen)
        player = main_source.index(
            "ge_original_stage_props_tick_player_exact();", target)
        assert active < onscreen < target < player < active + 1200

    stage_overlay = main_source.index("if (stage_actor_runtime_updated)")
    stage_overlay_end = main_source.index(
        "simulation_elapsed_milliseconds =", stage_overlay)
    stage_publication = main_source[stage_overlay:stage_overlay_end]
    assert stage_publication.count("chraiUpdateOnscreenPropCount();") == 1
    assert "chrpropUpdateAutoaimTarget();" not in stage_publication

    print("Live Dam viewport options precede MoveBond and exact prop/autoaim/player order is retained")


if __name__ == "__main__":
    main()
