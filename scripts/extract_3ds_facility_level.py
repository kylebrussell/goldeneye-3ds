#!/usr/bin/env python3
"""Build an exact-data Facility level bundle without activating its runtime."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile


BACKGROUND_SIZE = 200576
BACKGROUND_SHA256 = "1a70a9b0ab2c8747703b56bcf8cb160ab081bec52f801984fba61093e6d757a6"
STAN_SHA256 = "94f167e2e2a9e4deda6f55ba94cb0a35c7fe979377128e31d14aa1c552b0cd9b"
SETUP_SHA256 = "bfa356610c1d44f9e84b0f6a3aa6c3fa490c35d832f486c564c49ae4c5c70a9e"
SETUP_BIN_SHA256 = "efdeacf9bd5eb8c3a7b5eb4a5d16da0e466eca04b1222606e27cbe559fac4429"
ROOM_COUNT = 78
PORTAL_COUNT = 109
VISIBILITY_RECORD_COUNT = 101
BG_BASE = 0x0F000000


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def load_script(name: str):
    path = Path(__file__).with_name(name)
    module_name = "ge_facility_" + path.stem
    spec = importlib.util.spec_from_file_location(module_name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load extraction module: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def replace_directory(staging: Path, output: Path) -> None:
    def remove(path: Path) -> None:
        if path.is_symlink() or path.is_file():
            path.unlink()
        elif path.exists():
            shutil.rmtree(path)

    if output.is_symlink():
        raise ValueError(f"refusing to replace symlink output: {output}")
    old = output.with_name(output.name + ".old")
    remove(old)
    moved_old = False
    if output.exists():
        os.replace(output, old)
        moved_old = True
    try:
        os.replace(staging, output)
    except BaseException:
        if moved_old and old.exists() and not output.exists():
            os.replace(old, output)
        raise
    if moved_old:
        remove(old)


def background_visibility_summary(background: bytes) -> dict[str, object]:
    if len(background) != BACKGROUND_SIZE or sha256(background) != BACKGROUND_SHA256:
        raise ValueError("Facility background identity mismatch")
    _root, _rooms, portals_address, visibility_address, _environment = \
        struct.unpack_from(">5I", background)
    portal_offset = portals_address - BG_BASE
    visibility_offset = visibility_address - BG_BASE
    if not 0 <= visibility_offset <= portal_offset <= len(background):
        raise ValueError("Facility visibility table bounds are invalid")
    visibility = background[visibility_offset:portal_offset]
    if len(visibility) % 8 or len(visibility) // 8 != VISIBILITY_RECORD_COUNT:
        raise ValueError("Facility visibility record count mismatch")
    return {
        "record_count": len(visibility) // 8,
        "size": len(visibility),
        "source_offset": visibility_offset,
        "sha256": sha256(visibility),
    }


def extract(background_path: Path, stan_path: Path, setup_path: Path,
            setup_bin_path: Path, output: Path) -> dict[str, object]:
    rooms_module = load_script("extract_3ds_dam_rooms.py")
    bounds_module = load_script("build_3ds_dam_room_bounds.py")
    collision_module = load_script("extract_3ds_stage_collision.py")

    background = background_path.read_bytes()
    visibility = background_visibility_summary(background)
    output = output.expanduser().absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-facility-level-",
                                     dir=output.parent) as temporary_name:
        staging = Path(temporary_name) / "facility"
        staging.mkdir()
        (staging / "background.bin").write_bytes(background)
        rooms = rooms_module.extract_rooms(
            background_path, staging / "rooms", expected_size=BACKGROUND_SIZE,
            expected_sha256=BACKGROUND_SHA256, stage_key="facility",
            source_manifest_path="build/u/assets/obseg/bg/bg_ark_all_p.bin",
        )
        collision = collision_module.extract(
            stan_path, setup_path, staging / "collision", stage_key="facility",
            stage_label="Facility", expected_stan_sha256=STAN_SHA256,
            expected_setup_sha256=SETUP_SHA256,
            setup_bin_path=setup_bin_path,
            expected_setup_bin_sha256=SETUP_BIN_SHA256,
            stan_manifest_path="assets/obseg/stan/Tbg_ark_all_p_stanZ.c",
            setup_manifest_path="assets/obseg/setup/UsetuparkZ.c",
        )
        bounds_path = staging / "room_bounds.gebounds"
        bounds = bounds_module.build_asset(
            background_path, staging / "rooms", bounds_path,
            expected_source_size=BACKGROUND_SIZE,
            expected_source_sha256=BACKGROUND_SHA256,
            expected_room_count=ROOM_COUNT, expected_portal_count=PORTAL_COUNT,
            stage_label="Facility",
        )
        manifest = {
            "schema": 1,
            "stage": "Facility",
            "decomp_stage_key": "ark",
            "background": {
                "path": "background.bin", "size": len(background),
                "sha256": sha256(background), "room_count": rooms["room_count"],
                "portal_count": PORTAL_COUNT,
                "visibility": visibility,
            },
            "rooms": {
                "path": "rooms", "selected_count": len(rooms["selected_rooms"]),
                "stream_count": sum(len(room["streams"])
                                    for room in rooms["rooms"]),
                "manifest_sha256": sha256((staging / "rooms/manifest.json").read_bytes()),
            },
            "collision": {
                "path": "collision/collision.gestan",
                "size": collision["blob"]["size"],
                "sha256": collision["blob"]["sha256"],
                "tile_count": collision["tile_count"],
                "point_count": collision["point_count"],
            },
            "setup": {
                "path": "collision/setup.bin",
                "manifest": "collision/setup.json",
                "normal_spawn": collision["spawn"],
            },
            "room_bounds": {
                "path": "room_bounds.gebounds", "size": len(bounds),
                "sha256": sha256(bounds),
            },
        }
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n")
        replace_directory(staging, output)
    return manifest


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--background", type=Path,
                        default=root / "build/u/assets/obseg/bg/bg_ark_all_p.bin")
    parser.add_argument("--stan", type=Path,
                        default=root / "assets/obseg/stan/Tbg_ark_all_p_stanZ.c")
    parser.add_argument("--setup", type=Path,
                        default=root / "assets/obseg/setup/UsetuparkZ.c")
    parser.add_argument("--setup-bin", type=Path,
                        default=root / "build/u/assets/obseg/setup/UsetuparkZ.bin")
    parser.add_argument("--output", type=Path,
                        default=root / "build/3ds-levels/facility")
    args = parser.parse_args()
    try:
        manifest = extract(args.background, args.stan, args.setup,
                           args.setup_bin, args.output)
    except (OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"Facility bundle: {manifest['background']['room_count']} rooms, "
          f"{manifest['background']['portal_count']} portals, "
          f"{manifest['collision']['tile_count']} STAN tiles -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
