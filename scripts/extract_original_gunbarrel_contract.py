#!/usr/bin/env python3
"""Pin the native gunbarrel adapter to the unchanged NTSC-U title.c body."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

from extract_gun_update_and_fire_slice import extract_function


FUNCTIONS = (
    "initializeGunBarrelIntro",
    "renderGunbarrelEyeIntroSequence",
    "sub_GAME_7F007F30",
    "insert_bond_eye_intro",
    "insert_sight_backdrop_eye_intro",
)

REQUIRED = (
    "gunbarrel_mode = 2",
    "g_TitleX = -30.0f",
    "g_TitleY = 482.0f",
    "titleTransitionX = -100.0f",
    "word_CODE_bss_80069584 = 0x42",
    "BODY_Brosnan_Tuxedo",
    "BODY_Male_Pierce_Bond_Tuxedo",
    "PROP_CHRWPPK",
    "modelSetScale(chrModelInstance, 0.18779343f)",
    "modelSetAnimTranslationScale(chrModelInstance, 1.0f)",
    "modelSetAnimPlaySpeed(chrModelInstance, S_7F008E80_ANIM_SPEED",
    "ANIM_DATA_bond_eye_walk",
    "animation->unk04 - 0x44",
    "#define XINC 6.0f",
    "#define XDEC3 5.8183274f",
    "#define INCVAL 0x38E",
    "#define INTRO_EYE_COUNTER_CASE_4 108",
    "#define INTRO_EYE_COUNTER_CASE_5_ADD 8",
    "#define INTRO_EYE_COUNTER_CASE_6 0x1e",
    "if (g_TitleX > 1390.0f)",
    "g_TitleX = 1276.0f",
    "intro_eye_counter = 20",
    "die_blood_image_routine(0)",
    "die_blood_image_routine(1)",
    "intro_eye_counter >= 0xF7",
    "#define BOND_EYE_ANIM_START 137",
    "#define BOND_EYE_ANIM_SPEEDUP 212",
    "#define BOND_EYE_FIRE_SHOT 230",
    "modelTickAnim(chrModelInstance, 1, 1)",
    "GUN_RIFLE7BIG_1_SFX",
    "createGunbarrelRenderHole(barrelDisplayListPtr, 0x1E)",
    "gunbarrelPosition1[3] = {1758.2957f, 220.0f, 684.28143f}",
    "gunbarrelPosition2[3] = {-0.97f, 0.0f, 0.24f}",
    "gunbarrelPosition3[3] = {0.0f, 1.0f, -0.0f}",
)


def generate(repo: Path) -> str:
    source = (repo / "src/game/title.c").read_text()
    title3 = (repo / "src/game/title3.c").read_text()
    title2 = (repo / "src/game/title2.c").read_text()
    blood = (repo / "src/game/blood_animation.c").read_text()
    bodies = "\n\n".join(extract_function(source, name) for name in FUNCTIONS)
    hole_body = extract_function(title3, "createGunbarrelRenderHole")
    sight_body = extract_function(title2,
        "titleRenderFolderMenuBackgroundLines")
    blood_image_body = extract_function(blood, "gunbarrelBloodOverlayDL")
    blood_colour_body = extract_function(blood, "sub_GAME_7F01CA18")
    # The platform constants are preprocessor definitions immediately before
    # the renderer body, so retain that exact source range in the digest too.
    constants_start = source.index("#ifndef REFRESH_PAL")
    constants_end = source.index("s32 isGunBarrelInMode9", constants_start)
    globals_end = source.index("Gfx *manipulateGunbarrelAndLogoMatrices")
    contract = (source[:globals_end] + "\n\n" + bodies + "\n\n"
        + hole_body + "\n\n" + sight_body + "\n\n"
        + blood_image_body + "\n\n" + blood_colour_body + "\n\n"
        + source[constants_start:constants_end])
    render_required = (
        "guOrtho(matrixBufferGunbarrel0, 0.0f, 1280.0f, 0.0f, 960.0f",
        "g_TitleX + 768.0f, g_TitleY - 40.0f",
        "2.7f, 2.57f, 1.0f",
        "while ((i + 1) < 300)",
        "G_IM_SIZ_8b, 440, 1",
        "(i + 0x10) << 2",
        "G_IM_FMT_I, 96, 80",
        "0x96, 0x00, 0x00, 0xB4",
        "150, 0, 0, 180",
    )
    for needle in render_required:
        if needle not in contract:
            raise ValueError(f"canonical gunbarrel render contract lost: {needle}")
    for needle in REQUIRED:
        if needle not in contract:
            raise ValueError(f"canonical gunbarrel contract lost: {needle}")
    digest = hashlib.sha256(contract.encode()).hexdigest()
    return f'''/* Generated mechanically by extract_original_gunbarrel_contract.py.
 * Source contract SHA-256: {digest} */
#define GE_ORIGINAL_GUNBARREL_CONTRACT_SHA256 "{digest}"
#define GE_ORIGINAL_GUNBARREL_X_INITIAL (-30.0f)
#define GE_ORIGINAL_GUNBARREL_Y_INITIAL (482.0f)
#define GE_ORIGINAL_GUNBARREL_TRANSITION_X_INITIAL (-100.0f)
#define GE_ORIGINAL_GUNBARREL_TRANSITION_Y_INITIAL (482.0f)
#define GE_ORIGINAL_GUNBARREL_WORD_INITIAL (0x42)
#define GE_ORIGINAL_GUNBARREL_X_INCREMENT (6.0f)
#define GE_ORIGINAL_GUNBARREL_X_DECREMENT (5.8183274f)
#define GE_ORIGINAL_GUNBARREL_X_LIMIT (1390.0f)
#define GE_ORIGINAL_GUNBARREL_X_RESTART (1276.0f)
#define GE_ORIGINAL_GUNBARREL_X_END (-80.0f)
#define GE_ORIGINAL_GUNBARREL_SINE_INCREMENT (0x38e)
#define GE_ORIGINAL_GUNBARREL_HOLD_FRAMES (20)
#define GE_ORIGINAL_GUNBARREL_SWAY_FRAMES (108)
#define GE_ORIGINAL_GUNBARREL_FADE_INCREMENT (8)
#define GE_ORIGINAL_GUNBARREL_FADE_LIMIT (0xf7)
#define GE_ORIGINAL_GUNBARREL_BLACK_FRAMES (0x1e)
#define GE_ORIGINAL_GUNBARREL_BOND_TICKS_PER_FRAME (2)
#define GE_ORIGINAL_GUNBARREL_ANIM_START_TICK (137)
#define GE_ORIGINAL_GUNBARREL_ANIM_SPEEDUP_TICK (212)
#define GE_ORIGINAL_GUNBARREL_FIRE_TICK (230)
#define GE_ORIGINAL_GUNBARREL_MODEL_SCALE (0.18779343f)
#define GE_ORIGINAL_GUNBARREL_WALK_FRAME_BACKSTEP (0x44)
#define GE_ORIGINAL_GUNBARREL_CAMERA_X (1758.2957f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_Y (220.0f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_Z (684.28143f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_DIR_X (-0.97f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_DIR_Y (0.0f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_DIR_Z (0.24f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_UP_X (0.0f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_UP_Y (1.0f)
#define GE_ORIGINAL_GUNBARREL_CAMERA_UP_Z (-0.0f)
#define GE_ORIGINAL_GUNBARREL_FIELD_OF_VIEW (46.0f)
#define GE_ORIGINAL_GUNBARREL_PERSPECTIVE_ASPECT (320.0f / 240.0f)
#define GE_ORIGINAL_GUNBARREL_PERSPECTIVE_NEAR (10.0f)
#define GE_ORIGINAL_GUNBARREL_PERSPECTIVE_FAR (10000.0f)
#define GE_ORIGINAL_GUNBARREL_LOGICAL_WIDTH (1280U)
#define GE_ORIGINAL_GUNBARREL_LOGICAL_HEIGHT (960U)
#define GE_ORIGINAL_GUNBARREL_NATIVE_WIDTH (440U)
#define GE_ORIGINAL_GUNBARREL_NATIVE_HEIGHT (330U)
#define GE_ORIGINAL_GUNBARREL_SIGHT_WIDTH (440U)
#define GE_ORIGINAL_GUNBARREL_SIGHT_HEIGHT (299U)
#define GE_ORIGINAL_GUNBARREL_SIGHT_Y (16U)
#define GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_X (768.0f)
#define GE_ORIGINAL_GUNBARREL_BACKDROP_OFFSET_Y (-40.0f)
#define GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_X (2.7f)
#define GE_ORIGINAL_GUNBARREL_BACKDROP_SCALE_Y (2.57f)
#define GE_ORIGINAL_GUNBARREL_BLOOD_WIDTH (96U)
#define GE_ORIGINAL_GUNBARREL_BLOOD_HEIGHT (80U)
'''


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    generated = generate(args.repo.resolve())
    if args.check:
        if not args.output.exists() or args.output.read_text() != generated:
            raise SystemExit(f"stale gunbarrel contract: {args.output}")
        return
    args.output.write_text(generated)


if __name__ == "__main__":
    main()
