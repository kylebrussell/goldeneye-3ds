#!/usr/bin/env python3
"""Prepare and correctness-check one isolated placement-reuse reversal.

Reads an integrated checkout and writes only a new evidence directory. It never
changes the checkout, builds a game binary, runs an emulator or measures timing.
"""
from __future__ import annotations

import argparse
import ast
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


MODEL = "port/src/ge_original_model_scene.c"
TEST = "scripts/tests/test_model_placement_signature.py"
FAST_PATH = """    if (previous != NULL && input->room_id == previous->room_id
            && memcmp(input->position, previous->position,
                      sizeof(input->position)) == 0
            && memcmp(input->matrix, previous->matrix,
                      sizeof(input->matrix)) == 0)
        return previous_hash;
"""
BASELINE = """    /* Isolated A/B baseline: recompute the existing prefix for every input. */
    (void)previous;
    (void)previous_hash;
"""
INVALIDATION = "placement_input = input->segment4_vertex_count == 0U ? input : NULL;"


def sha(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def identity(path: Path) -> dict:
    data = path.read_bytes()
    return {"size": len(data), "sha256": sha(data)}


def function_span(source: str, name: str) -> tuple[int, int]:
    marker = name + "("
    start = source.rfind("static ", 0, source.index(marker))
    brace = source.index("{", start)
    depth, end = 1, brace + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return start, end


def reverse_reuse(source: str) -> str:
    start, end = function_span(source, "cache_placement_hash")
    body = source[start:end]
    if body.count(FAST_PATH) != 1 or source.count(FAST_PATH) != 1:
        raise ValueError("placement fast path changed; review the final source before regenerating")
    if source.count(INVALIDATION) != 1:
        raise ValueError("expected callback invalidation is missing or ambiguous")
    baseline = source[:start] + body.replace(FAST_PATH, BASELINE) + source[end:]
    new_start, new_end = function_span(baseline, "cache_placement_hash")
    if (source[:start] != baseline[:new_start] or source[end:] != baseline[new_end:]
            or baseline.replace(BASELINE, FAST_PATH, 1) != source):
        raise ValueError("reversal changed bytes outside the intended fast path")
    return baseline


def oracle_code(test_source: str, model_source: str, include_root: Path) -> str:
    # Reuse the existing independent scalar-byte oracle without importing its
    # module, whose final with-statement automatically compiles/runs the test.
    # Pin SOURCE to held bytes and REPO to captured headers before executing
    # only its C-generator portion. No performance driver is used.
    parsed = ast.parse(test_source)
    generator = []
    source_assignments = root_assignments = 0
    for node in parsed.body:
        if isinstance(node, ast.With):
            break
        if isinstance(node, ast.Assign) and len(node.targets) == 1:
            target = node.targets[0]
            if isinstance(target, ast.Name) and target.id == "SOURCE":
                node.value = ast.Constant(model_source)
                source_assignments += 1
            elif isinstance(target, ast.Name) and target.id == "REPO":
                node.value = ast.Call(ast.Name("Path", ast.Load()),
                                      [ast.Constant(str(include_root))], [])
                root_assignments += 1
        generator.append(node)
    if source_assignments != 1 or root_assignments != 1 or len(generator) == len(parsed.body):
        raise ValueError("oracle generator changed; review its isolation before regenerating")
    namespace = {}
    exec(compile(ast.fix_missing_locations(ast.Module(generator, [])), TEST, "exec"), namespace)
    code = namespace["CODE"]
    if code.count("assert(hash_bytes == 0); ++reused;") != 1:
        raise ValueError("oracle reuse counter changed")
    return code


def prepare(root: Path, output: Path, compiler: str) -> dict:
    root = root.resolve(strict=True)
    if output.is_symlink():
        raise ValueError("output must not be a symlink")
    output = output.absolute()
    if output.exists():
        raise ValueError("output already exists; preserve prior evidence")
    source = (root / MODEL).read_bytes().decode("utf-8")
    baseline = reverse_reuse(source)
    test_source = (root / TEST).read_bytes().decode("utf-8")
    output.mkdir(parents=True)
    commands = []
    receipt = {"schema": 1, "status": "incomplete", "mode": "correctness_only",
               "source_root": str(root), "changed_source": MODEL,
               "candidate_sha256": sha(source.encode()), "baseline_sha256": sha(baseline.encode()),
               "source_commit": subprocess.check_output(["git", "-C", str(root), "rev-parse", "HEAD"], text=True).strip(),
               "source_status": subprocess.check_output(["git", "-C", str(root), "status", "--short"], text=True),
               "oracle_sha256": sha(test_source.encode()), "helper": identity(Path(__file__).resolve()),
               "commands": commands}
    try:
        (output / "candidate-source.c").write_bytes(source.encode("utf-8"))
        (output / "baseline-source.c").write_bytes(baseline.encode("utf-8"))
        (output / "oracle-source.py").write_bytes(test_source.encode("utf-8"))
        patch = "".join(difflib.unified_diff(source.splitlines(True), baseline.splitlines(True),
                        fromfile="a/" + MODEL, tofile="b/" + MODEL))
        (output / "baseline.patch").write_bytes(patch.encode("utf-8"))
        receipt["patch_sha256"] = sha(patch.encode())
        snapshot = output / "snapshot"
        headers = {}
        for path in sorted((root / "port/include").rglob("*")):
            if path.is_symlink():
                raise ValueError("header snapshot contains a symlink")
            if path.is_file():
                relative = path.relative_to(root)
                data = path.read_bytes()
                headers[relative.as_posix()] = {"size": len(data), "sha256": sha(data)}
                target = snapshot / relative
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
        receipt["headers"] = headers
        cc = Path(shutil.which(compiler) or compiler).resolve(strict=True)
        receipt["compiler"] = {"path": str(cc), **identity(cc),
                               "version": subprocess.check_output([str(cc), "--version"], text=True)}

        def run(name, command, cwd=output):
            log = output / (name + ".log")
            with log.open("xb") as stream:
                result = subprocess.run(command, cwd=cwd, stdout=stream, stderr=subprocess.STDOUT)
            commands.append({"name": name, "argv": command, "cwd": str(cwd),
                             "exit_status": result.returncode, "log": log.name, **identity(log)})
            if result.returncode:
                raise ValueError(f"{name} failed; see {log}")

        # Apply and reverse the patch only against a private copy; verify exact
        # full-file bytes in both directions, independently of string reversal.
        private_model = snapshot / MODEL
        private_model.parent.mkdir(parents=True, exist_ok=True)
        private_model.write_bytes(source.encode("utf-8"))
        run("patch-check", ["git", "apply", "--check", str(output / "baseline.patch")], snapshot)
        run("patch-apply", ["git", "apply", str(output / "baseline.patch")], snapshot)
        if private_model.read_bytes() != baseline.encode("utf-8"):
            raise ValueError("applied patch differs from baseline source")
        run("patch-reverse", ["git", "apply", "--reverse", str(output / "baseline.patch")], snapshot)
        if private_model.read_bytes() != source.encode("utf-8"):
            raise ValueError("reverse patch differs from candidate source")

        for variant, model in (("candidate", source), ("baseline", baseline)):
            code = oracle_code(test_source, model, snapshot)
            if variant == "baseline":
                # The single intentional behavioral difference is doing all
                # 160 hash byte steps even for an otherwise reusable prefix.
                code = code.replace("assert(hash_bytes == 0); ++reused;",
                                    "assert(hash_bytes == 160); ++rebuilt;")
            cfile = output / (variant + "-oracle.c")
            cfile.write_bytes(code.encode("utf-8"))
            for opt in ("-O2", "-O3"):
                name = variant + opt
                binary = output / name
                run(name + "-compile", [str(cc), "-std=c11", opt, "-Wall", "-Wextra", "-Werror",
                    "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                    "-I" + str(snapshot / "port/include"), str(cfile), "-lm", "-o", str(binary)])
                run(name + "-verify", [str(binary)])
        if ((root / MODEL).read_bytes() != source.encode("utf-8")
                or (root / TEST).read_bytes() != test_source.encode("utf-8")):
            raise ValueError("source changed during preparation; regenerate after freeze")
        for name, expected in headers.items():
            if identity(root / name) != expected:
                raise ValueError("headers changed during preparation; regenerate after freeze")
        receipt["status"] = "complete"
        receipt["verified_difference"] = "Only cache_placement_hash's equality/reuse return is replaced by unconditional original-format hashing. Callback invalidation and all other source bytes remain intact."
    except BaseException as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        raise
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
    return receipt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--compiler", default="cc")
    args = parser.parse_args()
    try:
        receipt = prepare(args.source_root, args.output, args.compiler)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        parser.error(str(error))
    print(json.dumps({key: receipt[key] for key in ("status", "candidate_sha256", "baseline_sha256", "patch_sha256")}, indent=2))


if __name__ == "__main__":
    main()
