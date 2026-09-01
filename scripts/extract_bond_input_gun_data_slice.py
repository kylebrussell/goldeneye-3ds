#!/usr/bin/env python3
"""Generate canonical initial-Dam weapon statistics for the input closure."""

from __future__ import annotations

import argparse
from pathlib import Path


ITEMS = (
    ("ITEM_UNARMED", None, True, None),
    ("ITEM_FIST", "fist", False, "fist"),
    ("ITEM_KNIFE", "knife", False, "knife"),
    ("ITEM_THROWKNIFE", "throwknife", False, "throwknife"),
    ("ITEM_WPPK", "wppk", False, "wppk"),
    ("ITEM_WPPKSIL", "wppksil", False, "wppksil"),
    ("ITEM_TT33", "tt33", False, "tt33"),
    ("ITEM_SKORPION", "skorpion", False, "skorpion"),
    # Dam's authored guards carry the KF7/AK47. Its canonical WeaponStats
    # must exist before chr action code classifies the held weapon.
    ("ITEM_AK47", "ak47", False, "ak47"),
    ("ITEM_UZI", "uzi", False, "uzi"),
    ("ITEM_MP5K", "mp5k", False, "mp5k"),
    ("ITEM_MP5KSIL", "mp5ksil", False, "mp5ksil"),
    ("ITEM_SPECTRE", "spectre", False, "spectre"),
    ("ITEM_M16", "m16", False, "m16"),
    ("ITEM_FNP90", "fnp90", False, "fnp90"),
    ("ITEM_SHOTGUN", "shotgun", False, "shotgun"),
    ("ITEM_AUTOSHOT", "autoshot", False, "autoshot"),
    ("ITEM_SNIPERRIFLE", "sniperrifle", False, "sniperrifle"),
    ("ITEM_RUGER", "ruger", False, "ruger"),
    ("ITEM_GOLDENGUN", "goldengun", False, "goldengun"),
    ("ITEM_SILVERWPPK", "silverwppk", False, "silverwppk"),
    ("ITEM_GOLDWPPK", "goldwppk", False, "goldwppk"),
    ("ITEM_LASER", "laser", False, "laser"),
    ("ITEM_WATCHLASER", "watchlaser", False, "watchlaser"),
    ("ITEM_GRENADELAUNCH", "grenadelaunch", False, "grenadelaunch"),
    ("ITEM_ROCKETLAUNCH", "rocketlaunch", False, "rocketlaunch"),
    ("ITEM_GRENADE", "grenade", False, "grenade"),
    ("ITEM_TIMEDMINE", "timedmine", False, "timedmine"),
    ("ITEM_PROXIMITYMINE", "proximitymine", False, "proximitymine"),
    ("ITEM_REMOTEMINE", "remotemine", False, "remotemine"),
    ("ITEM_TRIGGER", "trigger", False, "trigger"),
    ("ITEM_TASER", "taser", False, "taser"),
    ("ITEM_BOMBCASE", "bombcase", False, "bombcase"),
    ("ITEM_PLASTIQUE", "plastique", False, "plastique"),
    ("ITEM_FLAREPISTOL", "flarepistol", False, "flarepistol"),
    ("ITEM_PITONGUN", "pitongun", False, "pitongun"),
    ("ITEM_CAMERA", "camera", False, "camera"),
    ("ITEM_BUG", "bug", False, "bug"),
    ("ITEM_MICROCAMERA", "microcamera", False, "microcamera"),
    ("ITEM_WATCHMAGNETATTRACT", "watchmagnetattract", False,
     "watchmagnetattract"),
    ("ITEM_GOLDENEYEKEY", "goldeneyekey", False, "goldeneyekey"),
    ("ITEM_SUIT_LF_HAND", "Csuit_lf_handz", False, "suit_lf_hand",
     "Csuit_lf_handZ"),
    ("ITEM_JOYPAD", "joypad", False, "joypad"),
)


def find_initialized_global(source: str, declaration: str) -> str:
    start = source.find(declaration)
    if start < 0:
        raise ValueError(f"canonical global not found: {declaration}")
    brace = source.find("{", start + len(declaration))
    if brace < 0:
        raise ValueError(f"initializer missing: {declaration}")
    depth = 0
    for index in range(brace, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                semicolon = source.find(";", index)
                if semicolon < 0:
                    raise ValueError(f"initializer terminator missing: {declaration}")
                return source[start:semicolon + 1]
    raise ValueError(f"unterminated initializer: {declaration}")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    gun_records = args.repo / "assets/obseg/gun/gunModelFileRecord.inc.c"
    records_text = gun_records.read_text()
    ordered_includes = [
        "fist", "knife", "throwknife", "wppk", "wppksil", "tt33",
        "skorpion", "ak47", "uzi", "mp5k", "mp5ksil", "spectre",
        "m16", "fnp90", "shotgun", "autoshot", "sniperrifle", "ruger",
        "goldengun", "silverwppk", "goldwppk", "laser", "watchlaser",
        "grenadelaunch", "rocketlaunch", "grenade", "timedmine",
        "proximitymine", "remotemine", "trigger", "taser", "bombcase",
        "plastique", "flarepistol", "pitongun",
        "camera", "bug", "microcamera", "watchmagnetattract",
        "goldeneyekey", "joypad",
    ]
    previous = -1
    for name in ordered_includes:
        include = f"assets/obseg/gun/{name}/gunFileRecord.inc.c"
        position = records_text.find(include)
        if position <= previous:
            raise ValueError(f"canonical initial weapon order changed at {name}")
        record = (args.repo / include).read_text()
        if f"GUNSTATS({name})" not in record:
            raise ValueError(f"canonical stats binding missing for {name}")
        previous = position

    for name in ("fist", "wppk", "wppksil", "tt33", "skorpion", "ak47",
                 "uzi", "mp5k", "mp5ksil", "spectre", "m16", "fnp90",
                 "shotgun", "autoshot", "sniperrifle", "ruger",
                 "goldengun", "silverwppk", "goldwppk", "laser",
                 "watchlaser", "grenadelaunch", "rocketlaunch", "grenade",
                 "timedmine", "proximitymine", "remotemine", "trigger",
                 "taser", "bombcase", "plastique", "flarepistol",
                 "pitongun", "camera", "bug", "microcamera",
                 "watchmagnetattract", "goldeneyekey", "joypad"):
        header = (args.repo / f"assets/obseg/gun/{name}/ModelFileHeader.inc.c").read_text()
        if f"MODELFILEHEADER({name}," not in header:
            raise ValueError(f"canonical first-person model header missing for {name}")

    gun_source = (args.repo / "src/game/gun.c").read_text()
    skeleton_source = (
        args.repo / "assets/embedded/skeletons/standard_gun.inc.c"
    ).read_text()
    standard_gun_jointlist = find_initialized_global(
        skeleton_source, "ModelJoint JOINTLIST(standard_gun)[] =")
    if "MODELSKELETON(standard_gun, 6, 15)" not in skeleton_source:
        raise ValueError("canonical standard-gun skeleton changed")
    revolver_source = (
        args.repo / "assets/embedded/skeletons/gun_revolver.inc.c"
    ).read_text()
    revolver_jointlist = find_initialized_global(
        revolver_source, "ModelJoint JOINTLIST(gun_revolver)[] =")
    revolver_jointlist = revolver_jointlist.replace(
        "ModelJoint JOINTLIST", "__attribute__((weak)) ModelJoint JOINTLIST",
        1)
    if "MODELSKELETON(gun_revolver, 7, 21)" not in revolver_source:
        raise ValueError("canonical revolver skeleton changed")
    kf7_source = (
        args.repo / "assets/embedded/skeletons/gun_kf7.inc.c"
    ).read_text()
    kf7_jointlist = find_initialized_global(
        kf7_source, "ModelJoint JOINTLIST(gun_kf7)[] =")
    kf7_jointlist = kf7_jointlist.replace(
        "ModelJoint JOINTLIST", "__attribute__((weak)) ModelJoint JOINTLIST",
        1)
    if "MODELSKELETON(gun_kf7, 7, 0x12)" not in kf7_source:
        raise ValueError("canonical KF7/Uzi skeleton changed")
    weapon_source = (
        args.repo / "assets/embedded/skeletons/g_weapon.inc.c"
    ).read_text()
    weapon_jointlist = find_initialized_global(
        weapon_source, "ModelJoint JOINTLIST(g_weapon)[] =")
    weapon_jointlist = weapon_jointlist.replace(
        "ModelJoint JOINTLIST", "__attribute__((weak)) ModelJoint JOINTLIST",
        1)
    if "MODELSKELETON(g_weapon, 0xD, 0x27)" not in weapon_source:
        raise ValueError("canonical controller skeleton changed")
    suit_source = (
        args.repo / "assets/embedded/skeletons/suit_lf_hand.inc.c"
    ).read_text()
    suit_jointlist = find_initialized_global(
        suit_source, "ModelJoint JOINTLIST(suit_lf_hand)[] =")
    suit_jointlist = suit_jointlist.replace(
        "ModelJoint JOINTLIST",
        "__attribute__((weak)) ModelJoint JOINTLIST", 1)
    if "MODELSKELETON(suit_lf_hand, 19, 45)" not in suit_source:
        raise ValueError("canonical suit/watch skeleton changed")
    suit_header_source = (
        args.repo / "assets/obseg/chr/suit_lf_hand/modelFileHeader.inc.c"
    ).read_text()
    if ("MODELFILEHEADER(suit_lf_hand, 0, &SKELETON(suit_lf_hand), 0, "
            "0xA, 9, 12231.949, 0, 0x16)" not in suit_header_source):
        raise ValueError("canonical suit/watch model header changed")
    cartridge_source = (
        args.repo / "assets/obseg/gun/cartridge/Model.c"
    ).read_text()
    for source_name, native_name in (
        ("proptextures", "ge_cartridge_textures"),
        ("ModelNode_0x018", "ge_cartridge_node_018"),
        ("ModelNode_0x030", "ge_cartridge_node_030"),
        ("GroupRecord_0x048", "ge_cartridge_group_048"),
        ("DisplayListRecord_0x1a8", "ge_cartridge_dl_1a8"),
        ("Vertex_0x068", "ge_cartridge_vertices_068"),
        ("GFX_PRIMARY_0x1c0", "ge_cartridge_gfx_primary_1c0"),
        ("GFX_SECONDARY_0x230", "ge_cartridge_gfx_secondary_230"),
        ("PADDING_0x064", "ge_cartridge_padding_064"),
        ("PADDING_0x1bc", "ge_cartridge_padding_1bc"),
    ):
        cartridge_source = cartridge_source.replace(source_name, native_name)
    cartridge_source = cartridge_source.replace(
        "&ge_cartridge_group_048,", "(union ModelRoData *)&ge_cartridge_group_048,")
    cartridge_source = cartridge_source.replace(
        "&ge_cartridge_dl_1a8,", "(union ModelRoData *)&ge_cartridge_dl_1a8,")
    cartridge_source = cartridge_source.replace(
        "&ge_cartridge_gfx_primary_1c0,", "ge_cartridge_gfx_primary_1c0,")
    cartridge_source = cartridge_source.replace(
        "&ge_cartridge_gfx_secondary_230,", "ge_cartridge_gfx_secondary_230,")
    cartridge_source = cartridge_source.replace(
        "&ge_cartridge_vertices_068,", "ge_cartridge_vertices_068,")
    unassigned_source = (
        args.repo / "assets/embedded/skeletons/gun_unassigned.inc.c"
    ).read_text()
    unassigned_jointlist = find_initialized_global(
        unassigned_source, "ModelJoint JOINTLIST(gun_unassigned)[] =")
    if "MODELSKELETON(gun_unassigned, 1, 3)" not in unassigned_source:
        raise ValueError("canonical cartridge skeleton changed")
    ammo_count = "#define AMMO_RELATED_MAX 30"
    if ammo_count not in gun_source:
        raise ValueError("canonical ammo table size changed")
    ammo_table = find_initialized_global(
        gun_source, "AmmoStats ammo_related[AMMO_RELATED_MAX] =")
    zero_trigger = "struct gun_trigger_state g_ZeroTriggerState = { 0, 0 };"
    if zero_trigger not in gun_source:
        raise ValueError("canonical zero trigger state changed")

    hand_state_globals = [
        find_initialized_global(gun_source, declaration)
        for declaration in (
            "u32 D_80034CA4[] =",
            "u32 D_80034E0C[] =",
            "struct Weapon1PTransformKeyframe throwKnifeDrawBackKeyframes[6] =",
            "struct Weapon1PTransformKeyframe throwKnifeReleaseKeyframes[6] =",
            "struct Weapon1PTransformKeyframe grenadeThrowKeyframes[6] =",
            "struct Weapon1PTransformKeyframe timedMineThrowKeyframes[6] =",
            "struct Weapon1PTransformKeyframe proxMineThrowKeyframes[6] =",
            "struct Weapon1PTransformKeyframe remoteMineThrowKeyframes[7] =",
            "Weapon1PTransformKeyframe fistMeleeKeyframes1[10] =",
            "Weapon1PTransformKeyframe fistMeleeKeyframes2[10] =",
            "Weapon1PTransformKeyframe sniperMeleeKeyframes1[11] =",
            "Weapon1PTransformKeyframe sniperMeleeKeyframes2[11] =",
            "Weapon1PTransformKeyframe taserFireKeyFrames[6] =",
            "Weapon1PTransformKeyframe taserRaiseKeyframes[6] =",
            "struct sfx2 watchlaser_fire_sounds =",
            "struct sfx3 knife_throw_sounds =",
        )
    ]

    lines = [
        "/* Generated from canonical gun WeaponStats; do not edit. */",
        "typedef int PLAYERFLAG;",
        '#include "game/gun.h"',
        '#include "game/bondview.h"',
        '#include "game/image.h"',
        "#define IMAGE_232 232",
        "#define IMAGE_233 233",
        "/* Cartridge models are outside the input/stat ABI.  Private names",
        " * keep these relocation fields non-null without defining game model",
        " * symbols; no input or aim helper dereferences them. */",
        "static ModelFileHeader ge_input_cartrifle_header;",
        "static ModelFileHeader ge_input_cartblue_header;",
        "static ModelFileHeader ge_input_cartshell_header;",
        "#define cartrifle_header ge_input_cartrifle_header",
        "#define cartblue_header ge_input_cartblue_header",
        "#define cartshell_header ge_input_cartshell_header",
        unassigned_jointlist,
        "ModelSkeleton SKELETON(gun_unassigned) = {1, 0, JOINTLIST(gun_unassigned), 3, 0};",
        cartridge_source,
        "ModelFileHeader cartridge_header = {",
        "    &ge_cartridge_node_018, &SKELETON(gun_unassigned), NULL,",
        "    0, 1, 14.128822f, 0, 2, ge_cartridge_textures, 0",
        "};",
        '#include <assets/obseg/gun/gunWeaponStats.inc.c>',
        standard_gun_jointlist,
        "ModelSkeleton SKELETON(standard_gun) = {6, 0, JOINTLIST(standard_gun), 15, 0};",
        revolver_jointlist,
        "__attribute__((weak)) ModelSkeleton SKELETON(gun_revolver) = {7, 0, JOINTLIST(gun_revolver), 21, 0};",
        kf7_jointlist,
        "__attribute__((weak)) ModelSkeleton SKELETON(gun_kf7) = {7, 0, JOINTLIST(gun_kf7), 0x12, 0};",
        weapon_jointlist,
        "__attribute__((weak)) ModelSkeleton SKELETON(g_weapon) = {0xD, 0, JOINTLIST(g_weapon), 0x27, 0};",
        suit_jointlist,
        "__attribute__((weak)) ModelSkeleton SKELETON(suit_lf_hand) = {19, 0, JOINTLIST(suit_lf_hand), 45, 0};",
        "__attribute__((weak)) ModelFileHeader suit_lf_hand_header = {",
        "    NULL, &SKELETON(suit_lf_hand), NULL, 0xA, 9, 12231.949f,",
        "    0, 0x16, NULL, 0",
        "};",
        '#include <assets/obseg/gun/fist/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/knife/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/throwknife/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/wppk/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/wppksil/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/tt33/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/skorpion/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/ak47/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/uzi/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/mp5k/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/mp5ksil/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/spectre/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/m16/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/fnp90/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/shotgun/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/autoshot/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/sniperrifle/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/ruger/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/goldengun/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/silverwppk/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/goldwppk/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/laser/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/watchlaser/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/grenadelaunch/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/rocketlaunch/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/grenade/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/timedmine/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/proximitymine/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/remotemine/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/trigger/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/taser/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/bombcase/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/plastique/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/flarepistol/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/pitongun/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/camera/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/bug/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/microcamera/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/watchmagnetattract/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/goldeneyekey/ModelFileHeader.inc.c>',
        '#include <assets/obseg/gun/joypad/ModelFileHeader.inc.c>',
        "#undef cartrifle_header",
        "#undef cartblue_header",
        "#undef cartshell_header",
        ammo_count,
        ammo_table,
        zero_trigger,
        *hand_state_globals,
        "",
        "GunModelFileRecord gitem_structs[ITEM_JOYPAD + 1] = {",
    ]
    for item_record in ITEMS:
        item, stats, no_model, model = item_record[:4]
        resource = item_record[4] if len(item_record) > 4 else None
        stats_expr = "NULL" if stats is None else f"&{stats}_stats"
        header_expr = "NULL" if model is None else f"&{model}_header"
        file_name = resource if resource is not None else f"G{model}Z"
        file_expr = "NULL" if model is None else f'"{file_name}"'
        lines.append(
            f"    [{item}] = {{ .item_header = {header_expr}, "
            f".item_file_name = {file_expr}, "
            f".has_no_model = {str(no_model).upper()}, "
            f".item_weapon_stats = {stats_expr} }},"
        )
    lines.extend(("};", ""))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines))
    print(f"generated canonical initial weapon stats -> {args.output}")


if __name__ == "__main__":
    main()
