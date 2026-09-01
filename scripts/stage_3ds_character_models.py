#!/usr/bin/env python3
"""Stage every ROM-backed character model required by the solo setups."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from typing import Any


MODEL_TABLE = Path("assets/obseg/chr/chrModelFileRecords.inc.c")
SOURCE_BLOBS = Path("build/u/assets/obseg/chr")
STAGE_INVENTORY = Path("docs/generated/solo_stage_asset_inventory.json")
SETUP_PARSER = Path("scripts/generate_3ds_stage_inventory.py")
EXTRA_SETUPS = ("UsetupdamZ", "UsetuparkZ")
RECORD_INCLUDE = re.compile(
    r"#include\s+<assets/obseg/chr/([^/]+)/chrModelFileRecord\.inc\.c>")
RECORD = re.compile(
    r'\{&([A-Za-z0-9_]+)_header,\s*"(C[A-Za-z0-9_]+Z)",\s*'
    r'([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),')
HEADER = re.compile(
    r"MODELFILEHEADER\([^,]+,\s*[^,]+,\s*([^,]+),\s*[^,]+,\s*"
    r"([^,]+),\s*([^,]+),\s*([^,]+),\s*([^,]+),\s*([^\)]+)\)")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    result = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def int_literal(value: str) -> int:
    return int(value.strip(), 0)


def load_setup_parser(root: Path):
    path = root / SETUP_PARSER
    spec = importlib.util.spec_from_file_location("ge_stage_inventory", path)
    if spec is None or spec.loader is None:
        raise ValueError(f"cannot import {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def parse_records(root: Path) -> list[dict[str, Any]]:
    table = root / MODEL_TABLE
    names = RECORD_INCLUDE.findall(table.read_text(encoding="utf-8"))
    if not names:
        raise ValueError("canonical character model table is empty")
    records = []
    for model_id, name in enumerate(names):
        record_path = root / f"assets/obseg/chr/{name}/chrModelFileRecord.inc.c"
        header_path = root / f"assets/obseg/chr/{name}/modelFileHeader.inc.c"
        record_match = RECORD.search(record_path.read_text(encoding="utf-8"))
        header_match = HEADER.search(header_path.read_text(encoding="utf-8"))
        if record_match is None or header_match is None:
            raise ValueError(f"unsupported character metadata syntax for {name}")
        header_name, resource, scale, pov, male, has_head = record_match.groups()
        skeleton, switches, matrices, radius, records_count, textures = (
            value.strip() for value in header_match.groups())
        if header_name != name or resource != f"C{name}Z":
            raise ValueError(f"character metadata name drift for {name}")
        if skeleton not in ("NULL", "&SKELETON(guard)",
                            "&SKELETON(suit_lf_hand)"):
            raise ValueError(f"unsupported skeleton {skeleton} for {name}")
        records.append({
            "model_id": model_id, "name": name, "resource": resource,
            "path": f"{resource}.bin", "scale_source": scale.strip(),
            "scale": float(scale), "pov_source": pov.strip(), "pov": float(pov),
            "is_male": bool(int_literal(male)),
            "has_head": bool(int_literal(has_head)),
            "skeleton": skeleton,
            "num_switches": int_literal(switches),
            "num_matrices": int_literal(matrices),
            "bounding_radius_source": radius,
            "bounding_radius": float(radius),
            "num_records_source": records_count,
            "num_textures": int_literal(textures),
            "record_sha256": sha256_file(record_path),
            "header_sha256": sha256_file(header_path),
        })
    return records


def dependency_ids(root: Path, records: list[dict[str, Any]]) -> tuple[set[int], set[int]]:
    inventory = json.loads((root / STAGE_INVENTORY).read_text(encoding="utf-8"))
    body_ids: set[int] = set()
    explicit_heads: set[int] = set()
    for stage in inventory["stages"]:
        deps = stage["setup"]["dependencies"]
        body_ids.update(deps["unique_guard_body_ids"])
        explicit_heads.update(deps["unique_explicit_guard_head_ids"])
    parser = load_setup_parser(root)
    for setup_name in EXTRA_SETUPS:
        setup_path = root / f"assets/obseg/setup/{setup_name}.c"
        deps = parser.parse_setup_dependencies(setup_path.read_text(encoding="utf-8"))
        body_ids.update(deps["unique_guard_body_ids"])
        explicit_heads.update(deps["unique_explicit_guard_head_ids"])

    chr_source = (root / "src/game/chr.c").read_text(encoding="utf-8")
    enum_source = (root / "src/bondconstants.h").read_text(encoding="utf-8")
    enum_block = re.search(r"typedef enum BODIES\s*\{(.*?)\}\s*BODIES;",
                           enum_source, re.DOTALL)
    if enum_block is None:
        raise ValueError("cannot find canonical BODIES enum")
    names: dict[str, int] = {}
    value = -1
    enum_body = re.sub(r"#ifdef ALL_BONDS.*?#endif", "",
                       enum_block.group(1), flags=re.DOTALL)
    enum_body = re.sub(r"^\s*#.*$", "", enum_body, flags=re.MULTILINE)
    for raw in enum_body.split(","):
        raw = re.sub(r"/\*.*?\*/", "", raw, flags=re.DOTALL).strip()
        if not raw or raw.startswith("#"):
            continue
        match = re.match(r"([A-Za-z0-9_]+)(?:\s*=\s*([A-Za-z0-9_]+|0x[0-9A-Fa-f]+|\d+))?", raw)
        if match is None:
            continue
        name, assigned = match.groups()
        if assigned is None:
            value += 1
        elif assigned in names:
            value = names[assigned]
        else:
            value = int(assigned, 0)
        names[name] = value
    head_block = re.search(r"typedef enum HEADS\s*\{(.*?)\}\s*HEADS;",
                           enum_source, re.DOTALL)
    if head_block is None:
        raise ValueError("cannot find canonical HEADS enum")
    value = -1
    for raw in head_block.group(1).split(","):
        raw = re.sub(r"/\*.*?\*/", "", raw, flags=re.DOTALL).strip()
        if not raw or raw.startswith("#"):
            continue
        match = re.match(
            r"([A-Za-z0-9_]+)(?:\s*=\s*([A-Za-z0-9_]+|"
            r"0x[0-9A-Fa-f]+|\d+))?", raw)
        if match is None:
            continue
        name, assigned = match.groups()
        if assigned is None:
            value += 1
        elif assigned in names:
            value = names[assigned]
        else:
            value = int(assigned, 0)
        names[name] = value
    male_block = re.search(r"s32 random_male_heads\[\]\s*=\s*\{(.*?)\};",
                           chr_source, re.DOTALL)
    if male_block is None:
        raise ValueError("cannot find canonical random male head pool")
    male_heads = {
        names[token]
        for token in re.findall(r"HEAD_[A-Za-z0-9_]+", male_block.group(1))
    }
    female_block = re.search(
        r"s32 random_female_heads\[\]\s*=\s*\{(.*?)\};",
        chr_source, re.DOTALL)
    if female_block is None:
        raise ValueError("cannot find canonical random female head pool")
    female_heads = {
        names[token]
        for token in re.findall(
            r"HEAD_[A-Za-z0-9_]+", female_block.group(1))
    }
    if any(body < 0 or body >= len(records) for body in body_ids):
        raise ValueError("setup references an out-of-range body id")
    if any(not records[body]["has_head"] for body in body_ids):
        explicit_heads.update(male_heads)
    # initializeGunBarrelIntro's authored startup-only pair is not referenced
    # by a mission setup, so retain it by canonical resource identity here.
    by_name = {record["name"]: record["model_id"] for record in records}
    body_ids.add(by_name["djbond"])
    # title.c passes BODY_Male_Pierce_Bond_Tuxedo.  In the canonical BODIES
    # enum that is the plain Brosnan head record (CheadbrosnanZ, id 78), not
    # the similarly named CheadbrosnansuitZ character record (id 75).
    explicit_heads.add(by_name["headbrosnan"])
    # The unchanged MENU_DISPLAY_CAST path owns models which are not setup
    # dependencies: Bond wardrobe variants, named cast/guest actors, and the
    # fixed heads selected by intro_char_table/init_menu18_displaycast.
    cast_source = (root / "src/game/front.c").read_text(encoding="utf-8")
    cast_match = re.search(
        r"struct intro_char intro_char_table\[\]\s*=\s*\{(.*?)\n\};",
        cast_source, re.DOTALL)
    cast_init = re.search(
        r"void init_menu18_displaycast\(void\)\s*\{(.*?)\n\}",
        cast_source, re.DOTALL)
    if cast_match is None or cast_init is None:
        raise ValueError("cannot find canonical frontend cast dependencies")
    cast_contract = cast_match.group(1) + cast_init.group(1)
    for token in set(re.findall(r"BODY_[A-Za-z0-9_]+", cast_contract)):
        if token not in names:
            raise ValueError(f"unknown cast body {token}")
        body_ids.add(names[token])
    for token in set(re.findall(r"HEAD_[A-Za-z0-9_]+", cast_contract)):
        if token in ("HEAD_FIXED", "HEAD_RANDOM"):
            continue
        if token not in names:
            raise ValueError(f"unknown cast head {token}")
        explicit_heads.add(names[token])
    # Extended credits includes female HEAD_RANDOM actors. Both original
    # random pools must therefore be packaged for the canonical chooser.
    explicit_heads.update(male_heads)
    explicit_heads.update(female_heads)
    return body_ids, explicit_heads


def checked_manifest(root: Path, blobs: Path | None = None) -> dict[str, Any]:
    records = parse_records(root)
    bodies, heads = dependency_ids(root, records)
    ids = sorted(bodies | heads)
    blobs = blobs or root / SOURCE_BLOBS
    models = []
    total_size = 0
    for model_id in ids:
        model = dict(records[model_id])
        model["roles"] = (["body"] if model_id in bodies else []) + (
            ["head"] if model_id in heads else [])
        source = blobs / model["path"]
        if not source.is_file() or source.is_symlink():
            raise ValueError(f"missing canonical character payload {source}")
        model["size"] = source.stat().st_size
        model["sha256"] = sha256_file(source)
        model["root_node_offset"] = (model["num_switches"] * 4
                                     + model["num_textures"] * 12)
        if model["size"] <= model["root_node_offset"]:
            raise ValueError(f"invalid character model layout for {model['name']}")
        total_size += model["size"]
        models.append(model)
    identity_rows = [[m["model_id"], m["name"], m["roles"], m["size"],
                      m["sha256"]] for m in models]
    return {
        "schema": 1,
        "kind": "goldeneye-us-solo-character-model-dependencies",
        "body_ids": sorted(bodies), "head_ids": sorted(heads),
        "body_count": len(bodies), "head_count": len(heads),
        "model_count": len(models), "total_size": total_size,
        "identity_sha256": sha256_bytes(json.dumps(
            identity_rows, separators=(",", ":")).encode("ascii")),
        "source_table": {"path": MODEL_TABLE.as_posix(),
                         "sha256": sha256_file(root / MODEL_TABLE)},
        "models": models,
    }


def manifest_bytes(manifest: dict[str, Any]) -> bytes:
    return (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")


def verify(output: Path, manifest: dict[str, Any]) -> None:
    expected = {"manifest.json", *(model["path"] for model in manifest["models"])}
    actual = {path.name for path in output.iterdir()
              if path.is_file() and not path.is_symlink()}
    if actual != expected:
        raise ValueError("staged character file set mismatch")
    if (output / "manifest.json").read_bytes() != manifest_bytes(manifest):
        raise ValueError("staged character manifest mismatch")
    for model in manifest["models"]:
        path = output / model["path"]
        if path.stat().st_size != model["size"] or sha256_file(path) != model["sha256"]:
            raise ValueError(f"staged character identity mismatch: {model['path']}")


def stage(root: Path, output: Path, blobs: Path | None = None) -> dict[str, Any]:
    blobs = blobs or root / SOURCE_BLOBS
    manifest = checked_manifest(root, blobs)
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}.", dir=output.parent))
    backup = output.parent / f".{output.name}.previous-{os.getpid()}"
    try:
        for model in manifest["models"]:
            shutil.copyfile(blobs / model["path"], temporary / model["path"])
        (temporary / "manifest.json").write_bytes(manifest_bytes(manifest))
        verify(temporary, manifest)
        if backup.exists():
            shutil.rmtree(backup)
        if output.exists():
            output.rename(backup)
        temporary.rename(output)
        if backup.exists():
            shutil.rmtree(backup)
    except BaseException:
        shutil.rmtree(temporary, ignore_errors=True)
        if backup.exists() and not output.exists():
            backup.rename(output)
        raise
    verify(output, manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--blobs", type=Path)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        manifest = checked_manifest(args.root, args.blobs)
        if args.check:
            verify(args.output, manifest)
        else:
            manifest = stage(args.root, args.output, args.blobs)
    except (OSError, ValueError, KeyError, json.JSONDecodeError) as error:
        raise SystemExit(f"cannot stage character models: {error}") from error
    print(f"verified {manifest['model_count']} exact character models "
          f"({manifest['body_count']} bodies, {manifest['head_count']} heads; "
          f"identity {manifest['identity_sha256']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
