#!/usr/bin/env python3
"""Build the production multiplayer graph natively from its 3DS source recipe.

No fixture gameplay consumers or previously compiled objects are linked. The
Makefile owns source extraction and per-unit slice defines. Host-only additions
are the existing explicit pointer ABI bridges and sanitizer instrumentation.
The archive and JSON compile ledger are intermediate acceptance artifacts.
"""
from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path)
    parser.add_argument("--jobs", type=int, default=2)
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    build = args.build or repo / "build/host-tests/multiplayer-match"
    build.mkdir(parents=True, exist_ok=True)
    makefile = repo / "platform/3ds/Makefile"
    lines = makefile.read_text().replace("\\\n", " ").splitlines()
    top = str(repo / "platform/3ds")
    per_unit: dict[str, list[str]] = {}
    generated: dict[str, list[str]] = {}
    target = ""
    for line in lines:
        flags = re.match(r"^([^:#]+): CFLAGS \+= (.*)$", line)
        if flags:
            # Preserve gameplay slice selection; warnings, ARM instructions,
            # platform include paths and optimization belong to this compiler.
            defines = [word for word in shlex.split(flags[2])
                       if word.startswith(("-D", "-fno-builtin"))]
            for name in flags[1].split():
                per_unit.setdefault(name, []).extend(defines)
        match = re.match(r"^(ge_original_[\w]+\.c):", line)
        if match:
            target = match[1]
        elif line and not line[0].isspace():
            target = ""
        if target and line.lstrip().startswith("python3 "):
            command = line.strip().replace("$(TOPDIR)", top).replace(
                "$@", str(build / target))
            if "$" in command:
                raise ValueError(f"unexpanded source recipe: {command}")
            generated[target] = shlex.split(command)
    # Production selects the complete retained props graph.
    generated.pop("ge_original_dam_guard_chr_scheduler_support_slice.c", None)
    for name, command in generated.items():
        subprocess.run(command, cwd=repo, check=True, stdout=subprocess.DEVNULL)

    sources = list((repo / "port/src").glob("*.c"))
    sources.extend(build / name for name in generated)
    names: list[str] = []
    for line in lines:
        match = re.match(r"^(?:GAME_CFILES|SETUP_CFILES)\s*[:+]=\s*(.*)", line)
        if match:
            names.extend(match[1].split())
        languages = re.match(r"^SOLO_LANGUAGE_NAMES\s*:=\s*(.*)", line)
        if languages:
            names.extend(name + ".c" for name in languages[1].split())
    # Text arrays are the native language owner used by pickups/HUD/results.
    names.extend("LgunE.c LmiscE.c LoptionsE.c LpropobjE.c LtitleE.c LmpweaponsE.c".split())
    candidates = list((repo / "src").rglob("*.c"))
    candidates.extend((repo / "assets/obseg/text").glob("*.c"))
    candidates.extend((repo / "assets/font").glob("*.c"))
    candidates.extend((repo / "assets/obseg/setup").glob("*.c"))
    for name in dict.fromkeys(names):
        found = [path for path in candidates if path.name == name]
        # The production search order puts libultra ahead of libultrare.
        found.sort(key=lambda path: "libultrare" in path.parts)
        if not found:
            raise ValueError(f"missing canonical source: {name}")
        sources.append(found[0])
    flags = ["cc", "-std=gnu11", "-O1", "-g", "-Wno-everything",
             "-fno-omit-frame-pointer", "-fsanitize=address,undefined",
             "-ffunction-sections", "-fdata-sections", "-fms-extensions",
             "-DGE_PORT_MS_INHERITS", "-D_LANGUAGE_C",
             "-DGE_PORT_FULL_GAMEPLAY_RUNTIME", "-DGE_DAM_FULL_PROPS_LIVE=1",
             "-DGE_DAM_STAGE_RUNTIME_LIVE=1",
             "-DGE_PORT_GUN_HOST_MODEL_ABI", "-DGE_PORT_MODEL_HOST_RWDATA_ABI",
             "-DGE_PORT_BOND_INPUT_HOST_STATE", "-DGE_PORT_MODEL_HIT_NATIVE_ABI",
             "-DGE_PORT_DAMAGE_HOST_ANIMATION_OFFSETS",
             "-DGE_PORT_ANIMATION_INIT_HOST_POINTER_ABI",
             "-DGE_PORT_ANIMATION_INIT_NATIVE_ABI",
             "-DGE_PORT_VTXSTORE_HOST_POINTER_ABI",
             "-DGE_PORT_ANIMATION_TABLE_HOST_TEST", "-DGE_PORT_HOST_ABI",
             "-I" + str(repo), "-I" + str(repo / "port/include"),
             "-I" + str(repo / "port/src"), "-I" + str(build),
             "-I" + str(repo / "src/game"), "-I" + str(repo / "src"),
             "-I" + str(repo / "src/libultra"),
             "-idirafter", str(repo / "include"),
             "-idirafter", str(repo / "include/PR")]
    header_hash = hashlib.sha256(b"".join(path.read_bytes() for path in sorted(
        (repo / "port/include").glob("*.h")))).hexdigest()
    ledger = []
    def fingerprint(entry):
        dependency_file = Path(entry["object"] + ".d")
        dependencies = []
        if dependency_file.exists():
            declaration = dependency_file.read_text().replace("\\\n", " ")
            dependencies = shlex.split(declaration.split(":", 1)[1])
        paths = sorted(set(dependencies + [entry["source"]]))
        digest = hashlib.sha256(json.dumps(entry["command"]).encode()
                                + header_hash.encode())
        for name in paths:
            path = Path(name)
            digest.update(name.encode())
            digest.update(path.read_bytes() if path.exists() else b"missing")
        entry["dependencies"] = paths
        entry["sha256"] = digest.hexdigest()
    for source in sources:
        obj = build / (source.stem + ".o")
        command = flags + per_unit.get(obj.name, []) + ["-MMD", "-MF", str(obj) + ".d",
            "-c", str(source), "-o", str(obj)]
        entry = dict(source=str(source), object=str(obj), command=command,
                     recipe=generated.get(source.name))
        fingerprint(entry)
        ledger.append(entry)
    def compile_one(entry):
        stamp = Path(entry["object"] + ".sha256")
        if stamp.exists() and stamp.read_text() == entry["sha256"]:
            return None
        result = subprocess.run(entry["command"], text=True, capture_output=True)
        Path(entry["object"] + ".log").write_text(result.stdout + result.stderr)
        if result.returncode:
            return Path(entry["source"]).name
        fingerprint(entry)
        stamp.write_text(entry["sha256"])
        return None
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as executor:
        failures = [name for name in executor.map(compile_one, ledger) if name]
    (build / "compile-ledger.json").write_text(json.dumps(ledger, indent=2) + "\n")
    if failures:
        print("Failed native production units (details in *.o.log): " + ", ".join(failures))
        raise SystemExit(1)
    archive = build / "libmultiplayer-native.a"
    archive.unlink(missing_ok=True)
    subprocess.run(["ar", "rcs", str(archive)] + [entry["object"] for entry in ledger], check=True)
    print(f"Fresh production source graph: {len(ledger)} units; {archive}")


if __name__ == "__main__":
    main()
