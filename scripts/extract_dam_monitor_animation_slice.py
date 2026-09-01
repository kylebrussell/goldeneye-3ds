#!/usr/bin/env python3
"""Extract GoldenEye's exact monitor interpreter and authored image table."""

from __future__ import annotations

import argparse
import re
from pathlib import Path


def extract_braced(source: str, start: int) -> str:
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                semi = pos + 1
                if semi < len(source) and source[semi] == ";":
                    semi += 1
                return source[start:semi]
    raise ValueError("unterminated braced declaration")


def extract_function(source: str, name: str) -> str:
    match = re.search(
        rf"(?m)^[A-Za-z_][^\n;{{}}]*\b{name}\s*\([^;\n]*\)[^;{{}}]*\{{",
        source,
    )
    if match is None:
        raise ValueError(f"missing {name}")
    return extract_braced(source, match.start())


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    propobj = (args.repo / "src/game/propobj.c").read_text()
    initobjects = (args.repo / "src/game/initobjects.c").read_text()
    oddtextures = (args.repo / "assets/oddtextures.c").read_text()
    image_externs = (args.repo / "assets/image_externs.h").read_text()
    tvcmd_start = propobj.index("struct tvcmd {")
    tvcmd = extract_braced(propobj, tvcmd_start)
    save_pointer = extract_function(
        propobj, "save_ptr_monitor_ani_code_to_obj_ani_slot")
    save_image = extract_function(propobj, "save_img_index_to_obj_ani_slot")
    interpreter = extract_function(
        propobj, "process_monitor_animation_microcode")
    commands_start = propobj.index("u32 monAnim00Bond[]")
    commands_end = propobj.index("ModelRenderData D_80031FD0", commands_start)
    commands = propobj[commands_start:commands_end]
    command_names = re.findall(r"(?m)^u32\s+(mon\w+)\[\]\s*=", commands)
    if not command_names:
        raise ValueError("missing monitor command arrays")
    jump_targets = sorted(set(re.findall(
        r"MONJUMP(?:TO|CHANCE)\(\s*(mon\w+)", commands)))
    missing_targets = [name for name in jump_targets
                       if name not in command_names]
    if missing_targets:
        raise ValueError(f"missing monitor jump targets: {missing_targets}")
    command_ids = {name: 0x80000000 + index
                   for index, name in enumerate(jump_targets)}
    for name, command_id in command_ids.items():
        commands = re.sub(
            rf"(MONJUMP(?:TO|CHANCE)\(\s*){name}\b",
            rf"\1GE_MONITOR_COMMAND_ID_{name}", commands)
    renamed_commands = {name: f"ge_original_{name}"
                        for name in command_names}
    for name, renamed in renamed_commands.items():
        commands = re.sub(rf"\b{name}\b", renamed, commands)
    commands = re.sub(r"(?m)^u32\s+", "static u32 ", commands)
    command_id_constants = "\n".join(
        f"#define GE_MONITOR_COMMAND_ID_{name} UINT32_C(0x{command_id:08x})"
        for name, command_id in command_ids.items())
    command_resolver_cases = "\n".join(
        f"    case GE_MONITOR_COMMAND_ID_{name}: "
        f"return {renamed_commands[name]};"
        for name in jump_targets)
    initial_match = re.search(
        r"MonitorRecord\s+g_InitialMonitorAnimController\s*=", initobjects)
    if initial_match is None:
        raise ValueError("missing initial monitor controller")
    initial_controller = initobjects[
        initial_match.start():initobjects.index(";", initial_match.start()) + 1]
    initial_controller = initial_controller.replace(
        "MonitorRecord g_InitialMonitorAnimController",
        "static const MonitorRecord ge_original_initial_monitor_controller",
        1)
    for name, renamed in renamed_commands.items():
        initial_controller = re.sub(
            rf"\b{name}\b", renamed, initial_controller)
    initial_controller = initial_controller.replace(
        "&ge_original_mon", "ge_original_mon")
    set_image = extract_function(propobj, "monitorSetImageByNum")
    # Preserve the canonical public entry point: both the generic stage
    # monitor adapter and unchanged chrai command 0x01a6 call this body.
    set_image = set_image.replace("s32 *image", "u32 *image", 1)
    for name, renamed in renamed_commands.items():
        set_image = re.sub(rf"\b{name}\b", renamed, set_image)
    set_image = set_image.replace("&ge_original_mon", "ge_original_mon")
    set_image = set_image.replace(
        "save_ptr_monitor_ani_code_to_obj_ani_slot(mon,  image);",
        "mon->cmdlist = (u32 *)image;\n    mon->offset = 0;")
    if "save_ptr_monitor_ani_code_to_obj_ani_slot" in set_image:
        raise ValueError("monitorSetImageByNum command-list assignment changed")
    pointer_cast = "(u32 *) m->time"
    if interpreter.count(pointer_cast) != 2:
        raise ValueError("monitor jump pointer ABI changed")
    interpreter = interpreter.replace(
        pointer_cast,
        "ge_original_monitor_resolve_command_list((u32)m->time)")
    images_start = oddtextures.index("sImageTableEntry s_monitorimages[]")
    images = extract_braced(oddtextures, images_start).replace(
        "s_monitorimages[]", "ge_original_monitor_images[]", 1)
    image_enum_start = image_externs.index("typedef enum IMAGEIDS")
    image_enum = extract_braced(image_externs, image_enum_start)
    image_enum_body = image_enum[image_enum.index("{") + 1:image_enum.rindex("}")]
    image_enum_body = re.sub(r"//[^\n]*|/\*.*?\*/", "", image_enum_body,
                             flags=re.S)
    image_ids = [item.strip() for item in image_enum_body.split(",")
                 if item.strip()]
    image_values = {name: index for index, name in enumerate(image_ids)}
    used_image_ids = sorted(set(re.findall(r"\bIMAGE_[A-Z0-9_]+\b", images)))
    missing_image_ids = [name for name in used_image_ids
                         if name not in image_values]
    if missing_image_ids:
        raise ValueError(f"missing image IDs: {missing_image_ids}")
    image_id_constants = "\n".join(
        f"#define {name} {image_values[name]}U" for name in used_image_ids)

    output = f'''/* Generated unchanged GoldenEye monitor interpreter. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "assets/oddtextures.h"
#include "game/chrai.h"
#include "ge_original_dam_monitor_render.h"
#include "ge_original_stage_monitor.h"

extern s32 g_ClockTimer;
extern f32 g_GlobalTimerDelta;
extern u32 randomGetNext(void);

#define MONITOR_TIMER_DELTA g_GlobalTimerDelta
#ifndef M_U16_MAX_VALUE_F
#define M_U16_MAX_VALUE_F 65536.0f
#endif

/* Canonical numeric values from the disabled autogenerated IMAGEIDS table. */
{image_id_constants}

{tvcmd}

/* The original command ABI stores branch destinations in one 32-bit word.
 * Host pointers are wider, so generated stable IDs occupy only those pointer
 * operands and are relocated back to their exact authored command arrays at
 * the unchanged interpreter boundary. ARM uses the same deterministic bank. */
{command_id_constants}

{commands}

static u32 *ge_original_monitor_resolve_command_list(u32 command_id)
{{
    switch (command_id) {{
{command_resolver_cases}
    default: return NULL;
    }}
}}

{initial_controller}

{set_image}

void ge_original_stage_monitor_set_image_exact(
    void *monitor_record, int32_t image_num)
{{
    MonitorRecord *monitor = monitor_record;
    if (monitor == NULL) return;
    monitorSetImageByNum(monitor, image_num);
}}

int ge_original_stage_monitor_controller_initialize(
    void *monitor_record, int32_t image_num)
{{
    MonitorRecord *monitor = monitor_record;
    if (monitor == NULL || image_num < 0 || image_num > 51) return 0;
    *monitor = ge_original_initial_monitor_controller;
    monitorSetImageByNum(monitor, image_num);
    return 1;
}}

const void *ge_original_stage_monitor_command_list(int32_t image_num)
{{
    MonitorRecord monitor;
    if (!ge_original_stage_monitor_controller_initialize(
            &monitor, image_num)) return NULL;
    return monitor.cmdlist;
}}

static {images}

typedef struct GeDamMonitorCapture {{
    Vertex vertices[4];
    Gfx display_list[16];
    sImageTableEntry *texture;
    u32 texture_alpha_mode;
    s32 texture_mode;
    int failed;
}} GeDamMonitorCapture;

static GeDamMonitorCapture *ge_dam_monitor_capture;

static Vertex *ge_dam_monitor_dyn_allocate_vertices(s32 count)
{{
    if (ge_dam_monitor_capture == NULL || count != 4) return NULL;
    return ge_dam_monitor_capture->vertices;
}}

static union ModelRwData *ge_dam_monitor_model_get_rw_data(
    Model *model, ModelNode *node)
{{
    u16 index;
    if (model == NULL || model->datas == NULL || node == NULL
            || node->Data == NULL
            || (node->Opcode & 0xff) != MODELNODE_OPCODE_DLCOLLISION)
        return NULL;
    index = node->Data->DisplayListCollisions.RwDataIndex;
    if (index >= (u16)model->rwdatalen) return NULL;
    return (union ModelRwData *)(void *)&model->datas[index];
}}

static void ge_dam_monitor_tex_select(Gfx **gdlptr,
    sImageTableEntry *texture, u32 alpha_mode, s32 texture_mode, u32 ulst)
{{
    (void)gdlptr;
    (void)ulst;
    if (ge_dam_monitor_capture == NULL) return;
    ge_dam_monitor_capture->texture = texture;
    ge_dam_monitor_capture->texture_alpha_mode = alpha_mode;
    ge_dam_monitor_capture->texture_mode = texture_mode;
}}

static u32 ge_dam_monitor_virtual_to_physical(void *address)
{{
    return (u32)(uintptr_t)address;
}}

#define dynAllocateVertices ge_dam_monitor_dyn_allocate_vertices
#define modelGetNodeRwData ge_dam_monitor_model_get_rw_data
#define texSelect ge_dam_monitor_tex_select
#define monitorimages ge_original_monitor_images
#define osVirtualToPhysical ge_dam_monitor_virtual_to_physical
#define save_ptr_monitor_ani_code_to_obj_ani_slot \
    ge_original_dam_monitor_save_command_list
#define save_img_index_to_obj_ani_slot \
    ge_original_dam_monitor_save_image_index
#define process_monitor_animation_microcode \
    ge_original_process_monitor_animation_microcode

{save_pointer}

{save_image}

{interpreter}

#undef process_monitor_animation_microcode
#undef save_img_index_to_obj_ani_slot
#undef save_ptr_monitor_ani_code_to_obj_ani_slot
#undef osVirtualToPhysical
#undef monitorimages
#undef texSelect
#undef modelGetNodeRwData
#undef dynAllocateVertices

int ge_original_stage_monitor_render_screen_tick(
    void *model_instance, void *monitor_record,
    uint32_t object_flags, uint32_t object_flags2,
    size_t screen_slot, GeOriginalDamMonitorRenderSnapshot *snapshot)
{{
    Model *model = model_instance;
    MonitorRecord *monitor = monitor_record;
    GeDamMonitorCapture capture;
    ModelNode *screen_node;
    Gfx *end;
    size_t index;
    s32 texture_mode;
    uintptr_t image_slot;

    if (model == NULL || monitor == NULL || snapshot == NULL
            || model->obj == NULL || model->obj->Switches == NULL
            || model->obj->numSwitches <= 0
            || (size_t)model->obj->numSwitches <= screen_slot)
        return 0;
    screen_node = model->obj->Switches[screen_slot];
    if (screen_node == NULL || screen_node->Data == NULL
            || (screen_node->Opcode & 0xff)
                != MODELNODE_OPCODE_DLCOLLISION
            || screen_node->Data->DisplayListCollisions.numVertices < 4)
        return 0;
    texture_mode = (object_flags2 & PROPFLAG2_00010000) != 0U ? 0
        : (object_flags & PROPFLAG_FIXED_MONITOR) != 0U ? 8 : 1;
    memset(&capture, 0, sizeof(capture));
    ge_dam_monitor_capture = &capture;
    end = ge_original_process_monitor_animation_microcode(
        model, screen_node, monitor, capture.display_list,
        texture_mode, 1);
    ge_dam_monitor_capture = NULL;
    if (end == NULL || capture.texture == NULL
            || end < capture.display_list
            || end > capture.display_list
                + sizeof(capture.display_list) / sizeof(capture.display_list[0]))
        return 0;

    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->switch_node = screen_node;
    snapshot->texture_config = capture.texture;
    snapshot->texture_id = (uint32_t)capture.texture->index;
    image_slot = (uintptr_t)monitor->tconfig;
    snapshot->image_slot = image_slot < 100U ? (uint8_t)image_slot : 0xffU;
    snapshot->width = capture.texture->width;
    snapshot->height = capture.texture->height;
    snapshot->level = capture.texture->level;
    snapshot->format = capture.texture->format;
    snapshot->depth = capture.texture->depth;
    snapshot->flags_s = capture.texture->flagsS;
    snapshot->flags_t = capture.texture->flagsT;
    snapshot->texture_mode = (uint8_t)capture.texture_mode;
    snapshot->texture_alpha_mode = (uint8_t)capture.texture_alpha_mode;
    snapshot->command_offset = monitor->offset;
    snapshot->pause60 = monitor->pause60;
    for (index = 0U; index < 4U; index++) {{
        snapshot->vertices[index].x = capture.vertices[index].coord.x;
        snapshot->vertices[index].y = capture.vertices[index].coord.y;
        snapshot->vertices[index].z = capture.vertices[index].coord.z;
        snapshot->vertices[index].s = capture.vertices[index].s;
        snapshot->vertices[index].t = capture.vertices[index].t;
        snapshot->vertices[index].red = capture.vertices[index].r;
        snapshot->vertices[index].green = capture.vertices[index].g;
        snapshot->vertices[index].blue = capture.vertices[index].b;
        snapshot->vertices[index].alpha = capture.vertices[index].a;
    }}
    return 1;
}}

int ge_original_dam_monitor_render_tick(
    void *model_instance, void *monitor_record,
    uint32_t object_flags, uint32_t object_flags2,
    GeOriginalDamMonitorRenderSnapshot *snapshot)
{{
    return ge_original_stage_monitor_render_screen_tick(
        model_instance, monitor_record, object_flags, object_flags2,
        0U, snapshot);
}}
'''
    args.output.write_text(output)
    print("generated unchanged monitor interpreter and exact image table "
          f"-> {args.output}")


if __name__ == "__main__":
    main()
