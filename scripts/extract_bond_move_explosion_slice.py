#!/usr/bin/env python3
"""Extract exact explosion/detonation bodies directly referenced by MoveBond."""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path


FUNCTIONS = {
    "src/game/propobj.c": (
        "objInit",
        "projectileFree",
        "projectileReset",
        "embedmentFree",
        "objFreeEmbedmentOrProjectile",
        "projectileAllocate",
        "sub_GAME_7F03FDA8",
        "propobjSetDropped",
        "weaponSetGunfireVisible",
        "maybe_detonate_object_and_its_children",
        "objIsCollectable",
        "objIsMortal",
        "objGetShotsTaken",
        "sub_GAME_7F04DCB4",
        "save_ptr_monitor_ani_code_to_obj_ani_slot",
        "ammocrateAllocate",
        "objExplode",
        "objInitWithModelDef",
        "init_trigger_toxic_gas_effect",
        "check_if_toxic_gas_activated",
        "objApplyDamage",
        "sub_GAME_7F04B478",
        "sub_GAME_7F04B590",
        "objDeform",
        "objFall",
        "objDestroySupportedObjects",
    ),
    "src/game/chraction.c": (
        "chrlvResetAimend",
        "chrSetFiring",
        "chrStopFiring",
        "play_sound_for_shot_actor",
        "chrlvExplosionDamage",
    ),
    "src/game/chr.c": (
        "chrGetEquippedWeaponProp",
        "chrDropItems",
    ),
    "src/game/gunfire.c": (
        "inc_cur_civilian_casualties",
        "increment_num_kills_display_text_in_MP",
    ),
    "src/game/bondview2.c": (
        "getMissiontimer",
    ),
    "src/game/model.c": (
        "getsubroty",
        "modelmgrCanSlotFitRwdata",
        "modelmgrInstantiateModel",
        "clear_model_obj",
        "modelmgrInstantiateModelWithAnim",
        "clear_aircraft_model_obj",
    ),
    "src/game/vtxstore.c": (
        "sub_GAME_7F09B7A8",
        "sub_GAME_7F09B7E4",
        "sub_GAME_7F09B820",
        "vtxstore_allocate",
        "sub_GAME_7F09C044",
    ),
    "src/game/chrprop.c": (
        "chrpropFree",
        "chrpropActivateThisFrame",
        "chrpropReparent",
    ),
    "src/game/explosion.c": (
        "explosionClearBulletImpactRoomByFlag",
        "explosionClearBulletImpactRoom",
        "explosionInitFlyingParticles",
        "explosionCreate",
    ),
    "src/game/front.c": (
        "get_player_mp_char_gender",
    ),
    "src/game/glass.c": (
        "glassCreateShard",
        "sub_GAME_7F0A1DA0",
    ),
    "src/game/stan.c": (
        "getTileRoom",
    ),
    "src/game/debugmenu_handler.c": (
        "get_debug_explosioninfo_flag",
    ),
}

DATA = {
    "src/game/chr.c": (
        "D_8002E648",
        "expl_forward",
        "expl_f_left",
        "expl_f_right",
        "expl_left",
        "expl_right",
        "expl_back",
        "expl_b_right",
        "expl_b_left",
        "explosion_animation_table",
    ),
    "src/game/gun.c": (
        "aSD",
    ),
    "src/game/bondview2.c": (
        "mission_timer",
    ),
    "src/game/explosion.c": (
        "g_SmokeBuffer",
        "g_ExplosionBuffer",
        "max_particles",
        "g_FlyingParticlesBuffer",
        "g_NumExplosionEntries",
        "g_NumSmokeEntries",
        "g_SmokeTypes",
        "g_ExplosionTypes",
        "g_ImpactTypes",
        "g_NumParticleEntries",
        "g_BulletImpactBuffer",
    ),
    "src/game/front.c": (
        "mp_chr_setup",
        "player_char",
    ),
    "src/game/chrprop.c": (
        "g_AmmoCrates",
        "gasTimeToFullOpacity",
        "gasDoesDamageFlag",
    ),
    "src/game/propobj.c": (
        "blank_07_object",
        "monAnim33BlackSolid",
        "activate_gas_sound_timer",
        "gasLeakSource",
        "gasLeakTimer",
    ),
    "assets/obseg/prop/propExplosionDetailsRecords.inc.c": (
        "object_explosion_details",
    ),
    "src/game/glass.c": (
        "SHATTERED_WINDOW_PIECES_BUFFER_LEN",
        "ptr_shattered_window_pieces",
        "g_NextShardNum",
    ),
    "src/game/objecthandler.c": (
        "g_AnimModelSlots",
        "g_ModelSlots",
        "g_MaxAnimModelSlots",
        "g_MaxModelSlots",
        "g_ModelIsLvResetting",
    ),
    "src/game/vtxstore.c": (
        "dword_CODE_bss_8007A0D0",
        "dword_CODE_bss_8007A0D4",
        "dword_CODE_bss_8007A0D8",
        "dword_CODE_bss_8007A0DC",
        "dword_CODE_bss_8007A0E0",
        "dword_CODE_bss_8007A0E4",
        "dword_CODE_bss_8007A0E8",
        "dword_CODE_bss_8007A0EC",
        "word_CODE_bss_8007A0F0",
        "word_CODE_bss_8007A0F2",
    ),
}

TYPEDEFS = {
    "src/game/propobj.c": ("Word4",),
}


def extract_function(source: str, name: str) -> str:
    if name == "explosionCreate":
        signature = (
            "#if defined(VERSION_JP) || defined(VERSION_EU)\n"
            "s32\n#else\nvoid\n#endif\nexplosionCreate("
        )
        signature_pos = source.find(signature)
        match = None if signature_pos < 0 else re.search(
            rf"explosionCreate\s*\([^;\n]*\)[^;{{}}]*\{{",
            source[signature_pos:],
        )
        if match is not None:
            match_start = signature_pos
            brace = source.index("{", signature_pos + match.start())
        else:
            match_start = -1
            brace = -1
    else:
        match = re.search(
            rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{re.escape(name)}\s*\([^;\n]*\)[^;{{}}]*\{{",
            source,
        )
        match_start = match.start() if match is not None else -1
        brace = source.index("{", match.start()) if match is not None else -1
    if match is None:
        raise ValueError(f"missing {name}")
    depth = 0
    in_string = in_char = in_line_comment = in_block_comment = escaped = False
    pos = brace
    while pos < len(source):
        char = source[pos]
        next_char = source[pos + 1] if pos + 1 < len(source) else ""
        if in_line_comment:
            if char == "\n":
                in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
                pos += 1
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
        elif char == "/" and next_char == "/":
            in_line_comment = True
            pos += 1
        elif char == "/" and next_char == "*":
            in_block_comment = True
            pos += 1
        elif char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            depth += 1
        elif char == "}":
            depth -= 1
            if depth == 0:
                return source[match_start:pos + 1]
        pos += 1
    raise ValueError(f"unterminated {name}")


def extract_data(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^(?:/\*[^\n]*\*/\s*)?[A-Za-z_][^;\n]*\b{re.escape(name)}(?:\s*\[[^\n]*\])?\s*(?:=|;)",
        source,
    )
    if match is None:
        raise ValueError(f"missing data {name}")
    braces = brackets = parens = 0
    in_string = in_char = in_line_comment = in_block_comment = escaped = False
    for pos in range(match.end() - 1, len(source)):
        char = source[pos]
        next_char = source[pos + 1] if pos + 1 < len(source) else ""
        if in_line_comment:
            if char == "\n":
                in_line_comment = False
        elif in_block_comment:
            if char == "*" and next_char == "/":
                in_block_comment = False
        elif in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif in_char:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == "'":
                in_char = False
        elif char == "/" and next_char == "/":
            in_line_comment = True
        elif char == "/" and next_char == "*":
            in_block_comment = True
        elif char == '"':
            in_string = True
        elif char == "'":
            in_char = True
        elif char == "{":
            braces += 1
        elif char == "}":
            braces -= 1
        elif char == "[":
            brackets += 1
        elif char == "]":
            brackets -= 1
        elif char == "(":
            parens += 1
        elif char == ")":
            parens -= 1
        elif char == ";" and braces == brackets == parens == 0:
            return source[match.start():pos + 1]
    raise ValueError(f"unterminated data {name}")


def extract_typedef(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^typedef\s+struct\s+\w+\s*\{{[^}}]*\}}\s*{re.escape(name)}\s*;",
        source,
        flags=re.S,
    )
    if match is None:
        raise ValueError(f"missing typedef {name}")
    return match.group(0)


def digest(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def generate(repo: Path) -> str:
    paths = set(FUNCTIONS) | set(DATA) | set(TYPEDEFS)
    sources = {path: (repo / path).read_text() for path in paths}
    sections = [
        "/* Generated mechanically from canonical GoldenEye explosion bodies/data. */",
        "#include <math.h>",
        "#include <stdint.h>",
        "#include <limits.h>",
        "#include <stdio.h>",
        "#include <ultra64.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "typedef int PLAYERFLAG;",
        "struct unk_09B7A0_struct_parent {",
        "    Vertex *unk00; s32 unk04; s32 unk08;",
        "    s16 unk0C; s16 unk0E; s16 unk10; s16 unk12;",
        "};",
        '#include "boss.h"',
        '#include "game/chraction.h"',
        '#include "game/chrObjRandom.h"',
        '#include "game/chr.h"',
        '#include "game/explosion.h"',
        '#include "game/front.h"',
        '#include "game/glass.h"',
        '#include "game/gun.h"',
        '#include "game/initanitable.h"',
        '#include "game/language.h"',
        '#include "game/lv.h"',
        "s32 lvlGetCurrentStageToLoad(void);",
        '#include "game/model.h"',
        '#include "game/objecthandler.h"',
        '#include "game/player.h"',
        "#define sub_GAME_7F0A1DA0 ge_port_legacy_sub_GAME_7F0A1DA0_decl",
        '#include "game/propobj.h"',
        "#undef sub_GAME_7F0A1DA0",
        '#include "game/stan.h"',
        '#include "game/vtxstore.h"',
        '#include "assets/obseg/text/LgunE.h"',
        '#include "assets/obseg/text/LtitleE.h"',
        '#include "assets/oddtextures.h"',
        '#include "music.h"',
        '#include "random.h"',
        '#include "memp.h"',
        '#include "snd.h"',
        '#include "game/debugmenu_handler.h"',
        "#undef MODELSKELETON",
        "#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \\",
        "    ModelSkeleton SKELETON(NAME) = {NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0};",
        '#include "assets/embedded/skeletons/prop_weapon.inc.c"',
        "/* Canonical BITFLAG positions 6 and 7; AIPARSE suppresses BITFLAG. */",
        "#define RUNTIMEBITFLAG_REMOVE (1u << 2)",
        "#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)",
        "#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)",
        "#define RUNTIMEBITFLAG_00008000 (1u << 15)",
        "#define RUNTIMEBITFLAG_00010000 (1u << 16)",
        "#ifndef VERSION_EU",
        "#define SHARD_HORIZ_VEL_SCALE 1.5f",
        "#define SHARD_VERT_VEL_SCALE 3.0f",
        "#define SHARD_ANGVEL_SCALE 0.1f",
        "#else",
        "#define SHARD_HORIZ_VEL_SCALE 1.8f",
        "#define SHARD_VERT_VEL_SCALE 3.6f",
        "#define SHARD_ANGVEL_SCALE 0.12f",
        "#endif",
        "void explosionClearBulletImpactRoomByFlag(PropRecord *prop, s8 flag);",
        "void explosionClearBulletImpactRoom(PropRecord *prop);",
        "void objExplode(ObjectRecord *obj, coord3d *target_pos, s32 playernum);",
        "AmmoCrateRecord *ammocrateAllocate(void);",
        "void save_ptr_monitor_ani_code_to_obj_ani_slot(MonitorRecord *mon, void *image);",
        "void objDeform(ObjectRecord *obj, E_EXPLOSIONTYPE explosiontype);",
        "void objDestroySupportedObjects(PropRecord *tableprop, s32 playernum);",
        "void objFall(ObjectRecord *obj, s32 playernum);",
        "PropRecord *objInit(ObjectRecord *obj, ModelFileHeader *model_header, PropRecord *prop, Model *model);",
        "ModelRoData_BoundingBoxRecord *chrobjGetBboxFromObjFile(ModelFileHeader *obj);",
        "void sub_GAME_7F0A1DA0(coord3d *pos, coord3d *xaxis, coord3d *yaxis, coord3d *zaxis, f32 xmin, f32 xmax, f32 ymin, f32 ymax, f32 arg8, f32 arg9);",
        "",
    ]
    for relative, names in DATA.items():
        for name in names:
            body = extract_data(sources[relative], name)
            sections.extend((f"/* {name} sha256={digest(body)} */", body, ""))
    for relative, names in TYPEDEFS.items():
        for name in names:
            body = extract_typedef(sources[relative], name)
            sections.extend((f"/* {name} sha256={digest(body)} */", body, ""))
    for relative, names in FUNCTIONS.items():
        for name in names:
            body = extract_function(sources[relative], name)
            sections.extend((f"/* {name} sha256={digest(body)} */", body, ""))
            if name == "vtxstore_allocate":
                host_body = body.replace(
                    "s32 vtxstore_allocate(",
                    "intptr_t ge_port_vtxstore_allocate_ptr(", 1).replace(
                    "var_a2 = ((s16 *)&dword_CODE_bss_8007A0D4)[1];",
                    "var_a2 = dword_CODE_bss_8007A0D4;", 1).replace(
                    "var_a2 = ((s16 *)&dword_CODE_bss_8007A0DC)[1];",
                    "var_a2 = dword_CODE_bss_8007A0DC;", 1).replace(
                    "return (s32)var_t0[var_a1].unk00;",
                    "return (intptr_t)var_t0[var_a1].unk00;", 1)
                sections.extend((
                    "#ifdef GE_PORT_VTXSTORE_HOST_POINTER_ABI",
                    "/* Same canonical allocator with native pointer width and",
                    " * the intended count fields, independent of host endian. */",
                    host_body,
                    "#endif",
                    "",
                ))
    return "\n".join(sections)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = generate(args.repo)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output)
    count = (sum(map(len, FUNCTIONS.values())) + sum(map(len, DATA.values()))
             + sum(map(len, TYPEDEFS.values())))
    print(f"generated {count} exact MoveBond explosion bodies/data -> {args.output}")


if __name__ == "__main__":
    main()
