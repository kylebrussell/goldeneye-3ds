#!/usr/bin/env python3
"""Extract exact services retained by the canonical Dam guard scheduler."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

from extract_dam_guard_chr_scheduler_slice import function_text


# Shared with the token-exactness test so adding an unchanged body from another
# translation unit cannot silently leave the verification source map behind.
SOURCE_FILES = {
    "chr": "chr.c",
    "file": "file.c",
    "front": "front.c",
    "chraction": "chraction.c",
    "propobj": "propobj.c",
    "bgfog": "bgfog.c",
    "bondview": "bondview.c",
    "bondview2": "bondview2.c",
    "debug": "debugmenu_handler.c",
    "model": "model.c",
    "objecthandler": "objecthandler.c",
    "objective_status2": "objective_status2.c",
    "objective_status": "objective_status.c",
    "chrprop": "chrprop.c",
    "explosion": "explosion.c",
    "fr": "../fr.c",
    "player": "player.c",
    "gun": "gun.c",
    "gunfire": "gunfire.c",
    "bg": "bg.c",
    "lightfixture": "lightfixture.c",
    "stan": "stan.c",
    "file2": "file2.c",
}


# Keep this ownership scheduler-specific. In particular, no action handler is
# extracted here; those remain owned by the guard-AI slices.
FUNCTIONS = (
    ("file", "get_007_accuracy_mod"),
    ("file", "get_007_damage_mod"),
    ("chr", "chrUpdateAimProperties"),
    ("chr", "chrGetFlinchAmount"),
    ("chraction", "chrlvAttackRelated7F0292A8"),
    ("chraction", "sub_GAME_7F02BFE4"),
    ("chraction", "sub_GAME_7F02C27C"),
    ("chraction", "chrlvUpdateShotbondsum"),
    ("bondview2", "bondviewCallRecordDamageKills"),
    ("chraction", "sub_GAME_7F02D630"),
    ("gun", "gunInitProjectileObject"),
    ("propobj", "chrobjMaybeDetonateObjectIfFlags"),
    ("chraction", "chrlvFireWeaponRelated"),
    ("chraction", "chrlvTriggerFireWeapon"),
    ("bgfog", "fogGetNearFogValuesP"),
    ("bondview2", "bondviewGetCurrentPlayersPosition"),
    ("bgfog", "fogPositionIsVisibleThroughFog"),
    ("propobj", "getPropCombinedRoomsBBox2D"),
    ("bondview", "camIsPosInScreenBox"),
    ("bondview", "camIsPosInScreen"),
    ("propobj", "sub_GAME_7F054C58"),
    ("propobj", "posIsOnScreen"),
    ("chr", "chrpropCleanupForRemoval"),
    ("propobj", "sub_GAME_7F050DE8"),
    ("propobj", "objDetach"),
    ("chrprop", "chrpropDetach"),
    ("debug", "get_debug_chrnum_flag"),
    ("debug", "get_debug_render_raster"),
    ("model", "modelSetDistanceScale"),
    ("model", "sub_GAME_7F06C768"),
    ("propobj", "chrRenderHeldWeapon"),
    ("model", "instcalcmatrices"),
    ("model", "modelUpdateMatrices"),
    ("model", "sub_GAME_7F06E540"),
    ("model", "sub_GAME_7F06E2B8"),
    ("model", "process_15_subposition"),
    ("model", "process_03_unknown"),
    ("objecthandler", "sub_GAME_7F06B29C"),
    ("objecthandler", "sub_GAME_7F06BB28"),
    ("chr", "chrHandleJointPositioned"),
    ("chrprop", "chrpropDelist"),
    ("chrprop", "chrpropDisable"),
    ("explosion", "explosionChrpropSmokeTick"),
    ("explosion", "explosionChrpropExplosionTick"),
    ("chrprop", "propExecuteTickOperation"),
    ("player", "get_player_position_in_shuffled"),
    ("propobj", "handle_alarm_gas_timer_calldamage"),
    ("propobj", "countdownTimerSetValue"),
    ("propobj", "countdownTimerGetValue"),
    ("propobj", "countdownTimerSetRunning"),
    ("propobj", "countdownTimerIsRunning"),
    ("propobj", "alarmIsActive"),
    ("propobj", "handle_gas_damage"),
    ("bgfog", "fogSwitchToSolosky2"),
    ("propobj", "if_enabled_reset_clock"),
    ("propobj", "check_guard_detonate_proxmine"),
    ("chraction", "chrlvGetPatrolPercentOrPosition"),
    ("propobj", "detonate_proxmine_In_range"),
    ("propobj", "alarmDeactivate"),
    ("propobj", "door7F0526EC"),
    ("propobj", "doorUpdateBbox"),
    ("chrprop", "propsDefragRoomProps"),
)

# Audit-only closure for every non-character branch selected by the unchanged
# propsTick dispatcher.  The normal generated object remains at FUNCTIONS until
# this entire frontier is link-complete and sanitizer-covered.
FULL_PROPS_FUNCTIONS = (
    # The live camera publisher calls this exact body after replacing the
    # canonical player's view-to-world matrix.  Rename it here because the
    # isolated matrix-producer harness owns a deliberately inert function of
    # the original name and a different compact player ABI.
    ("bondview", "bondviewUpdateFrustumPlanes"),
    ("chrprop", "chraiUpdateOnscreenPropCount"),
    ("gunfire", "get_bullet_angle"),
    ("bondview2", "bondviewUpdateXAutoAimTime"),
    ("chr", "chrGetOnscreenRenderBounds"),
    ("chrprop", "chrpropScoreAutoAimTarget"),
    ("chrprop", "chrpropUpdateAutoaimTarget"),
    # Direct unchanged helpers needed by the five projectile background/STAN
    # services below.  The remaining geometry primitives are already owned by
    # the live exact visibility, bullet-hit, and STAN slices.
    ("lightfixture", "check_if_imageID_is_light"),
    ("bg", "addToByteSetMaxSize15"),
    ("bg", "bgTestRayIntersectionInRoom"),
    # stan.o now owns these unchanged bodies for the live canonical
    # init_pathtable_something nearest-tile fallback.
    # Exact background/STAN services reached by handles_projectile_motion.
    # These stay audit-only until the complete unchanged props dispatcher is
    # link- and sanitizer-closed.
    ("bg", "bgFindRoomsAlongSegment"),
    ("bg", "bgCopyVisibleRoomsToList"),
    ("bg", "bgTestBulletHitBackground"),
    ("bg", "get_room_data_float2"),
    ("stan", "stanFindTileBelowPos"),
    ("propobj", "projectileSetSticky"),
    ("gunfire", "recall_joy2_hits_edit_flag"),
    ("bg", "bgGet2dBboxByRoomId"),
    ("bgfog", "fogLoadCurrentEnvironment"),
    ("propobj", "objTick"),
    ("bondview2", "playerTick"),
    ("fr", "viSetZRange"),
    ("fr", "viGetZRange"),
    ("propobj", "objFree"),
    ("propobj", "sub_GAME_7F042EB4"),
    ("propobj", "chrobjWeaponTick"),
    ("propobj", "sub_GAME_7F0442DC"),
    ("propobj", "objDropRecursively"),
    ("objective_status2", "sub_GAME_7F057DF8"),
    ("propobj", "glassCalculateOpacity"),
    ("propobj", "sub_GAME_7F04424C"),
    ("propobj", "objSettle"),
    ("propobj", "doorIsClosed"),
    ("chrprop", "doorIsPadlockFree"),
    ("propobj", "doorActivateWrapper"),
    ("propobj", "sub_GAME_7F0439B8"),
    ("propobj", "objEmbed"),
    ("propobj", "sub_GAME_7F0448A8"),
    ("propobj", "sub_GAME_7F044B38"),
    ("propobj", "doorDeactivatePortal"),
    ("objective_status2", "mtxLoadRandomRotation"),
    ("explosion", "explosionCreateSmoke"),
    ("bondview", "playerGetCrouchPos"),
    ("bondview2", "bondviewDeregisterPlayerRoom"),
    ("bondview2", "bondviewUpdatePlayerRoom"),
    ("bondview", "currentPlayerSetPerspective"),
    ("bondview", "currentPlayerSetCameraScale"),
    ("propobj", "objTryMovePropWithCollision"),
    ("propobj", "handles_projectile_motion"),
    ("bondview2", "SurroundWithExplosions"),
    ("propobj", "remove_obj_from_temp_proxmine_table"),
    ("propobj", "add_obj_to_temp_proxmine_table"),
    ("chrprop", "sub_GAME_7F03E6A0"),
    ("propobj", "sub_GAME_7F043838"),
    ("propobj", "embedmentAllocate"),
)


def definition_text(source: str, pattern: str) -> str:
    match = re.search(pattern, source, re.MULTILINE)
    if match is None:
        raise ValueError(f"missing data definition {pattern}")
    start = match.start()
    brace = source.find("{", match.start(), match.end() + 256)
    semi = source.find(";", match.start())
    if brace < 0 or semi < brace:
        return source[start:semi + 1]
    depth = 0
    state = "code"
    pos = brace
    while pos < len(source):
        char = source[pos]
        nxt = source[pos + 1] if pos + 1 < len(source) else ""
        if state == "code":
            if char == "/" and nxt == "*":
                state = "block"
                pos += 2
                continue
            if char == '"':
                state = "string"
            elif char == "{":
                depth += 1
            elif char == "}":
                depth -= 1
                if depth == 0:
                    semi = source.find(";", pos)
                    return source[start:semi + 1]
        elif state == "block" and char == "*" and nxt == "/":
            state = "code"
            pos += 2
            continue
        elif state == "string":
            if char == "\\":
                pos += 2
                continue
            if char == '"':
                state = "code"
        pos += 1
    raise ValueError(f"unterminated data definition {pattern}")


def definition_text_occurrence(source: str, pattern: str, index: int) -> str:
    matches = list(re.finditer(pattern, source, re.MULTILINE))
    if len(matches) <= index:
        raise ValueError(f"missing data definition occurrence {index}: {pattern}")
    return definition_text(source[matches[index].start():], pattern)


def function_declaration(source: str, name: str) -> str:
    body = function_text(source, name)
    return body[:body.index("{")].rstrip() + ";"


def selected_function_text(source: str, name: str) -> str:
    if name != "propExplode":
        return function_text(source, name)
    source = re.sub(
        r"#if defined\(VERSION_JP\) \|\| defined\(VERSION_EU\)\n"
        r"s32 propExplode\([^\n]+\)\n#else\n"
        r"(void propExplode\([^\n]+\))\n#endif",
        r"\1", source, count=1)
    return function_text(source, name)


def render(repo: Path, full_props: bool = False) -> str:
    game = repo / "src/game"
    sources = {key: (game / filename).read_text()
               for key, filename in SOURCE_FILES.items()}
    fr_header = (repo / "src/fr.h").read_text()
    image_externs = (repo / "assets/image_externs.h").read_text()
    pieces = [
        "/* Generated unchanged services for the canonical guard scheduler. */",
        "#include <ultra64.h>",
        "#include <PR/gbi.h>",
        "#include <gbi_extension.h>",
        "#include <assets/image_externs.h>",
        "#include <bondgame.h>",
        "#include <bondconstants.h>",
        "#include <bondtypes.h>",
        "#include <math.h>",
        "#include <float.h>",
        "#include <memp.h>",
        "#include <random.h>",
        "#include <snd.h>",
        '#include "bg.h"',
        '#include "bondaicommands.h"',
        '#include "bgfog.h"',
        '#include "boss.h"',
        '#include "bondview.h"',
        '#include "cheat.h"',
        '#include "chr.h"',
        '#include "chrai.h"',
        '#include "chraction.h"',
        '#include "chrobjdata.h"',
        '#include "debugmenu_handler.h"',
        '#include "dyn.h"',
        '#include "explosion.h"',
        '#include "file2.h"',
        '#include "front.h"',
        '#include "gun.h"',
        '#include "glass.h"',
        '#include "initanitable.h"',
        '#include "joy.h"',
        '#include "lv.h"',
        '#include "matrixmath.h"',
        '#include "math_unk_05A9E0.h"',
        '#include "model.h"',
        '#include "music.h"',
        '#include "objecthandler.h"',
        '#include "objective_status.h"',
        '#include "player.h"',
        '#include "propobj.h"',
        '#include "quaternion.h"',
        '#include "stan.h"',
        '#include "stanintersection.h"',
        '#include "tex.h"',
        '#include "vtxstore.h"',
        "",
        "#define PI_OVER_3 1.0471976f",
        "#define FIVEPI_OVER_18 0.87266463f",
        "#ifndef M_U32_MAX_VALUE_F",
        "#define M_U32_MAX_VALUE_F 4294967296.0f",
        "#endif",
        "#ifndef M_U16_MAX_VALUE_F",
        "#define M_U16_MAX_VALUE_F 65536.0f",
        "#endif",
        "#ifndef RUNTIMEBITFLAG_REMOVE",
        "#define RUNTIMEBITFLAG_REMOVE (1u << 2)",
        "#endif",
        "#ifndef RUNTIMEBITFLAG_HASPROJECTILE",
        "#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)",
        "#endif",
        "#ifndef PROXIMITY_MINE_TRIGGER_DISTANCE",
        "#define PROXIMITY_MINE_TRIGGER_DISTANCE 62500.0f",
        "#endif",
        "#define OP16_NODEINDEX_0C(data) (((ModelNode_Op16Record *)(data))->nodeindex0c)",
        "#define OP16_NODEINDEX_0E(data) (((ModelNode_Op16Record *)(data))->nodeindex0e)",
        "#define OP16_NODEINDEX_10(data) (((ModelNode_Op16Record *)(data))->nodeindex10)",
        "#define OP16_POS_X_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[0])",
        "#define OP16_POS_Y_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[1])",
        "#define OP16_POS_Z_VOL(data) (((volatile ModelNode_Op16Record *)(data))->pos.f[2])",
        "extern void clear_aircraft_model_obj(Model *model);",
        "extern void chrlvFireWeaponRelated(ChrRecord *chr, s32 hand);",
        "extern void handle_gas_damage(void);",
        "extern void if_enabled_reset_clock(void);",
        "extern void check_guard_detonate_proxmine(void);",
        "extern coord3d *chrlvGetChrOrPresetLocation(ChrRecord *chr,",
        "        s32 flags, s32 lookup_id, StandTile **stan);",
        "extern void setSeenBondTimeToNow(ChrRecord *chr);",
        "extern bool sub_GAME_7F04B590(ModelFileHeader *header, ModelNode *node);",
        "extern void chrpropDetach(PropRecord *prop);",
        "extern void detonate_proxmine_In_range(coord3d *pos);",
        "extern f32 slider_007_mode_accuracy;",
        "extern f32 slider_007_mode_damage;",
        "extern void setupUpdateObjectRoomPosition(ObjectRecord *obj);",
        "extern u32 modelAnimReadBitsAsU16Angle(u8 *bitstream, u8 width,",
        "                                         u32 bit_offset);",
        "extern void fogLoadCurrentEnvironment(EnvironmentRecord *environment);",
        "extern PadRecord *chrlvGetNextPatrolStepPad(ChrRecord *chr);",
        "extern void chrlvActGoposRelated(ChrRecord *chr, coord3d *target,",
        "                                 StandTile **stan);",
        "extern s32 objTick(PropRecord *prop);",
        "extern s32 playerTick(PropRecord *prop);",
        "extern bool stanTileHasZeroArea(StandTile *tile);",
        "extern void getTileMidPoint(StandTile *tile, coord3d *out);",
        "",
    ]
    if full_props:
        pieces.extend((
            "#ifndef M_PI_2F",
            "#define M_PI_2F 1.5707964f",
            "#endif",
            "#define OBJECT_INTERACTION_TIMER_DELTA g_GlobalTimerDelta",
            "#define CAM_ACCEL 0.00065449846f",
            "#define AUTOGUN_SPIN_ACCEL_PER_FRAME 0.009973311f",
            "#define AUTOGUN_SPIN_MAX_SPEED 0.5983986f",
            "#define ROCKET_SPEED_BREAK_THRESHOLD 27777.773f",
            "#define PROJECTILE_FRICTION_FACTOR 0.9f",
            "#define AUTOGUN_YAW_MAX_SPEED 0.00069813174f",
            "#define AUTOGUN_YAW_ACCEL_PER_FRAME 0.000011635529f",
            "#define AUTOGUN_PITCH_ACCEL_PER_FRAME 0.0000058177643f",
            "#define AUTOGUN_PITCH_MAX_SPEED 0.00034906587f",
            "#define AUTOGUN_ALERT_ACCEL_PER_FRAME 0.0008726647f",
            "#define AUTOGUN_TRACKING_FRAMES 120",
            "#define TRUCK_TURN_ACCEL_PER_FRAME 0.000109083085f",
            "#define TRUCK_TURN_DECEL_PER_FRAME 0.00021816617f",
            "#define TRUCK_TURN_MAX_SPEED 0.006544985f",
            "#define PROJECTILE_LIFETIME_FRAMES 2400",
            "#define GRENADE_SMOKE_FRAMES 301",
            "#define CCTV_ALARM_FRAMES 300.0f",
            "#define U32MAX 4294967295",
            "#define U32_TO_F32(x) (x*(1.0f/U32MAX))",
            "#define ROCKET_INITIAL_GRAVITY_MODIFIER 0.27777779f",
            "#define PROP_PROJECTILE_GRAVITY_MODIFIER 0.27777779f",
            "#define NUM_VIDEO_SETTINGS 2",
            "#define SCREEN_WIDTH 320",
            "#define SCREEN_HEIGHT 240",
            "#define FOV_Y_F 60.0f",
            "#define ASPECT_RATIO 1.3333334f",
            "#define BONDVIEW_AUTOAIM_TIME 0x19",
            "#define RUNTIMEBITFLAG_ISRETICK (1u << 3)",
            "#define RUNTIMEBITFLAG_THROWING_KNIFE_RELATED (1u << 5)",
            "#define RUNTIMEBITFLAG_EMBEDDED (1u << 6)",
            "#define RUNTIMEBITFLAG_00010000 (1u << 16)",
            "#define RUNTIMEBITFLAG_ACTIVATED (1u << 14)",
            "#define chrTick ge_original_dam_guard_chr_tick_exact",
            "#define bondviewUpdateFrustumPlanes ge_original_bondview_update_frustum_planes_exact",
            "#define door7F054FB4 ge_original_door_runtime_tick_slice",
            "#define sub_GAME_7F03CFE8 ge_door_collision_sub_GAME_7F03CFE8",
            "#define ZeroCoord ge_original_zero_coord",
            "#define g_BgRoomInfo ge_bg_visibility_room_info",
            "#define room_data_float1 ge_bg_visibility_level_scale",
            "#define room_data_float2 ge_bg_visibility_inverse_level_scale",
            "#define list_visible_rooms_in_cur_global_vis_packet ge_bg_visibility_global_rooms",
            "#define num_visible_rooms_in_cur_global_vis_packet ge_bg_visibility_global_room_count",
            "#define g_BgNumberOfRoomsDrawn ge_bg_visibility_rooms_drawn",
            "#define dword_CODE_bss_8007FFA0 ge_bg_visibility_draw_rooms",
            "extern ALSoundState *gunGetFreeSfxState(void);",
            "extern struct LaserRichochetSounds laser_ricochet_sounds;",
            "extern struct RicochetSoundsLarge ricochet_sounds_large;",
            "extern s32 ge_bg_visibility_rooms_drawn;",
            "extern s_bound_info ge_bg_visibility_draw_rooms[204];",
            "extern void ge_original_door_runtime_tick_slice(DoorRecord *door);",
            "extern f32 ge_door_collision_sub_GAME_7F03CFE8(PropRecord *prop);",
            "extern coord3d ge_original_zero_coord;",
            "extern s_room_info ge_bg_visibility_room_info[MAXROOMCOUNT];",
            "extern f32 ge_bg_visibility_level_scale;",
            "extern f32 ge_bg_visibility_inverse_level_scale;",
            "extern char ge_bg_visibility_global_rooms[0x98];",
            "extern s32 ge_bg_visibility_global_room_count;",
            "extern s32 sub_GAME_7F0B9F14(s32 portalnum, coord3d *pos1, coord3d *pos2);",
            "extern bool bgTestRayIntersectsBbox(coord3d *origin, coord3d *dir, s32 *bbox_min, s32 *bbox_max);",
            "extern bool intersectRayTriangle(Vertex *vertex0, Vertex *vertex1, Vertex *vertex2, coord3d *vertexOffset, coord3d *rayStart, coord3d *linePoint, coord3d *rayDirection, HitThing *hit);",
            "extern f32 getShortest2dDispToInfTripleEdge(StandTile *tile, s32 start3index, f32 x, f32 z);",
            "extern f32 level_scale;",
            "extern f32 inv_level_scale;",
            "extern u8 list_of_tilesizes[];",
            "extern s_smoketype g_SmokeTypes[];",
            "extern s32 g_SurroundBondWithExplosionsTicks;",
            "extern s32 g_PlayerTickExplodeCreatePosition;",
            "extern ModelRenderData D_80030B34;",
            "extern void ai(PropDefHeaderRecord *entity, PROP_TYPE type);",
            "extern void viSetZRange(f32 near, f32 far);",
            "extern void viGetZRange(f32 *zrange);",
            "extern s32 g_PlayerTickCount;",
            "extern f32 floorFloat(f32 value);",
            "extern f32 ceilFloat(f32 value);",
            definition_text(sources["chrprop"], r"^f32 difficulty\s*="),
            definition_text(
                sources["chrprop"], r"^struct coord2d g_DefaultAutoAimCoord\s*="),
            "extern struct firing_anim_struct firing_animation_groups[][6];",
            "extern void bondviewUpdatePlayerRoom(struct player *player);",
            "extern s32 chrTick(PropRecord *prop);",
            "extern void propExplode(PropRecord *prop, s32 explosion_type);",
            "extern void matrix_4x4_invert_affine(Mtxf *matrix, Mtxf *result);",
            "extern void guNormalize(f32 *x, f32 *y, f32 *z);",
            "struct Mtxf;",
            definition_text(image_externs, r"^typedef enum IMAGEIDS"),
            definition_text(fr_header, r"^typedef struct VideoSettings_s"),
            function_declaration(sources["bondview"], "playerGetCrouchPos"),
            function_declaration(sources["propobj"], "objGetWidth"),
            function_declaration(
                sources["propobj"], "projectileFindCollidingProp"),
            function_declaration(
                sources["objective_status2"], "mtxLoadRandomRotation"),
            function_declaration(
                sources["objective_status2"], "sub_GAME_7F057DF8"),
            definition_text(sources["bg"], r"^struct HitThingSub\s*\{"),
        ))
        for name in (
            "objFree", "sub_GAME_7F042EB4", "projectileFree",
            "sub_GAME_7F0439B8", "objEmbed", "objSettle",
            "sub_GAME_7F0402B4", "doorSetOpenState", "doorIsClosed",
            "door7F054FB4", "sub_GAME_7F053894", "sub_GAME_7F044B38",
            "glassCalculateOpacity", "doorDeactivatePortal",
            "door7F0526EC", "chrobjWeaponTick", "sub_GAME_7F0442DC",
            "sub_GAME_7F04424C", "sub_GAME_7F053A3C",
        ):
            pieces.append(function_declaration(sources["propobj"], name))
        pieces.append(function_declaration(
            sources["chrprop"], "doorIsPadlockFree"))
        for source, name in (
            ("explosion", "explosionClearBulletImpactRoomByFlag"),
            ("propobj", "handles_projectile_motion"),
            ("propobj", "objTryMovePropWithCollision"),
            ("propobj", "add_obj_to_temp_proxmine_table"),
            ("propobj", "remove_obj_from_temp_proxmine_table"),
            ("propobj", "updateDoorDisplacement"),
            ("propobj", "doorFinishOpen"),
            ("propobj", "doorFinishClose"),
            ("propobj", "doorBuildClippedVertices"),
            ("propobj", "sub_GAME_7F043838"),
            ("propobj", "chrobjGetBboxFromObjFile"),
            ("chrprop", "chrpropDeregisterRoom"),
        ):
            pieces.append(function_declaration(sources[source], name))
    for pattern in (
        r"^s32 D_8002C904\s*=\s*0;",
        r"^s32 g_AnimationTablePointerCountRelated\s*=\s*0;",
        r"^s32 D_8002C90C\s*=\s*0;",
        r"^s32 D_8002C910\s*=\s*0;",
        r"^ModelRenderData D_8002CC6C\s*=",
        r"^coord3d D_8002CCAC\s*=\s*\{0, 0, 0\};",
    ):
        pieces.append(definition_text(sources["chr"], pattern))
    if full_props:
        for pattern in (
            r"^f32 slider_007_mode_damage\s*=\s*1\.0f;",
            r"^f32 slider_007_mode_accuracy\s*=\s*1\.0f;",
        ):
            pieces.append(definition_text(sources["front"], pattern))
        for pattern in (
            r"^struct LaserRichochetSounds laser_ricochet_sounds\s*=",
            r"^struct RicochetSoundsLarge ricochet_sounds_large\s*=",
        ):
            pieces.append(definition_text(sources["gun"], pattern))
        pieces.append(definition_text_occurrence(
            sources["fr"],
            r"^struct VideoSettings_s g_ViDataArray\[NUM_VIDEO_SETTINGS\]\s*=",
            1))
        pieces.append(definition_text(
            sources["fr"],
            r"^VideoSettings \*g_ViBackData\s*=\s*&g_ViDataArray\[0\];"))
        pieces.append(definition_text(
            sources["bondview"], r"^s32 g_PlayerTickCount\s*=\s*0;"))
        pieces.append(definition_text(
            sources["bondview"],
            r"^struct firing_anim_struct firing_animation_groups\[\]\[6\]\s*="))
    for pattern in (
        r"^f32 g_AiAccuracyModifier\s*=\s*1\.0f;",
        r"^f32 g_AiDamageModifier\s*=\s*1\.0f;",
    ):
        pieces.append(definition_text(sources["chr"], pattern))
    pieces.append(definition_text(
        sources["file2"], r"^ChrRecord \*g_CurModelChr;"))
    pieces.append(definition_text(
        sources["player"], r"^PLAYER_ID array_PLAYER_IDs\[4\];"))
    for pattern in (
        r"^s32 g_FogSkyIsEnabled;",
        r"^NearFogRecord \*g_NearFogValuesP;",
        r"^EnvironmentRecord \* g_EnvironmentMainp;",
        r"^EnvironmentRecord \* g_EnvironmentAltp;",
        r"^f32 g_ScaledFarFogIntensity\s*=\s*FLT_MAX;",
    ):
        pieces.append(definition_text(sources["bgfog"], pattern))
    if full_props:
        pieces.append(definition_text(sources["bgfog"], r"^struct FogDetails"))
        for pattern in (
            r"^f32 g_FarFogIntensity;",
            r"^f32 g_DifferenceFromFarFogIntensity;",
            r"^f32 g_ScaledDifferenceFromFarFogIntensity\s*=\s*0\.0;",
            r"^CurrentEnvironmentRecord g_CurrentEnvironment\s*=",
        ):
            pieces.append(definition_text(sources["bgfog"], pattern))
        for pattern in (
            r"^/\*[^\n]*\*/ struct PropRecord \* D_80030B0C\s*=\s*NULL;",
            r"^/\*[^\n]*\*/ s32 bodypartshot\s*=\s*0xFFFFFFFF;",
            r"^/\*[^\n]*\*/ f32 F_80030B14\s*=\s*1\.0;",
            r"^/\*[^\n]*\*/ f32 g_AutogunPendingDamageTick\s*=\s*1\.0;",
            r"^/\*[^\n]*\*/ f32 g_AutogunDamageScalar\s*=\s*1\.0;",
            r"^ModelRenderData D_80030B34\s*=",
        ):
            pieces.append(definition_text(sources["propobj"], pattern))
        for pattern in (
            r"^/\*[^\n]*\*/ LockDoorRecord \*g_LevelLoadPropLockDoor\s*=\s*NULL;",
            r"^/\*[^\n]*\*/ ObjectRecord \*g_LevelLoadPropSwitch\s*=\s*NULL;",
        ):
            pieces.append(definition_text(sources["propobj"], pattern))
        pieces.append(definition_text(
            sources["objective_status"],
            r"^struct criteria_deposit \*ptr_last_deposit_in_room_subobject_entry_type21;"))
        pieces.append(definition_text(
            sources["chrprop"],
            r"^Embedment g_Embedments\[EMBEDMENT_ARR_MAX\];"))
        for pattern in (
            r"^struct Model \*g_CurrentProjectileModel;",
            r"^struct ModelNode \* dword_CODE_bss_80075B74;",
            r"^coord3d flt_CODE_bss_80075B78;",
            r"^coord3d flt_CODE_bss_80075B88;",
        ):
            pieces.append(definition_text(sources["chrprop"], pattern))
    for pattern in (
        r"^coord3d g_CamFrustumTopNormal;",
        r"^f32 g_CamFrustumTopOffset;",
        r"^coord3d g_CamFrustumBottomNormal;",
        r"^f32 g_CamFrustumBottomOffset;",
        r"^coord3d g_CamFrustumLeftNormal;",
        r"^f32 g_CamFrustumLeftOffset;",
        r"^coord3d g_CamFrustumRightNormal;",
        r"^f32 g_CamFrustumRightOffset;",
        r"^f32 g_CamFrustumNearOffset;",
    ):
        pieces.append(definition_text(sources["bondview"], pattern))
    for pattern in (
        r"^/\*[^\n]*\*/ f32 toxic_gas_sound_timer\s*=\s*0\.0;",
        r"^/\*[^\n]*\*/ s32 D_80030ADC\s*=\s*0;",
        r"^/\*[^\n]*\*/ s32 clock_enable\s*=\s*0;",
        r"^/\*[^\n]*\*/ f32 clock_time\s*=\s*0;",
    ):
        pieces.append(definition_text(sources["propobj"], pattern))
    pieces.append(definition_text(
        sources["propobj"], r"^ModelRenderData D_800322A4\s*="))
    if full_props:
        pieces.append(definition_text(
            sources["chrprop"],
            r"^WeaponObjRecord\* proxy_mine_table\[30\];"))
        pieces.append(definition_text(
            sources["chrprop"], r"^s32 g_OnScreenPropCount;"))
        for pattern in (
            r"^StandTile \*firststaninroom\[139\];",
            r"^StanRoomBounds g_StanRoomBounds\[139\];",
            r"^s32 dword_CODE_bss_8007B9DC;",
        ):
            pieces.append(definition_text(sources["stan"], pattern))
        for pattern in (
            r"^BoundVec D_80044868\s*=",
            r"^BoundVec D_80044874\s*=",
            r"^BoundVec D_80044880\s*=",
            r"^BoundVec D_8004488C\s*=",
        ):
            pieces.append(definition_text(sources["bg"], pattern))
    selected_functions = FUNCTIONS + (FULL_PROPS_FUNCTIONS if full_props else ())
    pieces.extend(selected_function_text(sources[source], name)
                  for source, name in selected_functions)
    pieces.extend((
        "#undef bondviewUpdateFrustumPlanes",
        "#undef OP16_POS_Z_VOL", "#undef OP16_POS_Y_VOL",
        "#undef OP16_POS_X_VOL", "#undef OP16_NODEINDEX_10",
        "#undef OP16_NODEINDEX_0E", "#undef OP16_NODEINDEX_0C",
        "#undef FIVEPI_OVER_18", "#undef PI_OVER_3", ""))
    return "\n\n".join(pieces)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--full-props", action="store_true")
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.write_text(render(args.repo, args.full_props))
    count = len(FUNCTIONS) + (len(FULL_PROPS_FUNCTIONS)
                              if args.full_props else 0)
    print(f"generated {count} exact Dam guard scheduler services")


if __name__ == "__main__":
    main()
