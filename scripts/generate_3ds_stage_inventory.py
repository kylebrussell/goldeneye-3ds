#!/usr/bin/env python3
"""Inventory the authored assets needed for every solo stage after Facility.

The level/asset association is read from the original decompiled tables.  The
normal spawn and direct setup model dependencies are read from the generated US
setup source and checked against the compiled setup blob.  No coordinates or
asset aliases are supplied by this script.
"""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import struct
import sys
import zlib


BG_BASE = 0x0F000000
ROOM_RECORD_SIZE = 24
PAD_RECORD_SIZE = 44
INTRO_SIZES = {0: 12, 1: 16, 2: 16, 3: 32, 4: 8,
               5: 8, 6: 40, 7: 12, 8: 8}

# These are the object-bearing cases in the original setupCommandGetObject.
# Their second setup word is ObjectRecord.obj, the direct PitemZ model id.
OBJECT_PROPDEF_TYPES = {
    1, 3, 4, 5, 6, 7, 8, 10, 11, 12, 13, 17, 20, 21,
    36, 39, 40, 41, 42, 43, 45, 47,
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def relative(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def int_literal(value: str) -> int:
    return int(value.strip(), 0)


def initializer(text: str, declaration: str) -> str:
    start = text.index(declaration)
    opening = text.index("{", start)
    depth = 0
    for index in range(opening, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[opening + 1:index]
    raise ValueError(f"unterminated initializer: {declaration}")


def parse_level_ids(constants: str) -> dict[str, int]:
    body = initializer(constants, "typedef enum LEVELID")
    values: dict[str, int] = {}
    current = -1
    for raw in body.split(","):
        entry = re.sub(r"//.*", "", raw).strip()
        if not entry or not entry.startswith("LEVELID_"):
            continue
        if "=" in entry:
            name, expression = (part.strip() for part in entry.split("=", 1))
            if re.fullmatch(r"-?(?:0x[0-9a-fA-F]+|\d+)", expression):
                current = int_literal(expression)
            else:
                match = re.fullmatch(r"(LEVELID_[A-Z0-9_]+)\s*\+\s*([A-Z0-9_]+|\d+)",
                                     expression)
                if match is None or match.group(1) not in values:
                    raise ValueError(f"unsupported LEVELID expression: {expression}")
                # Multiplayer aliases follow LEVELID_MAX and are irrelevant to
                # this inventory; their symbolic player offset need not be
                # duplicated here.
                if not match.group(2).isdigit():
                    continue
                current = values[match.group(1)] + int(match.group(2))
        else:
            name = entry
            current += 1
        values[name] = current
    return values


def parse_solo_order(boss: str) -> list[str]:
    body = initializer(boss, "struct memallocstring memallocstringtable[]")
    entries = re.findall(r"\{\s*(LEVELID_[A-Z0-9_]+)\s*,\s*\"", body)
    try:
        end = entries.index("LEVELID_EGYPT")
    except ValueError as error:
        raise ValueError("solo stage allocation table has no Egypt terminator") from error
    result = entries[:end + 1]
    if len(result) != 20 or result[:2] != ["LEVELID_DAM", "LEVELID_FACILITY"]:
        raise ValueError(f"unexpected solo stage order: {result}")
    return result


def parse_setup_table(chraidata: str) -> list[str | None]:
    body = initializer(chraidata, "char *setup_text_pointers[]")
    values = re.findall(r'"([^"]+)"|\b(NULL)\b', body)
    return [quoted if quoted else None for quoted, null in values]


def parse_background_table(bg: str) -> dict[str, tuple[str, str]]:
    body = initializer(bg, "struct levelentry levelinfotable[]")
    result: dict[str, tuple[str, str]] = {}
    pattern = re.compile(
        r"\{\s*(LEVELID_[A-Z0-9_]+)\s*,\s*\"bg/bg_([a-z0-9]+)_all_p\.seg\"\s*,"
        r"\s*\"(Tbg_[a-z0-9]+_all_p_stanZ)\""
    )
    for stage, alias, stan in pattern.findall(body):
        result[stage] = (alias, stan)
    return result


def source_for(root: Path, directory: str, stem: str) -> Path:
    candidates = [
        root / "assets/obseg" / directory / f"{stem}.c",
        root / "assets/obseg" / directory / "u" / f"{stem}.c",
    ]
    matches = [path for path in candidates if path.is_file()]
    if len(matches) != 1:
        raise ValueError(f"expected one US source for {stem}, found {matches}")
    return matches[0]


def parse_pads(setup_text: str) -> list[dict[str, object]]:
    body = initializer(setup_text, "PadRecord padlist[] =")
    pattern = re.compile(
        r'^\s*\{\s*\{([^}]*)\},\s*\{([^}]*)\},\s*\{([^}]*)\},\s*'
        r'(?:"([^"]*)"|NULL),\s*(?:0|NULL)\s*\},', re.M
    )

    def vector(value: str) -> list[float]:
        result = [float(item.strip().removesuffix("f")) for item in value.split(",")]
        if len(result) != 3:
            raise ValueError(f"invalid setup vector: {value}")
        return result

    pads = [{"position": vector(match.group(1)),
             "up": vector(match.group(2)),
             "look": vector(match.group(3)),
             "stan_name": match.group(4)} for match in pattern.finditer(body)]
    if not pads:
        raise ValueError("setup has no ordinary pads")
    return pads


def parse_intro_source(setup_text: str) -> tuple[list[dict[str, int]], list[int]]:
    body = initializer(setup_text, "s32 intro[] =")
    spawns = [
        {"record_index": int(index), "pad_index": int(pad), "demo_slot": int(demo)}
        for index, pad, demo in re.findall(
            r'/\* Type = Spawn; index = (\d+) \*/\s*\n'
            r'\s*_mkword\([^\n]+\),\s*(-?\d+),\s*(-?\d+),', body)
    ]
    normal = [spawn for spawn in spawns if spawn["demo_slot"] == 0]
    if len(normal) != 1:
        raise ValueError(f"expected one normal intro spawn, found {normal}")
    items = [
        int(item)
        for item, left, demo in re.findall(
            r'/\* Type = StartWeapon; index = \d+ \*/\s*\n'
            r'\s*_mkword\([^\n]+\),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),', body)
        if int(demo) == 0
    ]
    # Preserve an explicitly authored left-hand item as its own dependency.
    items.extend(
        int(left)
        for item, left, demo in re.findall(
            r'/\* Type = StartWeapon; index = \d+ \*/\s*\n'
            r'\s*_mkword\([^\n]+\),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),', body)
        if int(demo) == 0 and int(left) >= 0
    )
    return spawns, sorted(set(items))


def parse_intro_binary(setup: bytes) -> list[dict[str, int]]:
    if len(setup) < 40:
        raise ValueError("compiled setup is shorter than its ten-word header")
    intro_offset = struct.unpack_from(">I", setup, 8)[0]
    if intro_offset < 40 or intro_offset >= len(setup) or intro_offset % 4:
        raise ValueError("compiled setup intro offset is invalid")
    records: list[dict[str, int]] = []
    offset = intro_offset
    record_index = 0
    while True:
        if offset + 4 > len(setup):
            raise ValueError("compiled setup intro is unterminated")
        record_type = struct.unpack_from(">I", setup, offset)[0]
        if record_type == 9:
            break
        size = INTRO_SIZES.get(record_type)
        if size is None or offset + size > len(setup):
            raise ValueError(f"invalid compiled intro type {record_type}")
        if record_type == 0:
            _type, pad, demo = struct.unpack_from(">3i", setup, offset)
            records.append({"record_index": record_index,
                            "pad_index": pad, "demo_slot": demo})
        offset += size
        record_index += 1
    return records


def parse_setup_dependencies(setup_text: str) -> dict[str, object]:
    body = initializer(setup_text, "s32 propDefs[] =")
    comments = list(re.finditer(r'/\* Type = ([^;]+); index = (\d+) \*/', body))
    if not comments:
        raise ValueError("setup has no prop definition records")
    type_counts: Counter[str] = Counter()
    prop_models: list[int] = []
    body_ids: list[int] = []
    head_ids: list[int] = []
    guard_count = 0
    simple_words = re.compile(
        r'_mkword\(\s*(-?(?:0x[0-9a-fA-F]+|\d+))\s*,\s*'
        r'(-?(?:0x[0-9a-fA-F]+|\d+))\s*\)'
    )
    for position, match in enumerate(comments):
        index = int(match.group(2))
        if index != position:
            raise ValueError(f"non-contiguous prop index {index}, expected {position}")
        end = comments[position + 1].start() if position + 1 < len(comments) else len(body)
        record = body[match.end():end]
        header = re.search(r'_mkshort\(\s*0\s*,\s*(\d+)\s*\)', record)
        if header is None:
            raise ValueError(f"prop record {index} has no numeric type")
        record_type = int(header.group(1))
        type_counts[match.group(1)] += 1
        words = [(int_literal(a), int_literal(b)) for a, b in simple_words.findall(record)]
        if record_type in OBJECT_PROPDEF_TYPES:
            if not words:
                raise ValueError(f"object prop record {index} has no model/pad word")
            prop_models.append(words[0][0])
        elif record_type == 9:
            if len(words) < 4:
                raise ValueError(f"guard record {index} is truncated")
            guard_count += 1
            body_ids.append(words[1][0])
            head = words[-1][1]
            if head not in (-1, 0xffff):
                head_ids.append(head)
    if int(re.search(r'_mkshort\(\s*0\s*,\s*(\d+)\s*\)',
                     body[comments[-1].end():]).group(1)) != 48:
        raise ValueError("last setup prop record is not PROPDEF_END")
    unique_prop_models = sorted(set(prop_models))
    unique_body_ids = sorted(set(body_ids))
    unique_head_ids = sorted(set(head_ids))
    return {
        "prop_record_count_including_end": len(comments),
        "prop_record_type_counts": dict(sorted(type_counts.items())),
        "direct_prop_model_instance_count": len(prop_models),
        "unique_prop_model_count": len(unique_prop_models),
        "unique_prop_model_ids": unique_prop_models,
        "guard_count": guard_count,
        "unique_guard_body_count": len(unique_body_ids),
        "unique_guard_body_ids": unique_body_ids,
        "unique_explicit_guard_head_count": len(unique_head_ids),
        "unique_explicit_guard_head_ids": unique_head_ids,
    }


def validate_compiled_spawn(setup: bytes, source_spawns: list[dict[str, int]],
                            pads: list[dict[str, object]]) -> None:
    if parse_intro_binary(setup) != source_spawns:
        raise ValueError("compiled setup intro spawns do not match generated source")
    spawn = next(item for item in source_spawns if item["demo_slot"] == 0)
    pad_index = spawn["pad_index"]
    if not 0 <= pad_index < len(pads):
        raise ValueError(f"normal spawn pad {pad_index} is out of range")
    pads_offset = struct.unpack_from(">I", setup, 24)[0]
    offset = pads_offset + pad_index * PAD_RECORD_SIZE
    if offset + PAD_RECORD_SIZE > len(setup):
        raise ValueError("compiled normal spawn pad is outside setup")
    vectors = list(struct.unpack_from(">9f", setup, offset))
    expected = pads[pad_index]["position"] + pads[pad_index]["up"] + pads[pad_index]["look"]
    if any(abs(actual - wanted) > 0.0001 for actual, wanted in zip(vectors, expected)):
        raise ValueError("compiled normal spawn pad vectors do not match source")
    name_offset = struct.unpack_from(">I", setup, offset + 36)[0]
    if not 0 < name_offset < len(setup):
        raise ValueError("compiled normal spawn STAN name offset is invalid")
    end = setup.find(b"\0", name_offset)
    if end < 0:
        raise ValueError("compiled normal spawn STAN name is unterminated")
    name = setup[name_offset:end].decode("ascii")
    if name != pads[pad_index]["stan_name"]:
        raise ValueError("compiled normal spawn STAN name does not match source")


def inflate_stream(background: bytes, start: int, end: int) -> bytes:
    span = background[start - BG_BASE:end - BG_BASE]
    if len(span) < 3 or span[:2] != b"\x11\x72":
        raise ValueError(f"background stream 0x{start:08x} lacks Rare 1172 header")
    inflater = zlib.decompressobj(-15)
    decoded = inflater.decompress(span[2:]) + inflater.flush()
    if not inflater.eof or inflater.unconsumed_tail or any(inflater.unused_data):
        raise ValueError(f"background stream 0x{start:08x} is malformed")
    return decoded


def analyze_background(background: bytes) -> dict[str, object]:
    if len(background) < 20:
        raise ValueError("background is shorter than its header")
    _root, rooms_address, portals_address, visibility_address, _environment = \
        struct.unpack_from(">5I", background)
    room_start = rooms_address - BG_BASE
    room_end = visibility_address - BG_BASE
    portal_start = portals_address - BG_BASE
    if room_start != 20 or room_end <= room_start or portal_start < room_end \
            or portal_start > len(background) or (room_end - room_start) % 24:
        raise ValueError("background tables have invalid bounds")
    records = [struct.unpack_from(">IIIfff", background, offset)
               for offset in range(room_start, room_end, 24)]
    if len(records) < 3 or any(records[0]) or any(records[-1]):
        raise ValueError("background room table lacks canonical sentinels")
    rooms = records[:-2]
    sentinel = records[-2]
    starts = {address for room in rooms[1:] for address in room[:3] if address}
    bounds = starts | {address for address in sentinel[:3] if address}
    if not starts or len(bounds) < 2:
        raise ValueError("background has no bounded room streams")
    ordered = sorted(bounds)
    stream_ends = {start: next((item for item in ordered if item > start), 0)
                   for start in starts}
    if any(not end for end in stream_ends.values()):
        raise ValueError("background room stream lacks an upper bound")
    textures: set[int] = set()
    vertices = 0
    display_commands = 0
    for room_index, room in enumerate(rooms[1:], 1):
        for stream_index, address in enumerate(room[:3]):
            if not address:
                continue
            decoded = inflate_stream(background, address, stream_ends[address])
            if stream_index == 0:
                if len(decoded) % 16:
                    raise ValueError(f"room {room_index} vertex stream is misaligned")
                vertices += len(decoded) // 16
            else:
                if len(decoded) % 8:
                    raise ValueError(f"room {room_index} display list is misaligned")
                display_commands += len(decoded) // 8
                for offset in range(0, len(decoded), 8):
                    word0, word1 = struct.unpack_from(">II", decoded, offset)
                    if word0 >> 24 == 0xC0:
                        textures.add(word1 & 0xFFF)
    portal_count = 0
    while True:
        offset = portal_start + portal_count * 8
        if offset + 8 > len(background):
            raise ValueError("unterminated background portal table")
        geometry = struct.unpack_from(">I", background, offset)[0]
        if geometry == 0:
            break
        if not BG_BASE <= geometry < BG_BASE + len(background):
            raise ValueError(f"portal {portal_count} geometry is outside background")
        portal_count += 1
        if portal_count > 200:
            raise ValueError("background exceeds original PORTMAX")
    visibility_size = portal_start - room_end
    if visibility_size % 8:
        raise ValueError("visibility command table is misaligned")
    return {
        "room_count_including_dummy_room_0": len(rooms),
        "playable_room_slot_count": len(rooms) - 1,
        "portal_count": portal_count,
        "visibility_record_count": visibility_size // 8,
        "room_vertex_count": vertices,
        "room_display_list_command_count": display_commands,
        "unique_rare_texture_count": len(textures),
        "unique_rare_texture_ids": sorted(textures),
    }


def load_stan_parser(root: Path):
    path = root / "scripts/extract_3ds_dam_collision.py"
    spec = importlib.util.spec_from_file_location("ge_stage_inventory_stan", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load STAN parser: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def stan_pack_id(name: str) -> int:
    match = re.fullmatch(r"([pq])(\d+)([a-z])([0-7]?)", name)
    if match is None:
        raise ValueError(f"invalid authored STAN name: {name}")
    prefix, number, letter, subtriangle = match.groups()
    high = ((ord(prefix) - ord("p")) << 15) | int(number)
    low = (ord(letter) - ord("a")) * 8 + (int(subtriangle) if subtriangle else 0)
    return (high << 8) | low


def stage_label(symbol: str) -> str:
    special = {"LEVELID_BUNKER1": "Bunker 1", "LEVELID_BUNKER2": "Bunker 2",
               "LEVELID_SURFACE": "Surface 1", "LEVELID_SURFACE2": "Surface 2",
               "LEVELID_EGYPT": "Egyptian"}
    return special.get(symbol, symbol.removeprefix("LEVELID_").title())


def file_identity(path: Path, root: Path) -> dict[str, object]:
    data = path.read_bytes()
    return {"path": relative(path, root), "size": len(data), "sha256": sha256(data)}


def generate(root: Path) -> dict[str, object]:
    constants_path = root / "src/bondconstants.h"
    boss_path = root / "src/boss.c"
    chraidata_path = root / "src/game/chraidata.c"
    bg_table_path = root / "src/game/bg.c"
    level_ids = parse_level_ids(constants_path.read_text())
    solo_order = parse_solo_order(boss_path.read_text())
    setups = parse_setup_table(chraidata_path.read_text())
    backgrounds = parse_background_table(bg_table_path.read_text())
    stan_parser = load_stan_parser(root)
    stages = []
    for sequence, symbol in enumerate(solo_order[2:], 2):
        level_id = level_ids[symbol]
        if level_id >= len(setups) or setups[level_id] is None:
            raise ValueError(f"{symbol} has no canonical setup mapping")
        setup_stem = setups[level_id]
        if symbol not in backgrounds:
            raise ValueError(f"{symbol} has no canonical background mapping")
        bg_alias, stan_stem = backgrounds[symbol]
        background_bin = root / f"build/u/assets/obseg/bg/bg_{bg_alias}_all_p.bin"
        setup_bin = root / f"build/u/assets/obseg/setup/{setup_stem}.bin"
        stan_bin = root / f"build/u/assets/obseg/stan/{stan_stem}.bin"
        background_source = source_for(root, "bg", f"bg_{bg_alias}_all_p")
        setup_source = source_for(root, "setup", setup_stem)
        stan_source = source_for(root, "stan", stan_stem)
        setup_text = setup_source.read_text()
        pads = parse_pads(setup_text)
        source_spawns, intro_items = parse_intro_source(setup_text)
        setup_data = setup_bin.read_bytes()
        validate_compiled_spawn(setup_data, source_spawns, pads)
        normal = next(item for item in source_spawns if item["demo_slot"] == 0)
        pad = pads[normal["pad_index"]]
        stan_data = stan_source.read_bytes()
        # Tile-name comments such as ``/*p609b*/`` are human annotations and
        # may contain digits that the shared numeric parser would otherwise
        # mistake for header fields.
        stan_parse_data = re.sub(rb"/\*.*?\*/", b"", stan_data,
                                 flags=re.S)
        tiles = stan_parser.parse_tiles(stan_parse_data, expected_sha256=None,
                                        stage_label=stage_label(symbol))
        packed_spawn_stan = stan_pack_id(str(pad["stan_name"]))
        spawn_tiles = [tile for tile in tiles if tile["id"] == packed_spawn_stan]
        if len(spawn_tiles) != 1:
            raise ValueError(f"{symbol} spawn STAN resolves to {len(spawn_tiles)} tiles")
        dependencies = parse_setup_dependencies(setup_text)
        dependencies["normal_intro_item_ids"] = intro_items
        dependencies["normal_intro_item_count"] = len(intro_items)
        background_data = background_bin.read_bytes()
        stages.append({
            "solo_sequence_index": sequence,
            "stage": stage_label(symbol),
            "runtime_key": re.sub(r"[^a-z0-9]+", "", stage_label(symbol).lower()),
            "level_id": {"symbol": symbol, "value": level_id},
            "decomp_keys": {"background": bg_alias,
                            "setup": setup_stem,
                            "stan": stan_stem},
            "assets": {
                "background": {"compiled": file_identity(background_bin, root),
                               "source": file_identity(background_source, root)},
                "setup": {"compiled": file_identity(setup_bin, root),
                          "source": file_identity(setup_source, root)},
                "stan": {"compiled": file_identity(stan_bin, root),
                         "source": file_identity(stan_source, root)},
            },
            "world": analyze_background(background_data),
            "collision": {"stan_tile_count": len(tiles),
                          "stan_point_count": sum(len(tile["points"]) for tile in tiles)},
            "setup": {
                "pad_count": len(pads),
                "normal_intro_spawn": {
                    **normal,
                    "position": pad["position"],
                    "up": pad["up"],
                    "look": pad["look"],
                    "stan_name": pad["stan_name"],
                    "stan_packed_id": packed_spawn_stan,
                    "stan_tile_index": spawn_tiles[0]["index"],
                    "stan_blob_tile_index": tiles.index(spawn_tiles[0]),
                    "room": spawn_tiles[0]["room"],
                },
                "dependencies": dependencies,
            },
            "pipeline": {
                "order": sequence - 1,
                "status": "authored-assets-inventoried",
                "next_adapter_boundary": "generic stage bundle extraction and runtime registration",
            },
        })
    return {
        "schema": 1,
        "region": "u",
        "scope": "solo missions after Dam and Facility",
        "stage_count": len(stages),
        "existing_stage_predecessors": ["Dam", "Facility"],
        "evidence": [relative(path, root) for path in
                     (constants_path, boss_path, chraidata_path, bg_table_path)],
        "count_semantics": {
            "rooms": "original background room slots, with room 0 called out as dummy",
            "textures": "unique 12-bit Rare texture ids referenced by room display lists",
            "models": "direct ObjectRecord.obj ids plus guard body/head ids; transitive model internals are not claimed",
            "spawn": "the unique intro spawn whose original demo slot is zero",
        },
        "stages": stages,
    }


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument("--output", type=Path,
                        default=root / "docs/generated/solo_stage_asset_inventory.json")
    args = parser.parse_args()
    try:
        manifest = generate(args.root.resolve())
        encoded = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(encoded)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"wrote {manifest['stage_count']} authored solo stage inventories -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
