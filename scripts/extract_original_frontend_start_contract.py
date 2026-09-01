#!/usr/bin/env python3
"""Pin the 3DS frontend bridge to the decompiled solo-start contract.

This does not translate the N64 menu renderer.  It verifies the exact canonical
interface/init bodies which own each transition, then emits only the authored
complete-solo identifiers/text records needed by the platform-neutral
snapshot layer.
"""

from __future__ import annotations

import argparse
import hashlib
import re
from pathlib import Path

from extract_gun_update_and_fire_slice import extract_function


FUNCTIONS = (
    ("src/game/front.c", "frontChangeMenu"),
    ("src/game/front.c", "menu_init"),
    ("src/game/front.c", "init_menu00_legalscreen"),
    ("src/game/front.c", "interface_menu00_legalscreen"),
    ("src/game/front.c", "constructor_menu00_legalscreen"),
    ("src/game/front.c", "init_menu01_nintendo"),
    ("src/game/front.c", "interface_menu01_nintendo"),
    ("src/game/front.c", "constructor_menu01_nintendo"),
    ("src/game/front.c", "init_menu02_rarelogo"),
    ("src/game/front.c", "interface_menu02_rareware"),
    ("src/game/front.c", "constructor_menu02_rareware"),
    ("src/game/front.c", "init_menu03_gunbarrel"),
    ("src/game/front.c", "interface_menu03_eye"),
    ("src/game/front.c", "constructor_menu03_eye"),
    ("src/game/front.c", "init_menu04_goldeneyelogo"),
    ("src/game/front.c", "interface_menu04_goldeneyelogo"),
    ("src/game/front.c", "constructor_menu04_goldeneyelogo"),
    ("src/game/title.c", "setupRarewareLogoData"),
    ("src/game/title.c", "load_display_rare_logo"),
    ("src/game/title.c", "retrieve_display_rareware_logo"),
    ("src/game/title.c", "isGunBarrelInMode2"),
    ("src/game/title.c", "initializeGunBarrelIntro"),
    ("src/game/title.c", "renderGunbarrelEyeIntroSequence"),
    ("src/game/title.c", "insert_bond_eye_intro"),
    ("src/game/title.c", "isGunBarrelInMode9"),
    ("src/game/front.c", "interface_menu05_fileselect"),
    ("src/game/front.c", "interface_menu06_modesel"),
    ("src/game/front.c", "interface_menu07_missionsel"),
    ("src/game/front.c", "interface_menu08_difficulty"),
    ("src/game/front.c", "interface_menu0A_briefing"),
    ("src/game/front.c", "init_menu0B_runstage"),
    ("src/game/ramromreplay.c", "interface_menu0B_runstage"),
    ("src/game/front.c", "init_menu0C_missionfailed"),
    ("src/game/front.c", "interface_menu0C_missionfailed"),
    ("src/game/front.c", "frontCompleteAllObjectivesAliveSuccess"),
    ("src/game/front.c", "constructor_menu0C_missionfailed"),
    ("src/game/front.c", "init_menu0D_missioncomplete"),
    ("src/game/front.c", "interface_menu0D_missioncomplete"),
    ("src/game/front.c", "constructor_menu0D_missioncomplete"),
)

REQUIRED = (
    "PitemZ_entries[PROP_LEGALPAGE]",
    "viSetXY(440, 330)",
    "viSetViewSize(440, 330)",
    "MENU_LEGALSCREEN_MENU_TIMER_MAX (60*4+1)",
    "modelSetScale(logoinst, 1.0f)",
    "setsuboffset(logoinst, &pos)",
    "matrix_4x4_set_lookat_target(&lookatmtx, 0.0f, 0.0f, 4000.0f",
    "matrix_4x4_copy(&lookatmtx, renderdata.mtxlist)",
    "frontChangeMenu(MENU_NINTENDO_LOGO, TRUE)",
    "PitemZ_entries[PROP_NINTENDOLOGO]",
    "NINTENDO_TIMER_MAX 501",
    "ninLogoRotRate += 0.017453292f",
    "ninLogoScale *= 1.07977f",
    "setupRarewareLogoData(ptr_logo_and_walletbond_DL, 0x78000)",
    "guPerspective(&matrixBufferRareLogo0[D_8002A7D0], &perspNorm, 60.0f, (320.0f / 240.0f), 100.0f, 5000.0f, 1.0f)",
    "cameraPosition1[2] = arg3",
    "isGunBarrelInMode2()",
    "RAREWARE_LOGO_EYE_COUNT1 260",
    "RAREWARE_LOGO_EYE_COUNT2 290",
    "initializeGunBarrelIntro(ptr_logo_and_walletbond_DL, 0x78000)",
    "isGunBarrelInMode9()",
    "renderGunbarrelEyeIntroSequence (DL)",
    "guPerspective(&matrixBufferIntroBond[D_8002A7D0], &perspNorm, 46.0f, (320.0f / 240.0f), 10.0f, 10000.0f, 1.0f)",
    "gunbarrelPosition1[0]",
    "PitemZ_entries[PROP_GOLDENEYELOGO]",
    "guLookAtReflect(&logoReflectMtx, logoLookAt",
    "frontChangeMenu(MENU_FILE_SELECT",
    "frontChangeMenu(MENU_MODE_SELECT",
    "frontChangeMenu(MENU_MISSION_SELECT",
    "selected_stage = mission_folder_setup_entries[briefingpage].stage_id",
    "frontChangeMenu(MENU_DIFFICULTY",
    "selected_difficulty = mission_difficulty_highlighted",
    "frontChangeMenu(MENU_BRIEFING",
    "frontChangeMenu(MENU_RUN_STAGE",
    "bossSetLoadedStage(selected_stage)",
    "lvlSetSelectedDifficulty(selected_difficulty)",
    "frontChangeMenu(MENU_MISSION_FAILED, 1)",
    "frontChangeMenu(MENU_MISSION_COMPLETE, FALSE)",
    "frontCompleteAllObjectivesAliveSuccess()",
    "frontChangeMenu(MENU_MISSION_SELECT, FALSE)",
    "getStringID(LTITLE, TITLE_STR_98_REPORT)",
    "getStringID(LTITLE, TITLE_STR_99_MISSIONSTATUS)",
    "getStringID(LTITLE, TITLE_STR_100_KIA)",
    "getStringID(LTITLE, TITLE_STR_101_ABORTED)",
    "getStringID(LTITLE, TITLE_STR_102_COMPLETED)",
    "getStringID(LTITLE, TITLE_STR_103_FAILED)",
    "getStringID(LTITLE, TITLE_STR_104_STATS)",
)


# Solo mission ordering and the two distinct title channels are copied from
# mission_folder_setup_entries.  The first title is printed on briefing/report
# pages; the icon title is the shorter Bank Gothic label used by the 5x4 grid.
MISSION_SPECS = (
    ("SP_LEVEL_DAM", "LEVELID_DAM", "TITLE_STR_121_DAM", None,
     "UbriefdamZ", "LDAM"),
    ("SP_LEVEL_FACILITY", "LEVELID_FACILITY", "TITLE_STR_122_FAC", None,
     "UbriefarkZ", "LARK"),
    ("SP_LEVEL_RUNWAY", "LEVELID_RUNWAY", "TITLE_STR_123_RUN", None,
     "UbriefrunZ", "LRUN"),
    ("SP_LEVEL_SURFACE1", "LEVELID_SURFACE", "TITLE_STR_125_SURF", None,
     "UbriefsevxZ", "LSEVX"),
    ("SP_LEVEL_BUNKER1", "LEVELID_BUNKER1", "TITLE_STR_126_BUNK", None,
     "UbriefsevbunkerZ", "LSEV"),
    ("SP_LEVEL_SILO", "LEVELID_SILO", "TITLE_STR_128_SILO4",
     "TITLE_STR_129_SILO", "UbriefsiloZ", "LSILO"),
    ("SP_LEVEL_FRIGATE", "LEVELID_FRIGATE", "TITLE_STR_131_FRIG", None,
     "UbriefdestZ", "LDEST"),
    ("SP_LEVEL_SURFACE2", "LEVELID_SURFACE2", "TITLE_STR_125_SURF", None,
     "UbriefsevxbZ", "LSEVXB"),
    ("SP_LEVEL_BUNKER2", "LEVELID_BUNKER2", "TITLE_STR_126_BUNK", None,
     "UbriefsevbZ", "LSEVB"),
    ("SP_LEVEL_STATUE", "LEVELID_STATUE", "TITLE_STR_133_STATPARK",
     "TITLE_STR_134_STAT", "UbriefstatueZ", "LSTAT"),
    ("SP_LEVEL_ARCHIVES", "LEVELID_ARCHIVES", "TITLE_STR_135_MILARCH",
     "TITLE_STR_136_ARCH", "UbriefarchZ", "LARCH"),
    ("SP_LEVEL_STREETS", "LEVELID_STREETS", "TITLE_STR_137_STREETS", None,
     "UbriefpeteZ", "LPETE"),
    ("SP_LEVEL_DEPOT", "LEVELID_DEPOT", "TITLE_STR_138_DEPOT", None,
     "UbriefdepoZ", "LDEPO"),
    ("SP_LEVEL_TRAIN", "LEVELID_TRAIN", "TITLE_STR_139_TRAIN", None,
     "UbrieftraZ", "LTRA"),
    ("SP_LEVEL_JUNGLE", "LEVELID_JUNGLE", "TITLE_STR_141_JUN", None,
     "UbriefjunZ", "LJUN"),
    ("SP_LEVEL_CONTROL", "LEVELID_CONTROL", "TITLE_STR_142_CONCENTER",
     "TITLE_STR_143_CON", "UbriefcontrolZ", "LAREC"),
    ("SP_LEVEL_CAVERNS", "LEVELID_CAVERNS", "TITLE_STR_144_WATERCAV",
     "TITLE_STR_145_CAV", "UbriefcaveZ", "LCAVE"),
    ("SP_LEVEL_CRADLE", "LEVELID_CRADLE", "TITLE_STR_146_ANTENNA",
     "TITLE_STR_147_CRADLE", "UbriefcradZ", "LCRAD"),
    ("SP_LEVEL_AZTEC", "LEVELID_AZTEC", "TITLE_STR_149_AZTECCOMPLEX",
     "TITLE_STR_150_AZTEC", "UbriefaztZ", "LAZT"),
    ("SP_LEVEL_EGYPT", "LEVELID_EGYPT", "TITLE_STR_152_EGYPTIANTEMPLE",
     "TITLE_STR_153_EGYPTIAN", "UbriefcrypZ", "LCRYP"),
)


def _objective_rows(brief: str, bank: str) -> list[tuple[int, str]]:
    rows = [(int(text), difficulty) for parsed_bank, text, difficulty in
            re.findall(r"\{getStringID\((L[A-Z0-9]+),\s*(\d+)\),\s*"
                       r"(DIFFICULTY_[A-Z0-9]+)\}", brief)
            if parsed_bank == bank]
    if not rows or len(rows) > 5:
        raise ValueError(f"invalid authored objective table for {bank}: {rows}")
    return rows


def generate(repo: Path) -> str:
    front = (repo / "src/game/front.c").read_text()
    if "struct coord3d legalpage_pos = {0.0f, 0.0f, 0.0f};" not in front:
        raise ValueError("canonical legal-page authored origin lost")
    if "struct coord3d nintendologo_pos = {0};" not in front:
        raise ValueError("canonical Nintendo-logo authored origin lost")
    sources = {"src/game/front.c": front,
        "src/game/title.c": (repo / "src/game/title.c").read_text(),
        "src/game/ramromreplay.c":
            (repo / "src/game/ramromreplay.c").read_text()}
    bodies = [extract_function(sources[path], name)
        for path, name in FUNCTIONS]
    contract = "\n\n".join(bodies)
    for needle in REQUIRED:
        if needle not in contract:
            raise ValueError(f"canonical frontend contract lost: {needle}")
    mission_lines = []
    briefs = []
    rows = []
    current_chapter_number = None
    current_chapter_title = None
    mission_heading = {}
    entry_block = front.split(
        "struct mission_folder_setup mission_folder_setup_entries[] = {", 1
    )[1].split("{NULL,", 1)[0]
    for entry_line in entry_block.splitlines():
        stripped = entry_line.strip()
        if not stripped.startswith('{"'):
            continue
        number_match = re.match(r'\{"([^"]+)",', stripped)
        title_match = re.search(
            r'getStringID\(LTITLE,\s*(TITLE_STR_[A-Z0-9_]+)\)', stripped)
        if number_match is None or title_match is None:
            raise ValueError(f"invalid mission-folder heading: {stripped}")
        if "MISSION_HEADER" in stripped:
            current_chapter_number = number_match.group(1)
            current_chapter_title = title_match.group(1)
        elif "MISSION_PART" in stripped:
            brief_match = re.search(r'"(Ubrief[A-Za-z0-9]+Z)"', stripped)
            if (brief_match is not None and current_chapter_number is not None
                    and current_chapter_title is not None):
                mission_heading[brief_match.group(1)] = (
                    current_chapter_number, current_chapter_title,
                    number_match.group(1))
    for mission, stage, title, icon, brief_name, bank in MISSION_SPECS:
        mission_line = next(line.strip() for line in front.splitlines()
            if f'"{brief_name}"' in line and stage in line)
        brief = (repo / f"assets/obseg/brief/{brief_name}.c").read_text()
        objectives = _objective_rows(brief, bank)
        title_expr = f"getStringID(LTITLE,{title})"
        icon_expr = (f"getStringID(LTITLE,{icon})" if icon is not None
                     else title_expr)
        text_ids = [f"getStringID({bank},{text})" for text, _ in objectives]
        difficulties = [difficulty for _, difficulty in objectives]
        text_ids += ["0"] * (5 - len(text_ids))
        difficulties += ["0"] * (5 - len(difficulties))
        chapter_number, chapter_title, part_number = mission_heading[brief_name]
        rows.append(
            f'    {{{mission},{stage},{title_expr},{icon_expr},{bank},'
            f'"{chapter_number}",getStringID(LTITLE,{chapter_title}),'
            f'"{part_number}",{len(objectives)},\n'
            f"      {{{','.join(text_ids)}}},\n"
            f"      {{{','.join(difficulties)}}}}},")
        mission_lines.append(mission_line)
        briefs.append(brief)
    digest = hashlib.sha256((contract + "\n" + "\n".join(mission_lines)
        + "\n" + "\n".join(briefs)).encode()).hexdigest()
    mission_rows = "\n".join(rows)
    return f'''/* Generated mechanically by extract_original_frontend_start_contract.py.
 * Canonical complete-solo frontend contract SHA-256: {digest} */
#define GE_FRONTEND_CONTRACT_SHA256 "{digest}"
#define GE_FRONTEND_DAM_MISSION SP_LEVEL_DAM
typedef struct GeFrontendMissionContract {{
    int32_t mission;
    int32_t stage;
    uint16_t title;
    uint16_t grid_title;
    uint16_t text_bank;
    const char *chapter_number;
    uint16_t chapter_title;
    const char *part_number;
    uint8_t objective_count;
    uint16_t objective_text[5];
    uint8_t objective_difficulty[5];
}} GeFrontendMissionContract;
static const GeFrontendMissionContract ge_frontend_contract_missions[]={{
{mission_rows}
}};
static const uint16_t ge_frontend_contract_difficulty_text[4]={{
    getStringID(LTITLE,TITLE_STR_19_AGENT),
    getStringID(LTITLE,TITLE_STR_20_SECRETAGENT),
    getStringID(LTITLE,TITLE_STR_21_00AGENT),
    getStringID(LTITLE,TITLE_STR_22_007)
}};
static const uint16_t ge_frontend_contract_briefing_heading[5]={{
    getStringID(LTITLE,TITLE_STR_93_PRIMARYOBJECTIVES),
    getStringID(LTITLE,TITLE_STR_94_BACKGROUND),
    getStringID(LTITLE,TITLE_STR_95_MBRIEFING),
    getStringID(LTITLE,TITLE_STR_96_QBRANCH),
    getStringID(LTITLE,TITLE_STR_97_MONEYPENNY)
}};
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    output = generate(args.repo)
    if args.check:
        if not args.output.exists() or args.output.read_text() != output:
            raise SystemExit(f"stale canonical frontend contract: {args.output}")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(output)


if __name__ == "__main__":
    main()
