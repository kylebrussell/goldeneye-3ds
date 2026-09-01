#!/usr/bin/env python3
"""Convert the verified private Rareware-logo textures into 3DS T3X files."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib


SEGMENT_SHA256 = "90a5c04ed8461d21e8a766fb01673574b0484604a0284c65cab5527172b2decd"
WIDTH = 49
HEIGHT = 28


@dataclass(frozen=True)
class LogoTexture:
    name: str
    offset: int
    width: int
    height: int
    sha256: str


TEXTURES = (
    LogoTexture("rare-r", 0x0020, 49, 28, "0ea345214d9679a47811aa759f2cbf151bcb2550bdb752db362d87ad46c6b09c"),
    LogoTexture("rare-a", 0x0AE0, 49, 28, "5e095d77f379b8080aa2269843ce2629a18261699c815af5f4e112861ef9b8c4"),
    LogoTexture("rare-r2", 0x15A0, 49, 28, "17363d17954b018b085823cd2039d50822fa2b46a8f762e04ce461e45893dc14"),
    LogoTexture("rare-e", 0x2060, 49, 28, "631a01b0bdcee976797474604149ccd82038a6289573a40a42b6d1eb5628838b"),
    # load_display_rare_logo binds these exact RGBA16 images around the two
    # lit, texture-generated model display lists.  They are part of the logo,
    # not optional lighting maps: both lists combine TEXEL0 * PRIMITIVE.
    LogoTexture("rare-front", 0x4FE8, 32, 32, "2327f0d247cf792c3945fbe868602590a5f685699c29d2d12004c094bbea3a17"),
    LogoTexture("rare-body", 0x5FF0, 32, 32, "0c6fd4c3a18d56a6de46c316641d1c54aac80951ae64145cacd85dae18f80eb0"),
)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def expand_five(value: int) -> int:
    return (value << 3) | (value >> 2)


def decode_rgba5551(data: bytes, width: int = WIDTH, height: int = HEIGHT) -> bytes:
    if len(data) != width * height * 2:
        raise ValueError(f"expected {width * height * 2} texture bytes, got {len(data)}")
    output = bytearray(width * height * 4)
    for pixel in range(width * height):
        value = (data[pixel * 2] << 8) | data[pixel * 2 + 1]
        output[pixel * 4 + 0] = expand_five((value >> 11) & 0x1F)
        output[pixel * 4 + 1] = expand_five((value >> 6) & 0x1F)
        output[pixel * 4 + 2] = expand_five((value >> 1) & 0x1F)
        output[pixel * 4 + 3] = 0xFF if value & 1 else 0
    return bytes(output)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", zlib.crc32(kind + payload))


def encode_png(rgba: bytes, width: int = WIDTH, height: int = HEIGHT) -> bytes:
    stride = width * 4
    rows = b"".join(b"\0" + rgba[row * stride:(row + 1) * stride] for row in range(height))
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(rows, level=9))
        + png_chunk(b"IEND", b"")
    )


def replace_directory(staging: Path, output: Path) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    old = output.with_name(output.name + ".old")
    if old.exists():
        shutil.rmtree(old)
    if output.exists():
        os.replace(output, old)
    os.replace(staging, output)
    if old.exists():
        shutil.rmtree(old)


def convert(segment_path: Path, output: Path, tex3ds: str) -> dict[str, object]:
    segment = segment_path.read_bytes()
    if sha256(segment) != SEGMENT_SHA256:
        raise ValueError("Rareware segment SHA-256 mismatch")

    records: list[dict[str, object]] = []
    output = output.absolute()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-rareware-textures-", dir=output.parent) as temporary:
        staging = Path(temporary) / "output"
        staging.mkdir()
        for texture in TEXTURES:
            end = texture.offset + texture.width * texture.height * 2
            source = segment[texture.offset:end]
            if len(source) != texture.width * texture.height * 2 or sha256(source) != texture.sha256:
                raise ValueError(f"private texture verification failed: {texture.name}")
            png_path = Path(temporary) / f"{texture.name}.png"
            t3x_path = staging / f"{texture.name}.t3x"
            png_path.write_bytes(encode_png(
                decode_rgba5551(source, texture.width, texture.height),
                texture.width,
                texture.height,
            ))
            subprocess.run(
                [tex3ds, "-f", "rgba5551", "-z", "auto", "-o", str(t3x_path), str(png_path)],
                check=True,
            )
            records.append({
                "name": texture.name,
                "segment_offset": texture.offset,
                "width": texture.width,
                "height": texture.height,
                "source_sha256": texture.sha256,
                "png_sha256": sha256(png_path.read_bytes()),
                "t3x": t3x_path.name,
                "t3x_size": t3x_path.stat().st_size,
                "t3x_sha256": sha256(t3x_path.read_bytes()),
            })
        manifest = {"schema": 1, "segment_sha256": SEGMENT_SHA256, "textures": records}
        (staging / "manifest.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )
        replace_directory(staging, output)
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--segment", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tex3ds", default="tex3ds")
    args = parser.parse_args()
    try:
        manifest = convert(args.segment, args.output, args.tex3ds)
    except (OSError, subprocess.CalledProcessError, ValueError) as error:
        raise SystemExit(f"cannot convert Rareware logo textures: {error}") from error
    print(f"converted {len(manifest['textures'])} private Rareware textures -> {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
