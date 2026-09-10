#!/usr/bin/env python3
"""Lock, regenerate, and receipt an incremental multiplayer asset-pack update.

Only characters and multiplayer arenas are regenerated. All other pack payloads
are inherited byte-for-byte, with no claim of fresh conversion or known converter
provenance for those inherited entries. Source roots and ROMs are read-only.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import importlib
import io
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import zlib


SCHEMA = "ge-multiplayer-pack-regeneration-v1"
ROM_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
PREFIXES = ("converted/models/characters", "converted/multiplayer")
EXPECTED_MODELS = 74
EXPECTED_ARENAS = 11
SCRIPT_DIR = Path(__file__).resolve().parent
MAX_PAYLOAD = 64 * 1024 * 1024


def checked_path(path: Path, *, exists=True) -> Path:
    if ".." in path.parts:
        raise ValueError(f"parent traversal in filesystem path: {path}")
    path = path.absolute()
    for component in reversed((path, *path.parents)):
        if component.is_symlink():
            raise ValueError(f"symlink in filesystem path: {component}")
    if exists and not path.exists():
        raise ValueError(f"missing path: {path}")
    return path


def safe_name(name: str) -> str:
    if (not re.fullmatch(r"[A-Za-z0-9_./-]+", name)
            or any(part in ("", ".", "..") for part in name.split("/"))):
        raise ValueError(f"unsafe relative source name: {name!r}")
    return name


def file_bytes(path: Path) -> bytes:
    path = checked_path(path)
    if not path.is_file():
        raise ValueError(f"not a regular file: {path}")
    return path.read_bytes()


def file_identity(path: Path) -> dict:
    data = file_bytes(path)
    return {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def json_digest(value) -> str:
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":")).encode()).hexdigest()


def write_json(path: Path, value) -> None:
    checked_path(path, exists=False)
    with path.open("x") as stream:
        json.dump(value, stream, indent=2, sort_keys=True)
        stream.write("\n")


def walk_sources(directory: Path, suffix: str):
    checked_path(directory)
    for current, dirs, files in os.walk(directory, followlinks=False):
        for name in dirs + files:
            checked_path(Path(current) / name)
        dirs[:] = sorted(name for name in dirs if name != "__pycache__")
        for name in sorted(files):
            if name.endswith(suffix):
                yield Path(current) / name


def source_inventory(root: Path) -> dict:
    root = checked_path(root)
    required = [
        "scripts/filelist.u.csv", "scripts/stage_3ds_character_models.py",
        "scripts/extract_3ds_multiplayer_stages.py",
        "docs/generated/solo_stage_asset_inventory.json",
        "port/include/ge_multiplayer_stage_registry.inc",
        "src/bondconstants.h", "src/game/front.c", "src/game/chr.c",
        "src/game/bg.c", "src/game/chraidata.c",
        "assets/obseg/chr/chrModelFileRecords.inc.c",
    ]
    # Snapshot all Python converter sources, including transitive dynamically
    # loaded modules. Tests are included as conservative extra provenance.
    names = set(required)
    names.update(path.relative_to(root).as_posix()
                 for path in walk_sources(root / "scripts", ".py"))
    for kind in ("setup", "stan"):
        names.update(path.relative_to(root).as_posix()
                     for path in walk_sources(root / "assets/obseg" / kind, ".c"))
    table = (root / "assets/obseg/chr/chrModelFileRecords.inc.c").read_text()
    models = re.findall(r"#include\s+<assets/obseg/chr/([^/<>]+)/chrModelFileRecord\.inc\.c>", table)
    if not models or len(models) != len(set(models)):
        raise ValueError("invalid canonical character metadata table")
    for name in models:
        for leaf in ("chrModelFileRecord.inc.c", "modelFileHeader.inc.c"):
            names.add(safe_name(f"assets/obseg/chr/{name}/{leaf}"))
    return {safe_name(name): file_identity(root / name) for name in sorted(names)}


def runtime_identity() -> dict:
    # No external image/texture tools execute in this bounded update. Identify
    # the actual Python runtime and the standard-library modules used by the
    # converters, rather than recording only a mutable toolchain label.
    modules = {}
    for name in ("zlib", "_struct", "_hashlib", "csv", "json", "re", "pathlib",
                 "tempfile", "shutil", "subprocess", "hashlib"):
        module = importlib.import_module(name)
        path = getattr(module, "__file__", None)
        if path:
            resolved = Path(path).resolve(strict=True)
            modules[name] = {"path": str(resolved), **file_identity(resolved)}
    executable = Path(sys.executable).resolve(strict=True)
    return {"python": sys.version, "executable": str(executable),
            "executable_identity": file_identity(executable),
            "platform": platform.platform(), "machine": platform.machine(),
            "zlib_runtime": zlib.ZLIB_RUNTIME_VERSION, "modules": modules}


def runner_identity() -> dict:
    return {name: file_identity(SCRIPT_DIR / name) for name in
            (Path(__file__).name, "gepack_update.py")}


def make_lock(source_root: Path, rom: Path, base_pack: Path, output: Path) -> dict:
    from gepack_update import read_pack
    source_root, rom, base_pack = map(checked_path, (source_root, rom, base_pack))
    checked_path(output.parent)
    if output.exists() or output.is_symlink():
        raise ValueError("lock output already exists")
    rom_data = rom.read_bytes()
    sha1 = hashlib.sha1(rom_data).hexdigest()
    if sha1 != ROM_SHA1:
        raise ValueError("unsupported source ROM SHA-1")
    pack = read_pack(base_pack)
    if pack.source_sha1 != sha1:
        raise ValueError("base pack's ROM lineage differs from actual source ROM")
    sources = source_inventory(source_root)
    lock = {
        "schema": SCHEMA, "operation": "incremental_regeneration",
        "source_root": str(source_root), "source_files": sources,
        "source_inventory_sha256": json_digest(sources),
        "source_commit": subprocess.check_output(["git", "-C", str(source_root), "rev-parse", "HEAD"], text=True).strip(),
        "source_status": subprocess.check_output(["git", "-C", str(source_root), "status", "--short"], text=True),
        "rom": {"path": str(rom), "sha1": sha1, **file_identity(rom)},
        "base_pack": {"path": str(base_pack), "sha256": pack.sha256,
                      "size": pack.size, "entry_count": len(pack.entries)},
        "runtime": runtime_identity(), "runner_sources": runner_identity(),
        "replacement_prefixes": list(PREFIXES),
        "expected_character_models": EXPECTED_MODELS, "expected_arena_count": EXPECTED_ARENAS,
        "claim": "Only the listed prefixes will be freshly regenerated. Other fullpack payloads are inherited with unknown original conversion provenance.",
    }
    write_json(output, lock)
    return lock


def validate_lock(lock: dict) -> None:
    from gepack_update import read_pack
    if (lock.get("schema") != SCHEMA or lock.get("operation") != "incremental_regeneration"
            or lock.get("replacement_prefixes") != list(PREFIXES)
            or lock.get("expected_character_models") != EXPECTED_MODELS
            or lock.get("expected_arena_count") != EXPECTED_ARENAS):
        raise ValueError("unsupported regeneration lock contract")
    if lock["runtime"] != runtime_identity() or lock["runner_sources"] != runner_identity():
        raise ValueError("Python runtime or update helper differs from lock")
    actual = source_inventory(Path(lock["source_root"]))
    if actual != lock["source_files"] or json_digest(actual) != lock["source_inventory_sha256"]:
        raise ValueError("canonical source or converter bytes differ from lock")
    rom = Path(lock["rom"]["path"])
    if file_identity(rom) != {k: lock["rom"][k] for k in ("size", "sha256")}:
        raise ValueError("source ROM differs from lock")
    if hashlib.sha1(rom.read_bytes()).hexdigest() != lock["rom"]["sha1"] or lock["rom"]["sha1"] != ROM_SHA1:
        raise ValueError("unsupported or changed source ROM")
    pack = read_pack(Path(lock["base_pack"]["path"]))
    if pack.sha256 != lock["base_pack"]["sha256"] or pack.source_sha1 != ROM_SHA1:
        raise ValueError("base pack differs from lock")


def extract_payloads(rom_data: bytes, filelist: str, root: Path) -> dict:
    records = {}
    for row in csv.reader(io.StringIO(filelist)):
        if len(row) != 5:
            raise ValueError("invalid canonical ROM file-list row")
        offset_text, length_text, name, compressed_text, _ = row
        safe_name(name)
        if not name.startswith(tuple("assets/obseg/" + kind + "/" for kind in ("chr", "bg", "setup", "stan"))):
            continue
        if name in records:
            raise ValueError("duplicate canonical ROM payload path")
        offset, length, compressed = int(offset_text), int(length_text), int(compressed_text)
        if offset < 0 or length < 0 or offset + length > len(rom_data) or compressed not in (0, 1):
            raise ValueError("invalid canonical ROM payload bounds or encoding")
        raw = rom_data[offset:offset + length]
        payload = raw
        if compressed:
            if raw[:2] != b"\x11\x72":
                raise ValueError("invalid Rare compressed payload")
            decoder = zlib.decompressobj(-15)
            payload = decoder.decompress(raw[2:], MAX_PAYLOAD + 1)
            if not decoder.eof or decoder.unconsumed_tail or len(payload) > MAX_PAYLOAD:
                raise ValueError("truncated or oversized canonical payload")
        if len(payload) > MAX_PAYLOAD:
            raise ValueError("oversized canonical payload")
        records[name] = {"rom_offset": offset, "rom_size": length, "compressed": bool(compressed),
                         "rom_region_sha256": hashlib.sha256(raw).hexdigest(),
                         "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest()}
        if name.startswith("assets/obseg/chr/"):
            target = root / "build/u" / name
            target.parent.mkdir(parents=True, exist_ok=True)
            with target.open("xb") as stream:
                stream.write(payload)
    if not records:
        raise ValueError("no canonical payloads found")
    return records


def check_outputs(generated: Path, payloads: dict) -> tuple[dict, dict]:
    # Validate an immutable capture. Later compare this exact byte inventory
    # with the pack updater's output, closing the validation/publication gap.
    captured = {safe_name(path.relative_to(generated).as_posix()): file_bytes(path)
                for path in walk_sources(generated, "")}
    identities = {name: {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
                  for name, data in captured.items()}
    packed_identities = {}
    for name, identity in identities.items():
        directory, leaf = name.split("/", 1)
        if directory not in ("characters", "multiplayer"):
            raise ValueError("unexpected generated asset directory")
        prefix = PREFIXES[0] if directory == "characters" else PREFIXES[1]
        packed_identities[prefix + "/" + leaf] = identity

    def canonical_payload(kind, stem):
        # The US file list mixes shared setup files and region-specific setup/u
        # files. Resolve the exact unique stem just as the ROM converter does.
        leaf = safe_name(stem + ".bin")
        if "/" in leaf:
            raise ValueError("nested canonical payload stem")
        matches = [identity for name, identity in payloads.items()
                   if name.startswith("assets/obseg/" + kind + "/")
                   and Path(name).name == leaf]
        if len(matches) != 1:
            raise ValueError(f"missing or ambiguous canonical {kind} payload: {stem}")
        return matches[0]

    manifest = json.loads(captured["characters/manifest.json"])
    models = manifest.get("models", [])
    if manifest.get("model_count") != EXPECTED_MODELS or len(models) != EXPECTED_MODELS:
        raise ValueError("character manifest must contain all 74 canonical dependencies")
    expected = {"manifest.json"}
    for model in models:
        name = safe_name(model["path"])
        if "/" in name or name in expected:
            raise ValueError("duplicate or nested character payload")
        expected.add(name)
        identity = identities["characters/" + name]
        source = payloads["assets/obseg/chr/" + name]
        if identity != {k: source[k] for k in ("size", "sha256")} or identity != {k: model[k] for k in ("size", "sha256")}:
            raise ValueError("staged character differs from verified ROM payload")
    if {name.removeprefix("characters/") for name in captured if name.startswith("characters/")} != expected:
        raise ValueError("unexpected staged character entries")
    keys = sorted({name.split("/")[1] for name in captured if name.startswith("multiplayer/")})
    if len(keys) != EXPECTED_ARENAS or len(set(keys)) != EXPECTED_ARENAS:
        raise ValueError("expected all 11 canonical multiplayer arenas")
    for key in keys:
        safe_name(key)
        stage = "multiplayer/" + key + "/"
        data = json.loads(captured[stage + "manifest.json"])
        if data["key"] != key or data["source_rom_sha1"] != ROM_SHA1:
            raise ValueError("arena identity differs from verified ROM")
        for target, kind, stem in (("background.bin", "bg", "bg_" + data["background"] + "_all_p"),
                                   ("setup.bin", "setup", data["setup"])):
            expected_payload = canonical_payload(kind, stem)
            if identities[stage + target] != {k: expected_payload[k] for k in ("size", "sha256")}:
                raise ValueError("arena background or setup differs from verified ROM")
        for name, identity in data["files"].items():
            if identities[stage + safe_name(name)] != {k: identity[k] for k in ("size", "sha256")}:
                raise ValueError("arena output differs from generated registry manifest")
    return ({"character_models": len(models), "arena_keys": keys,
             "character_identity_sha256": manifest["identity_sha256"]}, packed_identities)


def build(lock_path: Path, output: Path) -> dict:
    from gepack_update import update_pack
    lock_path = checked_path(lock_path)
    lock_data = lock_path.read_bytes()
    lock = json.loads(lock_data)
    validate_lock(lock)
    output = checked_path(output, exists=False)
    checked_path(output.parent)
    if output.exists():
        raise ValueError("output directory already exists; never replace previous evidence")
    output.mkdir()
    logs = output / "logs"; logs.mkdir()
    snapshot = output / "canonical-source"; snapshot.mkdir()
    generated = output / "generated"; generated.mkdir()
    commands = []
    try:
        (output / "lock.json").write_bytes(lock_data)
        for name, identity in lock["source_files"].items():
            target = snapshot / safe_name(name)
            source = Path(lock["source_root"]) / name
            if file_identity(source) != identity:
                raise ValueError("source changed while snapshotting")
            target.parent.mkdir(parents=True, exist_ok=True)
            data = source.read_bytes()
            if {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()} != identity:
                raise ValueError("source changed while reading snapshot")
            target.write_bytes(data)
        rom_path = output / "source-rom.z64"
        rom_data = Path(lock["rom"]["path"]).read_bytes()
        if ({"size": len(rom_data), "sha256": hashlib.sha256(rom_data).hexdigest()}
                != {k: lock["rom"][k] for k in ("size", "sha256")}
                or hashlib.sha1(rom_data).hexdigest() != ROM_SHA1):
            raise ValueError("ROM changed while snapshotting")
        rom_path.write_bytes(rom_data)
        payloads = extract_payloads(rom_data, (snapshot / "scripts/filelist.u.csv").read_text(), snapshot)
        write_json(output / "canonical-payloads.json", payloads)

        def run(name, arguments):
            # -I omits the script directory from sys.path, whereas these
            # canonical converters use sibling imports. A tiny isolated
            # bootstrap adds only the captured script directory explicitly.
            script, *rest = map(str, arguments)
            command = [lock["runtime"]["executable"], "-I", "-S", "-B", "-c",
                "import runpy,sys;sys.path.insert(0,sys.argv[1]);p=sys.argv[2];sys.argv=sys.argv[2:];runpy.run_path(p,run_name='__main__')",
                str(snapshot / "scripts"), script, *rest]
            log_path = logs / (name + ".log")
            with log_path.open("xb") as stream:
                result = subprocess.run(command, cwd=snapshot, stdout=stream, stderr=subprocess.STDOUT,
                                        env={"PATH": os.defpath, "LC_ALL": "C", "TZ": "UTC"})
            commands.append({"name": name, "argv": command, "cwd": str(snapshot),
                             "exit_status": result.returncode, "log": str(log_path.relative_to(output)),
                             **file_identity(log_path)})
            if result.returncode:
                raise ValueError(f"converter {name} failed; see {log_path}")

        stager = snapshot / "scripts/stage_3ds_character_models.py"
        run("characters", [stager, "--root", snapshot, "--output", generated / "characters"])
        run("characters-check", [stager, "--root", snapshot, "--output", generated / "characters", "--check"])
        run("multiplayer", [snapshot / "scripts/extract_3ds_multiplayer_stages.py", "--root", snapshot,
            "--rom", rom_path, "--output", generated / "multiplayer", "--registry",
            snapshot / "port/include/ge_multiplayer_stage_registry.inc", "--check"])
        verified, validated_entries = check_outputs(generated, payloads)
        # Refuse changing converter/source snapshots, runtime, ROM or base
        # input. The source tree itself may continue unrelated development.
        for name, identity in lock["source_files"].items():
            if file_identity(snapshot / name) != identity:
                raise ValueError("converter modified a pinned source file")
        if file_identity(rom_path) != {k: lock["rom"][k] for k in ("size", "sha256")}:
            raise ValueError("converter modified the captured source ROM")
        if lock["runtime"] != runtime_identity() or lock["runner_sources"] != runner_identity():
            raise ValueError("runtime or update helper changed during conversion")
        if file_identity(Path(lock["base_pack"]["path"])) != {k: lock["base_pack"][k] for k in ("size", "sha256")}:
            raise ValueError("base pack changed during conversion")
        update = update_pack(Path(lock["base_pack"]["path"]),
                             dict(zip(PREFIXES, (generated / "characters", generated / "multiplayer"))),
                             output / "goldeneye.u.gepack")
        if update["base"]["sha256"] != lock["base_pack"]["sha256"]:
            raise ValueError("base pack changed before publication")
        packed_regenerated = {item["name"]: {k: item[k] for k in ("size", "sha256")}
                              for item in update["output"]["entries"]
                              if any(item["name"].startswith(prefix + "/") for prefix in PREFIXES)}
        if packed_regenerated != validated_entries:
            raise ValueError("regenerated assets changed after canonical validation")
        receipt = {"schema": SCHEMA, "status": "complete", "operation": "incremental_regeneration",
                   "lock_sha256": hashlib.sha256(lock_data).hexdigest(), "lock": lock,
                   "canonical_payloads": payloads, "canonical_payload_inventory_sha256": json_digest(payloads),
                   "commands": commands, "verified": verified, "pack_update": update,
                   "validated_generated_entries": validated_entries,
                   "claim": "Character and multiplayer prefixes regenerated from the verified ROM and pinned canonical source. All other payloads inherited; not a fresh full conversion."}
        write_json(output / "receipt.json", receipt)
        return receipt
    except BaseException as error:
        pack = output / "goldeneye.u.gepack"
        # This entire directory was created by this run. Also remove a linked
        # candidate if publication succeeded but the helper failed before return.
        if pack.is_file() and not pack.is_symlink():
            pack.unlink()
        write_json(output / "failure.json", {"schema": SCHEMA, "status": "failed", "error": str(error), "commands": commands})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    subs = parser.add_subparsers(dest="command", required=True)
    lock_parser = subs.add_parser("lock")
    for name in ("source-root", "rom", "base-pack", "output"):
        lock_parser.add_argument("--" + name, type=Path, required=True)
    build_parser = subs.add_parser("build")
    build_parser.add_argument("--lock", type=Path, required=True)
    build_parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        if args.command == "lock":
            value = make_lock(args.source_root, args.rom, args.base_pack, args.output)
            print(f"Locked {len(value['source_files'])} source files; ROM {value['rom']['sha1']}")
        else:
            value = build(args.lock, args.output)
            print(json.dumps({"status": value["status"], "verified": value["verified"],
                              "output": value["pack_update"]["output"]["sha256"]}, indent=2))
    except (OSError, ValueError, KeyError, zlib.error, subprocess.CalledProcessError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
