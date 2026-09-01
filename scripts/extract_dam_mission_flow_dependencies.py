#!/usr/bin/env python3
"""Generate the audited dependency frontier for Dam stage AI list 0x1000."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path


def function_text(source: str, name: str) -> str:
    match = re.search(rf"(?m)^.*\b{name}\s*\([^;]*\)\s*\{{", source)
    if match is None:
        raise ValueError(f"missing canonical function {name}")
    if name == "ai":
        end_marker = "} // ai()"
        end = source.find(end_marker, match.start())
        if end < 0:
            raise ValueError("unterminated canonical function ai")
        return source[match.start():end + len(end_marker)]
    brace = source.index("{", match.start())
    depth = 0
    for pos in range(brace, len(source)):
        if source[pos] == "{":
            depth += 1
        elif source[pos] == "}":
            depth -= 1
            if depth == 0:
                return source[match.start():pos + 1]
    raise ValueError(f"unterminated canonical function {name}")


def digest(text: str) -> str:
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    setup_path = args.repo / "assets/obseg/setup/UsetupdamZ.c"
    chrai_path = args.repo / "src/game/chrai.c"
    stage_alloc_path = args.repo / "src/game/deb_loadallmodels.c"
    setup = setup_path.read_text(encoding="utf-8")
    chrai = chrai_path.read_text(encoding="utf-8")
    stage_alloc = stage_alloc_path.read_text(encoding="utf-8")
    list_match = re.search(r"(?ms)^u8 ai_20\[\] = \{.*?^\};", setup)
    if list_match is None:
        raise ValueError("missing authored Dam ai_20")
    list_text = list_match.group(0)
    commands = re.findall(r"(?m)^\s{4}([a-z][a-z0-9_]*)", list_text)
    expected = ["label", "ai_sleep", "if_objective_bitfield_is_set_on"]
    if commands[:3] != expected:
        raise ValueError(f"unexpected Dam list prefix: {commands[:3]}")

    functions = [
        "chraiitemsize",
        "chraiGetAIListID",
        "chraiGoToLabel",
        "ailistFindById",
        "ai",
    ]
    manifest = {
        "schema": 2,
        "authored_setup": {
            "path": str(setup_path.relative_to(args.repo)),
            "stage_list_symbol": "ai_20",
            "stage_list_id": "0x1000",
            "ailist_record_index": 20,
            "sha256": digest(list_text),
            "command_count": len(commands),
            "command_names": commands,
        },
        "canonical_interpreter": {
            "path": str(chrai_path.relative_to(args.repo)),
            "slice_macros": [
                "GE_PORT_DAM_MISSION_FLOW_SLICE",
                "GE_PORT_DAM_MISSION_SFX_SLICE",
            ],
            "functions": [
                {"name": name, "sha256": digest(function_text(chrai, name))}
                for name in functions
            ],
            "linked_commands": [
                {"name": "goto_next", "id": "0x00"},
                {"name": "goto_first", "id": "0x01"},
                {"name": "label", "id": "0x02"},
                {"name": "ai_sleep", "id": "0x03"},
                {"name": "ai_list_end", "id": "0x04"},
                {"name": "if_item_is_stationary_within_level", "id": "0x57"},
                {"name": "if_item_is_attached_to_object", "id": "0x58"},
                {"name": "if_object_not_destroyed", "id": "0x5b"},
                {"name": "set_objective_bitfield", "id": "0x9a"},
                {"name": "if_objective_bitfield_is_set_on", "id": "0x9c"},
                {"name": "text_print_top", "id": "0xc3"},
                {"name": "sfx_play", "id": "0xc4"},
                {"name": "sfx_emit_from_object", "id": "0xc5"},
            ],
        },
        "canonical_stage_actor_allocation": {
            "path": str(stage_alloc_path.relative_to(args.repo)),
            "slice_macro": "GE_PORT_DAM_STAGE_AI_ALLOC_SLICE",
            "function": "alloc_false_GUARDdata_to_exec_global_action",
            "sha256": digest(function_text(
                stage_alloc, "alloc_false_GUARDdata_to_exec_global_action")),
            "dam_background_actor_count": 8,
            "storage_dependency": "MEMPOOL_STAGE",
        },
        "verified_transitions": [
            {
                "name": "initial_yield",
                "entry_offset": 0,
                "published_yield_offset": 3,
                "commands": ["label(0x2a)", "ai_sleep"],
            },
            {
                "name": "objective_complete_terminal_loop",
                "entry_offset": 3,
                "published_yield_offset": 113,
                "commands": [
                    "if_objective_bitfield_is_set_on(0x00040000, 0x04)",
                    "label(0x04)",
                    "ai_sleep",
                ],
                "authored_objective_argument": "0x00040000",
                "runtime_objective_bit": "0x00000400",
            },
            {
                "name": "healthy_tag_normal_loop",
                "entry_offset": 3,
                "published_yield_offset": 3,
                "commands": [
                    "if_objective_bitfield_is_set_on(0x00040000, 0x04)",
                    "if_object_not_destroyed(0x05, 0x07)",
                    "if_object_not_destroyed(0x04, 0x07)",
                    "if_objective_bitfield_is_set_on(0x00010000, 0x07)",
                    "if_objective_bitfield_is_set_on(0x00200000, 0x07)",
                    "if_item_is_attached_to_object(0x2f, 0x05, 0x0a)",
                    "if_item_is_stationary_within_level(0x2f, 0x0d)",
                    "goto_first(0x2a)",
                    "ai_sleep",
                ],
                "authored_tags": [5, 4],
            },
            {
                "name": "tracker_attached_normal_loop",
                "entry_offset": 3,
                "published_yield_offset": 3,
                "condition": "item 0x2f is attached to authored tag 5",
                "commands": [
                    "if_item_is_attached_to_object(0x2f, 0x05, 0x0a)",
                    "set_objective_bitfield(0x00010000)",
                    "text_print_top(LdamE[8])",
                    "sfx_play(0xe300, 0x00)",
                    "sfx_emit_from_object(0x00, 0x05, 0x0000)",
                    "goto_first(0x2a)",
                    "ai_sleep",
                ],
                "runtime_objective_bit": "0x00000100",
                "hud_text": "LdamE[8]",
                "runtime_sfx_id": "0x00e3",
                "emitter_tag": 5,
            },
            {
                "name": "tag5_destroyed_terminal_transition",
                "entry_offset": 3,
                "published_yield_offset": 113,
                "condition": "objIsHealthy(objFindByTagId(5)) == FALSE",
                "commands": [
                    "if_object_not_destroyed(0x05, 0x07)",
                    "set_objective_bitfield(0x00080000)",
                    "text_print_top(LdamE[14])",
                    "set_objective_bitfield(0x00020000)",
                    "goto_next(0x04)",
                    "label(0x04)",
                    "ai_sleep",
                ],
                "authored_tag": 5,
                "authored_objective_arguments": [
                    "0x00080000",
                    "0x00020000",
                ],
                "runtime_objective_bits": [
                    "0x00000800",
                    "0x00000200",
                ],
                "runtime_objective_result": "0x00000a00",
                "hud_text": "LdamE[14]",
                "damage_boundary": "completed PropDef destroyed state",
            },
        ],
        "next_dependency_frontier": [
            {
                "condition": "tag 5/4 objects enter the visible object lifecycle",
                "canonical_dependencies": [
                    "native model 335/70 relocation",
                    "unchanged setup object constructors",
                    "projectile/object damage publication",
                ],
            },
        ],
        "later_side_effect_families": [
            "native projectile/object damage path",
            "remaining Dam background AI lists 0x1001-0x1007",
        ],
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
