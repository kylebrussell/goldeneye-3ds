#!/usr/bin/env python3
"""Retain GoldenEye's exact native guard-construction body for all stages."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path


def load_function_parser(repo: Path):
    path = repo / "scripts/extract_dam_guard_chr_scheduler_slice.py"
    spec = importlib.util.spec_from_file_location("ge_function_parser", path)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)
    return module.function_text


def render(repo: Path) -> str:
    function_text = load_function_parser(repo)
    chr_source = (repo / "src/game/chr.c").read_text()
    model_source = (repo / "src/game/model.c").read_text()
    file_source = (repo / "src/game/file.c").read_text()
    propobj_source = (repo / "src/game/propobj.c").read_text()
    chrprop_source = (repo / "src/game/chrprop.c").read_text()
    pieces = [
        "/* Generated unchanged canonical campaign guard actor slice. */",
        "#include <ultra64.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        '#include "game/chr.h"',
        '#include "game/chrobjdata.h"',
        '#include "game/file.h"',
        '#include "game/front.h"',
        '#include "game/model.h"',
        '#include "game/propobj.h"',
        "",
        "ChrRecord *ge_original_stage_guard_actor_chr_slots = 0;",
        "s32 ge_original_stage_guard_actor_num_chr_slots = 0;",
        "s32 ge_original_stage_guard_actor_next_chr_id = 0x1388;",
        "f32 ge_original_stage_guard_actor_animation_rate = 1.0f;",
        "extern s32 g_GlobalTimer;",
        "extern DIFFICULTY lvlGetSelectedDifficulty(void);",
        "extern void ge_original_dam_guard_chr_detect_rooms_exact(ChrRecord *chr);",
        "extern PropRecord *ge_original_stage_guard_hat_obj_init_exact(",
        "    ObjectRecord *object, ModelFileHeader *header, PropRecord *prop, Model *model);",
        "extern void ge_original_stage_guard_lighting_tile_rgb_exact(",
        "    StandTile *stan, f32 x, f32 z, u8 rgb[4]);",
        "",
        "#define g_ChrSlots ge_original_stage_guard_actor_chr_slots",
        "#define g_NumChrSlots ge_original_stage_guard_actor_num_chr_slots",
        "#define player1_guardID ge_original_stage_guard_actor_next_chr_id",
        "#define animation_rate ge_original_stage_guard_actor_animation_rate",
        "#define chrDetectRooms ge_original_dam_guard_chr_detect_rooms_exact",
        "#define modelGetNodeRwData ge_original_stage_guard_model_node_rw_exact",
        "#define setpartoffset ge_original_stage_guard_model_set_part_offset_exact",
        "#define setsuboffset ge_original_stage_guard_model_set_root_offset_exact",
        "#define setsubroty ge_original_stage_guard_model_set_root_angle_exact",
        "#define getsubroty ge_original_stage_guard_model_get_root_angle_exact",
        "#define modelSetAnimPlaySpeed ge_original_stage_guard_model_set_anim_play_speed_exact",
        "#define modelSetScale ge_original_stage_guard_model_set_scale_exact",
        "#define sub_GAME_7F06FF5C ge_original_stage_guard_model_set_callback_exact",
        "#define get_007_health_mod ge_original_stage_guard_health_mod_exact",
        "#define init_GUARDdata_with_set_values ge_original_stage_guard_actor_init_exact",
        "#define propweaponSetDual ge_original_stage_guard_weapon_set_dual_exact",
        "#define chrEquipWeapon ge_original_stage_guard_weapon_equip_exact",
        "#define weaponSetGunfireVisible ge_original_stage_guard_weapon_gunfire_exact",
        "#define chrpropReparent ge_original_stage_guard_weapon_reparent_exact",
        "",
        function_text(model_source, "modelGetNodeRwData"),
        function_text(model_source, "setpartoffset"),
        function_text(model_source, "setsuboffset"),
        function_text(model_source, "setsubroty"),
        function_text(model_source, "getsubroty"),
        function_text(model_source, "modelSetAnimPlaySpeed"),
        function_text(model_source, "modelSetScale"),
        function_text(model_source, "sub_GAME_7F06FF5C"),
        function_text(file_source, "get_007_health_mod"),
        function_text(chr_source, "init_GUARDdata_with_set_values").replace(
            "(s32) sub_GAME_7F01FC10",
            "(s32)(uintptr_t)sub_GAME_7F01FC10"),
        function_text(chrprop_source, "chrpropReparent"),
        function_text(propobj_source, "propweaponSetDual"),
        function_text(propobj_source, "chrEquipWeapon"),
        function_text(propobj_source, "weaponSetGunfireVisible"),
        function_text(propobj_source, "hatApplyToChr")
        .replace("hatApplyToChr", "ge_original_stage_guard_hat_apply_exact")
        .replace("objInit", "ge_original_stage_guard_hat_obj_init_exact")
        .replace("chrpropReparent", "ge_original_stage_guard_weapon_reparent_exact"),
        function_text(propobj_source, "set_color_shading_from_tile")
        .replace("set_color_shading_from_tile", "ge_original_stage_guard_lighting_sample_exact")
        .replace("copy_tile_RGB_as_24bit", "ge_original_stage_guard_lighting_tile_rgb_exact"),
        function_text(propobj_source, "update_color_shading")
        .replace("update_color_shading", "ge_original_stage_guard_lighting_step_exact"),
        "",
        "#undef chrpropReparent",
        "#undef weaponSetGunfireVisible",
        "#undef chrEquipWeapon",
        "#undef propweaponSetDual",
        "#undef init_GUARDdata_with_set_values",
        "#undef get_007_health_mod",
        "#undef sub_GAME_7F06FF5C",
        "#undef modelSetAnimPlaySpeed",
        "#undef modelSetScale",
        "#undef setsubroty",
        "#undef getsubroty",
        "#undef setsuboffset",
        "#undef setpartoffset",
        "#undef modelGetNodeRwData",
        "#undef chrDetectRooms",
        "#undef animation_rate",
        "#undef player1_guardID",
        "#undef g_NumChrSlots",
        "#undef g_ChrSlots",
        "",
        "void propweaponSetDual(WeaponObjRecord *leftweapon, "
        "WeaponObjRecord *rightweapon)\n"
        "{\n"
        "    ge_original_stage_guard_weapon_set_dual_exact("
        "leftweapon, rightweapon);\n"
        "}",
        "",
        "bool chrEquipWeapon(WeaponObjRecord *weapon, ChrRecord *chr)\n"
        "{\n"
        "    return ge_original_stage_guard_weapon_equip_exact("
        "weapon, chr);\n"
        "}",
        "",
    ]
    return "\n\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo))
    print("generated exact campaign guard actor constructor")


if __name__ == "__main__":
    main()
