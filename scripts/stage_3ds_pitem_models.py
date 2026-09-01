#!/usr/bin/env python3
"""Stage the complete, exact PitemZ model table for the 3DS asset pack.

The ordering and model metadata come from the same source includes that build
``PitemZ_entries``.  Model payloads are copied byte-for-byte from the canonical
US build output; a missing or additional payload is an error rather than an
invitation to synthesize a replacement.
"""

from __future__ import annotations

import argparse
from decimal import Decimal, InvalidOperation
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from typing import Any


TABLE_RELATIVE = Path("assets/obseg/prop/propItemModelFileRecord.inc.c")
SOURCE_BLOBS_RELATIVE = Path("build/u/assets/obseg/prop")
INCLUDE_RE = re.compile(
    r"^\s*#include\s+<(?P<path>assets/obseg/prop/"
    r"(?P<directory>[A-Za-z0-9_]+)/propFileRecord\.inc\.c)>\s*$",
    re.MULTILINE,
)
RECORD_RE = re.compile(
    r"^\s*PROPFILERECORD\s*\(\s*(?P<name>[A-Za-z0-9_]+)\s*,\s*"
    r"(?P<scale>[^,)]+)\s*\)\s*$",
    re.MULTILINE,
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_label(path: Path, root: Path) -> str:
    try:
        return path.relative_to(root).as_posix()
    except ValueError:
        return path.name


def parse_models(root: Path, table: Path | None = None) -> list[dict[str, Any]]:
    table = table or root / TABLE_RELATIVE
    table_text = table.read_text(encoding="utf-8")
    includes = list(INCLUDE_RE.finditer(table_text))
    all_record_includes = re.findall(
        r"^\s*#include\s+[<\"]([^>\"]*propFileRecord\.inc\.c)[>\"]\s*$",
        table_text, re.MULTILINE)
    if not includes:
        raise ValueError(f"no canonical PitemZ records in {table}")
    if len(includes) != len(all_record_includes):
        raise ValueError(f"unsupported PitemZ include syntax/path in {table}")

    models: list[dict[str, Any]] = []
    names: set[str] = set()
    paths: set[str] = set()
    for model_id, include in enumerate(includes):
        include_name = include.group("path")
        directory_name = include.group("directory")
        if include_name in paths:
            raise ValueError(f"duplicate PitemZ include: {include_name}")
        paths.add(include_name)
        record_path = root / include_name
        record_data = record_path.read_bytes()
        matches = list(RECORD_RE.finditer(record_data.decode("utf-8")))
        if len(matches) != 1:
            raise ValueError(
                f"expected one PROPFILERECORD in {include_name}, found {len(matches)}")
        name = matches[0].group("name")
        scale_source = matches[0].group("scale").strip()
        if name != directory_name:
            raise ValueError(
                f"PitemZ name/directory mismatch in {include_name}: {name}")
        if name in names:
            raise ValueError(f"duplicate PitemZ model name: {name}")
        names.add(name)
        try:
            scale_decimal = Decimal(scale_source)
        except InvalidOperation as error:
            raise ValueError(
                f"non-numeric PitemZ scale in {include_name}: {scale_source}") from error
        if not scale_decimal.is_finite() or scale_decimal <= 0:
            raise ValueError(
                f"invalid PitemZ scale in {include_name}: {scale_source}")
        resource = f"P{name}Z"
        models.append({
            "model_id": model_id,
            "order": model_id,
            "name": name,
            "scale": float(scale_decimal),
            "scale_source": scale_source,
            "resource": resource,
            "path": f"{resource}.bin",
            "record": {
                "path": include_name,
                "sha256": sha256_bytes(record_data),
            },
        })
    return models


def checked_manifest(root: Path, table: Path | None = None,
                     blobs: Path | None = None) -> dict[str, Any]:
    table = table or root / TABLE_RELATIVE
    blobs = blobs or root / SOURCE_BLOBS_RELATIVE
    models = parse_models(root, table)
    expected = {model["path"] for model in models}
    actual = {path.name for path in blobs.glob("P*Z.bin") if path.is_file()}
    missing = sorted(expected - actual)
    additional = sorted(actual - expected)
    if missing or additional:
        details = []
        if missing:
            details.append("missing " + ", ".join(missing))
        if additional:
            details.append("additional " + ", ".join(additional))
        raise ValueError("PitemZ payload set mismatch: " + "; ".join(details))

    total_size = 0
    for model in models:
        source = blobs / model["path"]
        if source.is_symlink():
            raise ValueError(f"PitemZ payload must not be a symlink: {source}")
        size = source.stat().st_size
        if size == 0:
            raise ValueError(f"empty PitemZ payload: {source}")
        model["size"] = size
        model["sha256"] = sha256_file(source)
        total_size += size

    identity_rows = [
        [model["model_id"], model["name"], model["scale_source"],
         model["size"], model["sha256"]]
        for model in models
    ]
    identity = sha256_bytes(json.dumps(
        identity_rows, separators=(",", ":"), ensure_ascii=True).encode("utf-8"))
    return {
        "schema": 1,
        "kind": "goldeneye-us-pitemz-table",
        "model_count": len(models),
        "total_size": total_size,
        "identity_sha256": identity,
        "source_table": {
            "path": source_label(table, root),
            "sha256": sha256_file(table),
        },
        "source_payload_directory": source_label(blobs, root),
        "models": models,
    }


def manifest_bytes(manifest: dict[str, Any]) -> bytes:
    return (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode("utf-8")


def verify_stage(output: Path, manifest: dict[str, Any]) -> None:
    expected_files = {"manifest.json"}
    expected_files.update(model["path"] for model in manifest["models"])
    if not output.is_dir() or output.is_symlink():
        raise ValueError(f"PitemZ stage is not a regular directory: {output}")
    actual_files = {
        path.name for path in output.iterdir()
        if path.is_file() and not path.is_symlink()
    }
    nonfiles = [path.name for path in output.iterdir()
                if not path.is_file() or path.is_symlink()]
    if actual_files != expected_files or nonfiles:
        raise ValueError("staged PitemZ file set mismatch")
    if (output / "manifest.json").read_bytes() != manifest_bytes(manifest):
        raise ValueError("staged PitemZ manifest mismatch")
    for model in manifest["models"]:
        path = output / model["path"]
        if path.stat().st_size != model["size"] or sha256_file(path) != model["sha256"]:
            raise ValueError(f"staged PitemZ identity mismatch: {model['path']}")


def stage(root: Path, output: Path, table: Path | None = None,
          blobs: Path | None = None) -> dict[str, Any]:
    table = table or root / TABLE_RELATIVE
    blobs = blobs or root / SOURCE_BLOBS_RELATIVE
    manifest = checked_manifest(root, table, blobs)
    if output.is_symlink():
        raise ValueError(f"refusing symlink output: {output}")
    if output.exists() and not output.is_dir():
        raise ValueError(f"refusing non-directory output: {output}")
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = Path(tempfile.mkdtemp(prefix=f".{output.name}.", dir=output.parent))
    backup = output.parent / f".{output.name}.previous-{os.getpid()}"
    try:
        for model in manifest["models"]:
            shutil.copyfile(blobs / model["path"], temporary / model["path"])
        (temporary / "manifest.json").write_bytes(manifest_bytes(manifest))
        verify_stage(temporary, manifest)
        if backup.exists():
            shutil.rmtree(backup)
        if output.exists():
            output.rename(backup)
        temporary.rename(output)
        if backup.exists():
            shutil.rmtree(backup)
    except BaseException:
        if temporary.exists():
            shutil.rmtree(temporary)
        if backup.exists() and not output.exists():
            backup.rename(output)
        raise
    verify_stage(output, manifest)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--table", type=Path)
    parser.add_argument("--blobs", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--check", action="store_true",
                        help="verify an existing stage without changing it")
    args = parser.parse_args()
    try:
        manifest = checked_manifest(args.root, args.table, args.blobs)
        if args.check:
            verify_stage(args.output, manifest)
        else:
            manifest = stage(args.root, args.output, args.table, args.blobs)
    except (OSError, ValueError) as error:
        raise SystemExit(f"cannot stage PitemZ models: {error}") from error
    print(f"verified {manifest['model_count']} exact PitemZ models "
          f"({manifest['identity_sha256']})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
