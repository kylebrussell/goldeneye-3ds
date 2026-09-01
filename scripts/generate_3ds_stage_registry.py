#!/usr/bin/env python3
"""Generate the native solo-stage descriptor rows from checked asset bundles."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes) -> str:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return f"0x{value:016x}"


def enum_symbol(level_symbol: str) -> str:
    suffix = level_symbol.removeprefix("LEVELID_")
    if not re.fullmatch(r"[A-Z][A-Z0-9_]*", suffix):
        raise ValueError(f"invalid LEVELID symbol: {level_symbol}")
    return "GE_STAGE_" + suffix


def check_file(bundle: Path, record: dict[str, object]) -> None:
    path = bundle / str(record["path"])
    data = path.read_bytes()
    if (len(data) != record["size"] or sha256(data) != record["sha256"]
            or fnv1a64(data) != record["fnv1a64"]):
        raise ValueError(f"bundle identity mismatch: {path}")


def checked_rows(inventory_path: Path, bundles: Path) -> list[dict[str, object]]:
    inventory_data = inventory_path.read_bytes()
    inventory = json.loads(inventory_data)
    inventory_hash = sha256(inventory_data)
    stages = inventory["stages"]
    if len(stages) != 18:
        raise ValueError(f"expected 18 post-Facility solo stages, found {len(stages)}")
    rows: list[dict[str, object]] = []
    for stage in stages:
        key = stage["runtime_key"]
        bundle = bundles / key
        manifest = json.loads((bundle / "manifest.json").read_text())
        if (manifest["runtime_key"] != key
                or manifest["source_inventory_sha256"] != inventory_hash
                or manifest["level_id"] != stage["level_id"]
                or manifest["decomp_keys"] != stage["decomp_keys"]
                or manifest["room_count"]
                    != stage["world"]["room_count_including_dummy_room_0"]
                or manifest["portal_count"] != stage["world"]["portal_count"]
                or manifest["stan_tile_count"]
                    != stage["collision"]["stan_tile_count"]
                or manifest["stan_point_count"]
                    != stage["collision"]["stan_point_count"]):
            raise ValueError(f"bundle manifest disagrees with inventory: {key}")
        spawn = stage["setup"]["normal_intro_spawn"]
        if (manifest["spawn"]["blob_tile_index"] != spawn["stan_blob_tile_index"]
                or manifest["spawn"]["room"] != spawn["room"]):
            raise ValueError(f"bundle spawn disagrees with inventory: {key}")
        for record in manifest["files"].values():
            check_file(bundle, record)
        if (manifest["files"]["background"]["sha256"]
                != stage["assets"]["background"]["compiled"]["sha256"]
                or manifest["files"]["setup"]["sha256"]
                != stage["assets"]["setup"]["compiled"]["sha256"]):
            raise ValueError(f"compiled authored asset mismatch: {key}")
        rows.append({"stage": stage, "manifest": manifest})

    by_key = {row["stage"]["runtime_key"]: row for row in rows}
    surface1 = by_key["surface1"]
    surface2 = by_key["surface2"]
    if (surface1["stage"]["decomp_keys"]["background"]
            != surface2["stage"]["decomp_keys"]["background"]
            or surface1["stage"]["decomp_keys"]["stan"]
            != surface2["stage"]["decomp_keys"]["stan"]
            or surface1["manifest"]["files"]["background"]["sha256"]
            != surface2["manifest"]["files"]["background"]["sha256"]
            or surface1["manifest"]["files"]["collision"]["sha256"]
            != surface2["manifest"]["files"]["collision"]["sha256"]
            or surface1["stage"]["decomp_keys"]["setup"]
            == surface2["stage"]["decomp_keys"]["setup"]
            or surface1["manifest"]["files"]["setup"]["sha256"]
            == surface2["manifest"]["files"]["setup"]["sha256"]):
        raise ValueError("Surface 1/2 authored shared-world/setup distinction was lost")
    return rows


def parse_level_info(path: Path) -> dict[str, dict[str, object]]:
    text = path.read_text()
    body = text[text.index("struct levelentry levelinfotable[] = {"):]
    body = body[:body.index("};")]
    pattern = re.compile(
        r'\{\s*(LEVELID_[A-Z0-9_]+)\s*,\s*'
        r'"bg/bg_([a-z0-9]+)_all_p\.seg"\s*,\s*'
        r'"[^"]+"\s*,\s*'
        r'([0-9.]+)\s*,\s*([0-9.]+)\s*,\s*([0-9.]+)\s*\}')
    result: dict[str, dict[str, object]] = {}
    for index, match in enumerate(pattern.finditer(body)):
        symbol, background, scale, visibility, distance = match.groups()
        result[symbol] = {
            "index": index,
            "background": background,
            "scale": scale,
            "visibility": visibility,
            "distance": distance,
        }
    if len(result) < 20:
        raise ValueError("canonical levelinfotable parse is incomplete")
    return result


def render(rows: list[dict[str, object]],
           level_info: dict[str, dict[str, object]] | None = None) -> str:
    if level_info is None:
        level_info = parse_level_info(
            Path(__file__).resolve().parents[1] / "src/game/bg.c")
    lines = [
        "/* Generated by scripts/generate_3ds_stage_registry.py; do not edit. */",
        "/* symbol, LEVELID/index/scale/visibility/distance, key, background key, setup key, STAN key,",
        " * background size/FNV, collision size/FNV, bounds size/FNV,",
        " * setup size/FNV, room/portal/tile/point counts, spawn tile/room */",
    ]
    for row in rows:
        stage = row["stage"]
        manifest = row["manifest"]
        files = manifest["files"]
        level = stage["level_id"]
        keys = stage["decomp_keys"]
        spawn = manifest["spawn"]
        info = level_info[level["symbol"]]
        if info["background"] != keys["background"]:
            raise ValueError(
                f"levelinfotable background mismatch: {stage['runtime_key']}")
        lines.extend([
            "GE_SOLO_STAGE(",
            f"    {enum_symbol(level['symbol'])}, {level['value']}, {info['index']},",
            f"    {info['scale']}f, {info['visibility']}f, {info['distance']}f,",
            f"    \"{stage['runtime_key']}\", \"{keys['background']}\",",
            f"    \"{keys['setup']}\", \"{keys['stan']}\",",
            f"    {files['background']['size']}U, UINT64_C({files['background']['fnv1a64']}),",
            f"    {files['collision']['size']}U, UINT64_C({files['collision']['fnv1a64']}),",
            f"    {files['room_bounds']['size']}U, UINT64_C({files['room_bounds']['fnv1a64']}),",
            f"    {files['setup']['size']}U, UINT64_C({files['setup']['fnv1a64']}),",
            f"    {manifest['room_count']}U, {manifest['portal_count']}U,",
            f"    {manifest['stan_tile_count']}U, {manifest['stan_point_count']}U,",
            f"    {spawn['blob_tile_index']}U, {spawn['room']}U)",
        ])
    return "\n".join(lines) + "\n"


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory", type=Path,
                        default=root / "docs/generated/solo_stage_asset_inventory.json")
    parser.add_argument("--bundles", type=Path,
                        default=root / "build/3ds-levels")
    parser.add_argument("--output", type=Path,
                        default=root / "port/include/ge_solo_stage_registry.inc")
    parser.add_argument("--check", action="store_true",
                        help="fail rather than rewrite when output is stale")
    args = parser.parse_args()
    try:
        output = render(
            checked_rows(args.inventory, args.bundles),
            parse_level_info(root / "src/game/bg.c"))
        if args.check:
            if args.output.read_text() != output:
                raise ValueError(f"generated registry is stale: {args.output}")
        else:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(output)
    except (KeyError, OSError, TypeError, ValueError) as error:
        parser.error(str(error))
    print(f"validated 18 authored solo stage descriptors -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
