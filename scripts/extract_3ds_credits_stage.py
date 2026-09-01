#!/usr/bin/env python3
"""Build the authored Cuba credits-stage bundle for the native runtime."""

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


BACKGROUND_SIZE = 4000
BACKGROUND_SHA256 = "2b630644f24fc30d00f03a772c7908a61ebdc470eda99859991d9766cec7a887"
STAN_SHA256 = "b68c2a09d2e24368fdea938612e5c0be6cb64f58969839e8f380f53a44d38d29"
SETUP_SOURCE_SHA256 = "cb7bc8f08264b6b20ccc804af3e73b31813c6174091c3a2a2238402bb71c93be"
SETUP_BINARY_SHA256 = "25eea9f317c88c5d6d6093ef2ad83a9d3c02275121d9c88e6e5352312f624b50"
ROOM_COUNT = 2
PORTAL_COUNT = 0


def load_script(root: Path, filename: str):
    path = root / "scripts" / filename
    spec = importlib.util.spec_from_file_location("ge_credits_" + path.stem, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load extraction module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def identity(path: Path, relative: str) -> dict[str, object]:
    data = path.read_bytes()
    return {
        "path": relative,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }


def checked(path: Path, digest: str) -> Path:
    if sha256(path) != digest:
        raise ValueError(f"authored asset identity mismatch: {path}")
    return path


def replace_directory(staging: Path, output: Path) -> None:
    old = output.with_name(output.name + ".old")
    if old.exists():
        shutil.rmtree(old)
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
        shutil.rmtree(old)


def extract(root: Path, output: Path) -> dict[str, object]:
    background = checked(
        root / "build/u/assets/obseg/bg/bg_len_all_p.bin",
        BACKGROUND_SHA256)
    stan = checked(root / "assets/obseg/stan/Tbg_len_all_p_stanZ.c",
                   STAN_SHA256)
    setup_source = checked(root / "assets/obseg/setup/u/UsetuplenZ.c",
                           SETUP_SOURCE_SHA256)
    setup_binary = checked(root / "build/u/assets/obseg/setup/UsetuplenZ.bin",
                           SETUP_BINARY_SHA256)
    rooms_module = load_script(root, "extract_3ds_dam_rooms.py")
    collision_module = load_script(root, "extract_3ds_stage_collision.py")
    bounds_module = load_script(root, "build_3ds_dam_room_bounds.py")

    output = output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-cuba-stage-",
                                     dir=output.parent) as temporary:
        staging = Path(temporary) / "cuba"
        staging.mkdir()
        shutil.copyfile(background, staging / "background.bin")
        rooms = rooms_module.extract_rooms(
            background, staging / "rooms", expected_size=BACKGROUND_SIZE,
            expected_sha256=BACKGROUND_SHA256, stage_key="cuba",
            source_manifest_path=
                "build/u/assets/obseg/bg/bg_len_all_p.bin")
        collision = collision_module.extract(
            stan, setup_source, staging / "collision", stage_key="cuba",
            stage_label="Cuba", expected_stan_sha256=STAN_SHA256,
            expected_setup_sha256=SETUP_SOURCE_SHA256,
            setup_bin_path=setup_binary,
            expected_setup_bin_sha256=SETUP_BINARY_SHA256)
        bounds_module.build_asset(
            background, staging / "rooms", staging / "room_bounds.gebounds",
            expected_source_size=BACKGROUND_SIZE,
            expected_source_sha256=BACKGROUND_SHA256,
            expected_room_count=ROOM_COUNT,
            expected_portal_count=PORTAL_COUNT, stage_label="Cuba")
        if (rooms["room_count"] != ROOM_COUNT
                or collision["tile_count"] != 206
                or collision["point_count"] != 618
                or collision["spawn"]["setup_pad"] != 44
                or collision["spawn"]["blob_tile_index"] != 89
                or collision["spawn"]["room"] != 1):
            raise ValueError("Cuba authored topology/spawn contract changed")
        manifest = {
            "schema": 1,
            "stage": "Cuba Credits",
            "runtime_key": "cuba",
            "level_id": {"symbol": "LEVELID_CUBA", "value": 54},
            "decomp_keys": {
                "background": "len",
                "setup": "UsetuplenZ",
                "stan": "Tbg_len_all_p_stanZ",
            },
            "room_count": ROOM_COUNT,
            "portal_count": PORTAL_COUNT,
            "stan_tile_count": collision["tile_count"],
            "stan_point_count": collision["point_count"],
            "spawn": collision["spawn"],
            "files": {
                "background": identity(staging / "background.bin",
                                       "background.bin"),
                "collision": identity(
                    staging / "collision/collision.gestan",
                    "collision/collision.gestan"),
                "room_bounds": identity(staging / "room_bounds.gebounds",
                                        "room_bounds.gebounds"),
                "setup": identity(staging / "collision/setup.bin",
                                  "collision/setup.bin"),
            },
        }
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        replace_directory(staging, output)
    return manifest


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument("--output", type=Path,
                        default=root / "build/3ds-levels/cuba")
    args = parser.parse_args()
    try:
        manifest = extract(args.root.resolve(), args.output)
    except (OSError, RuntimeError, ValueError) as error:
        parser.error(str(error))
    print("built authored Cuba credits bundle: "
          f"{manifest['room_count']} rooms, "
          f"{manifest['stan_tile_count']} STAN tiles -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
