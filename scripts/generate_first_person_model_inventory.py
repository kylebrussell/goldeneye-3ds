#!/usr/bin/env python3
"""Inventory canonical solo-stage loadouts and their first-person models.

The stage order, setup mapping, intro commands, ITEM_IDS order, gun-model
records, and held-prop relations are all read from decompiled source.  This is
an evidence manifest for extending the ROM model pipeline; it does not invent
loadouts or substitute models.
"""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import re

from generate_3ds_stage_inventory import (
    initializer,
    parse_level_ids,
    parse_setup_table,
    parse_solo_order,
    source_for,
    stage_label,
)


START_WEAPON_PATTERN = re.compile(
    r'/\* Type = StartWeapon; index = (\d+) \*/\s*\n'
    r'\s*_mkword\([^\n]+\),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+),')
COLLECTABLE_PATTERN = re.compile(
    r'/\* Type = Collectable; index = \d+ \*/\s*\n'
    r'\s*_mkword\(256,\s*_mkshort\(0,\s*8\)\),\s*'
    r'_mkword\(\d+,\s*(-?\d+)\)')


def parse_item_ids(constants: str) -> list[str]:
    body = initializer(constants, "typedef enum ITEM_IDS")
    names = re.findall(r"\b(ITEM_[A-Z0-9_]+)\b", body)
    result = [name for name in names
              if name not in {"ITEM_NOTHING", "ITEM_IDS_MAX"}]
    if not result or result[0] != "ITEM_UNARMED" or result[-1] != "ITEM_TOKEN":
        raise ValueError("unexpected ITEM_IDS order")
    if len(result) != len(set(result)):
        raise ValueError("duplicate ITEM_IDS entries")
    return result


def parse_gun_relation(root: Path, item: str) -> dict[str, object]:
    if item == "ITEM_SUIT_LF_HAND":
        record_path = root / "assets/obseg/gun/gunModelFileRecord.inc.c"
        text = record_path.read_text()
        if not re.search(
                r"SUIT_LFRECORD\(suit_lf_hand,\s*0,\s*"
                r"GUNSTATS\(Csuit_lf_handz\)", text):
            raise ValueError("canonical suit left-hand record changed")
        header_path = (root / "assets/obseg/chr/suit_lf_hand"
                       / "modelFileHeader.inc.c")
        if not header_path.is_file():
            raise ValueError("canonical suit left-hand header is missing")
        return {
            "has_first_person_model": True,
            "resource": "Csuit_lf_handZ",
            "record_source": record_path.relative_to(root).as_posix(),
            "header_source": header_path.relative_to(root).as_posix(),
        }
    directory = item.removeprefix("ITEM_").lower()
    record_path = root / "assets/obseg/gun" / directory / "gunFileRecord.inc.c"
    if not record_path.is_file():
        return {"has_first_person_model": False, "resource": None,
                "record_source": None}
    text = record_path.read_text()
    match = re.search(
        r"GUNFILERECORD\(\s*([A-Za-z0-9_]+)\s*,\s*(0|1|FALSE|TRUE)\s*,",
        text)
    if match is None:
        raise ValueError(f"cannot parse gun record {record_path}")
    name, no_model = match.groups()
    relation: dict[str, object] = {
        "has_first_person_model": no_model in {"0", "FALSE"},
        "resource": f"G{name}Z" if no_model in {"0", "FALSE"} else None,
        "record_source": record_path.relative_to(root).as_posix(),
    }
    header_path = record_path.with_name("ModelFileHeader.inc.c")
    if no_model in {"0", "FALSE"}:
        if not header_path.is_file():
            raise ValueError(f"modeled item lacks header {header_path}")
        relation["header_source"] = header_path.relative_to(root).as_posix()
    return relation


def parse_held_prop_relations(player_source: str) -> dict[str, str]:
    body = initializer(player_source, "PROP getPropForHeldItem")
    return dict(re.findall(
        r"case\s+(ITEM_[A-Z0-9_]+)\s*:\s*ret\s*=\s*(PROP_[A-Z0-9_]+)\s*;",
        body))


def parse_filelist(root: Path) -> dict[str, dict[str, int]]:
    result: dict[str, dict[str, int]] = {}
    for line in (root / "scripts/filelist.u.csv").read_text().splitlines():
        fields = line.split(",")
        if len(fields) < 3:
            continue
        match = re.fullmatch(
            r"assets/obseg/(?:gun|chr)/([GC][A-Za-z0-9_]+Z)\.bin",
            fields[2])
        if match is not None:
            result[match.group(1)] = {
                "rom_start": int(fields[0]), "compressed_size": int(fields[1])}
    return result


def generate(root: Path) -> dict[str, object]:
    constants_path = root / "src/bondconstants.h"
    boss_path = root / "src/boss.c"
    chraidata_path = root / "src/game/chraidata.c"
    player_path = root / "src/game/player.c"
    constants = constants_path.read_text()
    items = parse_item_ids(constants)
    item_index = {name: index for index, name in enumerate(items)}
    level_ids = parse_level_ids(constants)
    solo_order = parse_solo_order(boss_path.read_text())
    setup_table = parse_setup_table(chraidata_path.read_text())
    held_props = parse_held_prop_relations(player_path.read_text())
    filelist = parse_filelist(root)
    extractor_path = root / "scripts/extract_3ds_first_person_pp7.py"
    provider_path = root / "port/src/ge_original_first_person_item_model.c"

    canonical_items = []
    for index, item in enumerate(items):
        relation = parse_gun_relation(root, item)
        resource = relation["resource"]
        if resource in filelist:
            relation["rom"] = filelist[resource]
        relation["held_prop"] = held_props.get(item)
        canonical_items.append({"item": item, "item_id": index, **relation})

    stages = []
    frequencies: Counter[str] = Counter()
    collectable_frequencies: Counter[str] = Counter()
    collectable_stages: dict[str, set[str]] = {}
    for sequence, symbol in enumerate(solo_order):
        level_id = level_ids[symbol]
        setup_stem = setup_table[level_id]
        if setup_stem is None:
            raise ValueError(f"{symbol} has no setup")
        source = source_for(root, "setup", setup_stem)
        setup_text = source.read_text()
        intro = initializer(setup_text, "s32 intro[] =")
        records = []
        for record_index, right_id, left_id, demo_slot in START_WEAPON_PATTERN.findall(intro):
            right_value, left_value = int(right_id), int(left_id)
            if not 0 <= right_value < len(items):
                raise ValueError(f"{symbol} invalid right-hand item {right_value}")
            right = items[right_value]
            left = None
            if left_value >= 0:
                if left_value >= len(items):
                    raise ValueError(f"{symbol} invalid left-hand item {left_value}")
                left = items[left_value]
            frequencies[right] += 1
            if left is not None:
                frequencies[left] += 1
            records.append({"record_index": int(record_index),
                            "demo_slot": int(demo_slot),
                            "right_item": right, "right_item_id": right_value,
                            "left_item": left, "left_item_id": left_value})
        label = stage_label(symbol)
        for item_value in COLLECTABLE_PATTERN.findall(setup_text):
            item_id = int(item_value)
            if 0 <= item_id < len(items):
                item = items[item_id]
                collectable_frequencies[item] += 1
                collectable_stages.setdefault(item, set()).add(label)
        stages.append({"solo_sequence_index": sequence,
                       "stage": label,
                       "level_id": symbol,
                       "setup_source": source.relative_to(root).as_posix(),
                       "authored_start_weapon_records": records})

    frequency_rows = []
    for item, count in sorted(frequencies.items(),
                              key=lambda pair: (-pair[1], item_index[pair[0]])):
        relation = canonical_items[item_index[item]]
        frequency_rows.append({"item": item, "item_id": item_index[item],
                               "authored_intro_occurrences": count,
                               "first_person_resource": relation["resource"],
                               "held_prop": relation["held_prop"]})
    collectable_rows = []
    for item, count in sorted(
            collectable_frequencies.items(),
            key=lambda pair: (-pair[1], item_index[pair[0]])):
        relation = canonical_items[item_index[item]]
        collectable_rows.append({
            "item": item,
            "item_id": item_index[item],
            "authored_setup_occurrences": count,
            "solo_stage_count": len(collectable_stages[item]),
            "solo_stages": sorted(collectable_stages[item]),
            "first_person_resource": relation["resource"],
            "has_first_person_model": relation["has_first_person_model"],
        })
    modeled_relations = {
        row["item"]: row["resource"] for row in canonical_items
        if row["has_first_person_model"]
    }
    packaged_relations = {
        item: resource for resource, item in re.findall(
            r'Resource\("([GC][A-Za-z0-9_]+Z)",\s*"(ITEM_[A-Z0-9_]+)"',
            extractor_path.read_text())
    }
    provider_relations = dict(re.findall(
        r'\{(ITEM_[A-Z0-9_]+),\s*GE_ORIGINAL_FIRST_PERSON_MODEL_[A-Z0-9_]+,\s*'
        r'"([GC][A-Za-z0-9_]+Z)"\}', provider_path.read_text()))
    if set(packaged_relations) != set(modeled_relations):
        raise ValueError("ROM first-person package does not cover every modeled gitem")
    if provider_relations != modeled_relations:
        raise ValueError("runtime item/model provider does not match canonical gitem resources")
    return {
        "schema": 1,
        "region": "u",
        "scope": "all 20 solo stages, all authored intro demo slots, and canonical inventory order",
        "stage_count": len(stages),
        "evidence": [path.relative_to(root).as_posix() for path in
                     (constants_path, boss_path, chraidata_path, player_path,
                      root / "assets/obseg/gun/gunModelFileRecord.inc.c",
                      root / "assets/obseg/prop/propItemModelFileRecord.inc.c",
                      root / "scripts/filelist.u.csv", extractor_path,
                      provider_path)],
        "semantics": {
            "canonical_inventory_order": "ITEM_IDS numeric order used by bondinvSortInv",
            "authored_intro_occurrences": "right and explicit left items in every StartWeapon command, including demo slots",
            "held_prop": "exact getPropForHeldItem relation into PitemZ_entries; null means the canonical function publishes no held prop",
            "authored_setup_collectable_frequency": "valid ITEM_IDS in PROPDEF_COLLECTABLE records across the 20 solo setup streams",
            "runtime_cache_coverage": "exact equality between modeled canonical gitem entries, ROM package entries, and generic item/model provider relations",
        },
        "runtime_cache_coverage": {
            "canonical_modeled_items": len(modeled_relations),
            "packaged_rom_resources": len(packaged_relations),
            "provider_relations": len(provider_relations),
            "complete": True,
        },
        "authored_intro_frequency": frequency_rows,
        "authored_setup_collectable_frequency": collectable_rows,
        "canonical_inventory_order": canonical_items,
        "stages": stages,
    }


def main() -> int:
    default_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=default_root)
    parser.add_argument("--output", type=Path,
                        default=default_root / "docs/generated/first_person_model_inventory.json")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        manifest = generate(args.root.resolve())
        encoded = json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        if args.check:
            if not args.output.is_file() or args.output.read_text() != encoded:
                raise ValueError(f"stale inventory: {args.output}")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(encoded)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"validated {manifest['stage_count']} solo-stage first-person inventories")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
