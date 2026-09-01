#!/usr/bin/env python3
"""Build generic authored bundles for solo missions after Dam and Facility."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import zlib


def load_script(root: Path, filename: str):
    path = root / "scripts" / filename
    name = "ge_solo_stage_" + path.stem
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load extraction module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fnv1a64(data: bytes) -> int:
    value = 0xCBF29CE484222325
    for byte in data:
        value ^= byte
        value = value * 0x100000001B3 & 0xFFFFFFFFFFFFFFFF
    return value


def identity(path: Path) -> dict[str, object]:
    data = path.read_bytes()
    return {"path": path.name, "size": len(data), "sha256": sha256(data),
            "fnv1a64": f"0x{fnv1a64(data):016x}"}


def remove(path: Path) -> None:
    if path.is_symlink() or path.is_file():
        path.unlink()
    elif path.exists():
        shutil.rmtree(path)


def replace_directory(staging: Path, output: Path) -> None:
    if output.is_symlink():
        raise ValueError(f"refusing to replace symlink output: {output}")
    old = output.with_name(output.name + ".old")
    remove(old)
    moved = False
    if output.exists():
        os.replace(output, old)
        moved = True
    try:
        os.replace(staging, output)
    except BaseException:
        if moved and old.exists() and not output.exists():
            os.replace(old, output)
        raise
    if moved:
        remove(old)


def checked_path(root: Path, record: dict[str, object]) -> Path:
    path = root / str(record["path"])
    data = path.read_bytes()
    if len(data) != record["size"] or sha256(data) != record["sha256"]:
        raise ValueError(f"inventory identity mismatch: {path}")
    return path


def extract_stage(root: Path, stage: dict[str, object], output: Path,
                  rooms_module, collision_module, bounds_module,
                  inventory_sha256: str) -> dict[str, object]:
    key = str(stage["runtime_key"])
    label = str(stage["stage"])
    background_record = stage["assets"]["background"]["compiled"]
    background_source_record = stage["assets"]["background"]["source"]
    setup_record = stage["assets"]["setup"]["compiled"]
    setup_source_record = stage["assets"]["setup"]["source"]
    stan_record = stage["assets"]["stan"]["compiled"]
    stan_source_record = stage["assets"]["stan"]["source"]
    background = checked_path(root, background_record)
    checked_path(root, background_source_record)
    setup = checked_path(root, setup_record)
    setup_source = checked_path(root, setup_source_record)
    checked_path(root, stan_record)
    stan_source = checked_path(root, stan_source_record)

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=f"ge-{key}-stage-",
                                     dir=output.parent) as temporary:
        staging = Path(temporary) / key
        staging.mkdir()
        shutil.copyfile(background, staging / "background.bin")
        rooms = rooms_module.extract_rooms(
            background, staging / "rooms", expected_size=background_record["size"],
            expected_sha256=background_record["sha256"], stage_key=key,
            source_manifest_path=background_record["path"],
        )
        collision = collision_module.extract(
            stan_source, setup_source, staging / "collision", stage_key=key,
            stage_label=label, expected_stan_sha256=stan_source_record["sha256"],
            expected_setup_sha256=setup_source_record["sha256"],
            setup_bin_path=setup,
            expected_setup_bin_sha256=setup_record["sha256"],
            stan_manifest_path=stan_source_record["path"],
            setup_manifest_path=setup_source_record["path"],
        )
        bounds_path = staging / "room_bounds.gebounds"
        bounds_module.build_asset(
            background, staging / "rooms", bounds_path,
            expected_source_size=background_record["size"],
            expected_source_sha256=background_record["sha256"],
            expected_room_count=stage["world"]["room_count_including_dummy_room_0"],
            expected_portal_count=stage["world"]["portal_count"],
            stage_label=label,
        )
        spawn = stage["setup"]["normal_intro_spawn"]
        if (collision["tile_count"] != stage["collision"]["stan_tile_count"]
                or collision["point_count"] != stage["collision"]["stan_point_count"]
                or collision["spawn"]["blob_tile_index"]
                    != spawn["stan_blob_tile_index"]
                or collision["spawn"]["room"] != spawn["room"]
                or rooms["room_count"]
                    != stage["world"]["room_count_including_dummy_room_0"]):
            raise ValueError(f"{label} generated bundle disagrees with inventory")
        files = {
            "background": identity(staging / "background.bin"),
            "collision": identity(staging / "collision/collision.gestan"),
            "room_bounds": identity(bounds_path),
            "setup": identity(staging / "collision/setup.bin"),
            "rooms_manifest": identity(staging / "rooms/manifest.json"),
        }
        files["background"]["path"] = "background.bin"
        files["collision"]["path"] = "collision/collision.gestan"
        files["room_bounds"]["path"] = "room_bounds.gebounds"
        files["setup"]["path"] = "collision/setup.bin"
        files["rooms_manifest"]["path"] = "rooms/manifest.json"
        manifest = {
            "schema": 1,
            "stage": label,
            "runtime_key": key,
            "solo_sequence_index": stage["solo_sequence_index"],
            "level_id": stage["level_id"],
            "decomp_keys": stage["decomp_keys"],
            "source_inventory_sha256": inventory_sha256,
            "files": files,
            "room_count": rooms["room_count"],
            "portal_count": stage["world"]["portal_count"],
            "stan_tile_count": collision["tile_count"],
            "stan_point_count": collision["point_count"],
            "spawn": collision["spawn"],
        }
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        replace_directory(staging, output)
    return manifest


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument("--inventory", type=Path,
                        default=root / "docs/generated/solo_stage_asset_inventory.json")
    parser.add_argument("--output", type=Path,
                        default=root / "build/3ds-levels")
    parser.add_argument("--stage", action="append", default=[],
                        help="runtime key to extract; repeatable, defaults to all 18")
    args = parser.parse_args()
    try:
        root = args.root.resolve()
        inventory_data = args.inventory.read_bytes()
        inventory = json.loads(inventory_data)
        selected = set(args.stage)
        known = {stage["runtime_key"] for stage in inventory["stages"]}
        if selected - known:
            raise ValueError(f"unknown stage keys: {sorted(selected - known)}")
        stages = [stage for stage in inventory["stages"]
                  if not selected or stage["runtime_key"] in selected]
        rooms_module = load_script(root, "extract_3ds_dam_rooms.py")
        collision_module = load_script(root, "extract_3ds_stage_collision.py")
        bounds_module = load_script(root, "build_3ds_dam_room_bounds.py")
        manifests = [
            extract_stage(root, stage, args.output / stage["runtime_key"],
                          rooms_module, collision_module, bounds_module,
                          sha256(inventory_data))
            for stage in stages
        ]
    except (KeyError, OSError, UnicodeError, ValueError, zlib.error) as error:
        parser.error(str(error))
    print(f"built {len(manifests)} authored solo stage bundles -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
