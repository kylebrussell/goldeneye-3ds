#!/usr/bin/env python3
"""Convert extracted GoldenEye textures into a deterministic 3DS catalog.

The converter deliberately emits one T3X per source LOD.  This keeps every
runtime resource independently addressable and avoids forcing unrelated level
textures into a single, memory-resident atlas.
"""

from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import re

CATALOG_SCHEMA = 1


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as stream:
        header = stream.read(24)
    if len(header) != 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG image: {path}")
    return struct.unpack(">II", header[16:24])


def select_texture_format(path: Path, requested: str) -> str:
    if requested != "auto":
        return requested
    with path.open("rb") as stream:
        header = stream.read(26)
    if len(header) != 26 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"missing PNG color type: {path}")
    # tex2png preserves original intensity/alpha images as PNG gray+alpha.
    # RGBA5551 erased fractional alpha (GLASS3 is uniformly 96/255). LA8
    # preserves both channels without increasing the 16-bit GPU footprint.
    return "la8" if header[25] == 4 else "rgba5551"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True,
                        help="directory containing extracted .bin textures")
    parser.add_argument("--png-output", type=Path, required=True,
                        help="generated PNG staging directory")
    parser.add_argument("--t3x-output", type=Path, required=True,
                        help="generated T3X directory")
    parser.add_argument("--catalog", type=Path, required=True,
                        help="generated JSON catalog")
    parser.add_argument("--tex2png", type=Path, required=True)
    parser.add_argument("--tex3ds", default="tex3ds")
    parser.add_argument("--images-def", type=Path,
                        help="images.def used to attach original numeric image IDs")
    parser.add_argument("--format", default="auto",
                        help="tex3ds output format (auto: la8 for gray+alpha, rgba5551 otherwise)")
    parser.add_argument("--compression", default="auto")
    parser.add_argument("--mipmap",
                        help="tex3ds mipmap filter (for example: box)")
    parser.add_argument("--texture", action="append", default=[], metavar="PATH",
                        help="convert only this input-relative .bin (repeatable)")
    parser.add_argument("--limit", type=int,
                        help="convert at most N sorted inputs (for smoke tests)")
    parser.add_argument("--jobs", type=int, default=1,
                        help="number of textures to convert concurrently (default: 1)")
    return parser.parse_args()


def discover_inputs(root: Path, selected: list[str], limit: int | None) -> list[Path]:
    if selected:
        inputs = []
        for name in selected:
            relative = Path(name)
            if relative.is_absolute() or ".." in relative.parts:
                raise SystemExit(f"invalid texture path: {name}")
            candidate = root / relative
            if not candidate.is_file() or candidate.is_symlink():
                raise SystemExit(f"texture not found: {candidate}")
            inputs.append(candidate)
    else:
        inputs = [path for path in root.rglob("*.bin") if path.is_file() and not path.is_symlink()]

    inputs.sort(key=lambda path: path.relative_to(root).as_posix().encode("utf-8"))
    relative_names = [path.relative_to(root).as_posix() for path in inputs]
    if len(relative_names) != len(set(relative_names)):
        raise SystemExit("duplicate texture input path")
    folded = [name.casefold() for name in relative_names]
    if len(folded) != len(set(folded)):
        raise SystemExit("case-insensitive texture path collision")
    if limit is not None:
        if limit < 0:
            raise SystemExit("--limit cannot be negative")
        inputs = inputs[:limit]
    return inputs


def replace_directory(staging: Path, destination: Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    old = destination.with_name(destination.name + ".old")
    if old.exists():
        shutil.rmtree(old)
    if destination.exists():
        os.replace(destination, old)
    os.replace(staging, destination)
    if old.exists():
        shutil.rmtree(old)


def run_tool(command: list[str]) -> None:
    result = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode != 0:
        if result.stdout:
            sys.stderr.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        raise subprocess.CalledProcessError(result.returncode, command)


def load_image_ids(path: Path | None) -> dict[str, int]:
    if path is None:
        return {}
    result: dict[str, int] = {}
    pattern = re.compile(r"^\s*IMAGE\(([^,]+),")
    for line in path.read_text(encoding="utf-8").splitlines():
        match = pattern.match(line)
        if match is not None:
            name = match.group(1).strip()
            if not name or name in result:
                raise ValueError(f"duplicate or empty image name in {path}: {name!r}")
            result[name] = len(result)
    if not result:
        raise ValueError(f"no IMAGE records found in {path}")
    return result


def convert_texture(source: Path, input_root: Path, staged_png: Path, staged_t3x: Path,
                    tex2png: Path, tex3ds: str, texture_format: str,
                    compression: str, mipmap: str | None,
                    image_ids: dict[str, int]) -> dict[str, object]:
    relative = source.relative_to(input_root)
    relative_stem = relative.with_suffix("")
    png_directory = staged_png / relative.parent
    t3x_directory = staged_t3x / relative.parent
    png_directory.mkdir(parents=True, exist_ok=True)
    t3x_directory.mkdir(parents=True, exist_ok=True)
    run_tool([str(tex2png), str(source), str(png_directory)])
    lod_pngs = sorted(
        png_directory.glob(relative_stem.name + "-[0-9].png"),
        key=lambda path: int(path.stem.rsplit("-", 1)[1]),
    )
    if not lod_pngs:
        raise RuntimeError(f"tex2png produced no LODs for {relative.as_posix()}")

    lods: list[dict[str, object]] = []
    for png in lod_pngs:
        lod = int(png.stem.rsplit("-", 1)[1])
        relative_t3x = relative_stem.parent / f"{relative_stem.name}-{lod}.t3x"
        t3x = staged_t3x / relative_t3x
        selected_format = select_texture_format(png, texture_format)
        command = [
            tex3ds, "-f", selected_format, "-z", compression,
            "-o", str(t3x),
        ]
        if mipmap is not None:
            command.extend(["-m", mipmap])
        command.append(str(png))
        run_tool(command)
        width, height = png_dimensions(png)
        lods.append({
            "lod": lod,
            "format": selected_format,
            "width": width,
            "height": height,
            "png": (relative_stem.parent / png.name).as_posix(),
            "png_sha256": file_sha256(png),
            "t3x": relative_t3x.as_posix(),
            "t3x_size": t3x.stat().st_size,
            "t3x_sha256": file_sha256(t3x),
        })

    record: dict[str, object] = {
        "source": relative.as_posix(),
        "source_size": source.stat().st_size,
        "source_sha256": file_sha256(source),
        "lods": lods,
    }
    image_name = relative.with_suffix("").as_posix()
    if image_name in image_ids:
        record["image_id"] = image_ids[image_name]
    return record


def main() -> int:
    args = parse_args()
    input_root = args.input.resolve()
    if not input_root.is_dir():
        raise SystemExit(f"texture input directory not found: {input_root}")
    tex2png = args.tex2png.resolve()
    if not tex2png.is_file():
        raise SystemExit(f"tex2png not found: {tex2png}")
    if args.jobs < 1:
        raise SystemExit("--jobs must be at least 1")
    try:
        image_ids = load_image_ids(args.images_def)
    except (OSError, UnicodeError, ValueError) as error:
        raise SystemExit(f"cannot load image IDs: {error}") from error

    inputs = discover_inputs(input_root, args.texture, args.limit)
    png_output = args.png_output.resolve()
    t3x_output = args.t3x_output.resolve()
    catalog_path = args.catalog.resolve()
    common_parent = png_output.parent
    common_parent.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="ge-textures-", dir=common_parent) as temporary_name:
        temporary = Path(temporary_name)
        staged_png = temporary / "png"
        staged_t3x = temporary / "t3x"
        staged_png.mkdir()
        staged_t3x.mkdir()

        def worker(source: Path) -> dict[str, object]:
            return convert_texture(source, input_root, staged_png, staged_t3x, tex2png,
                                   args.tex3ds, args.format, args.compression,
                                   args.mipmap, image_ids)

        textures = []
        with ThreadPoolExecutor(max_workers=args.jobs) as executor:
            # executor.map yields in input order, preserving byte-identical JSON
            # regardless of which conversion worker finishes first.
            for number, texture in enumerate(executor.map(worker, inputs), start=1):
                textures.append(texture)
                if number % 100 == 0 or number == len(inputs):
                    print(f"converted {number}/{len(inputs)} textures", flush=True)

        catalog = {
            "schema": CATALOG_SCHEMA,
            "texture_count": len(textures),
            "lod_count": sum(len(item["lods"]) for item in textures),
            "format": args.format,
            "compression": args.compression,
            "mipmap": args.mipmap,
            "textures": textures,
        }
        encoded_catalog = (json.dumps(catalog, indent=2, sort_keys=True) + "\n").encode("utf-8")

        # Generated directories are replaced as a unit, so stale outputs from a
        # previous full or partial run cannot leak into the asset pack.
        replace_directory(staged_png, png_output)
        replace_directory(staged_t3x, t3x_output)
        catalog_path.parent.mkdir(parents=True, exist_ok=True)
        staged_catalog = catalog_path.with_suffix(catalog_path.suffix + ".tmp")
        staged_catalog.write_bytes(encoded_catalog)
        os.replace(staged_catalog, catalog_path)

    print(f"cataloged {len(textures)} textures / {catalog['lod_count']} LODs -> {catalog_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
