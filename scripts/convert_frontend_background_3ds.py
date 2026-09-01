#!/usr/bin/env python3
"""Convert GoldenEye's exact RLE folder backdrop into a PICA T3X texture."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile
import zlib


SOURCE_SHA256 = "b0704f2db5c0fc820050317a2848bba93b95dec4d37f6c1e040ffadb2bad8cf5"
WIDTH = 440
HEIGHT = 299
X_OFFSET = -28


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload
            + struct.pack(">I", zlib.crc32(kind + payload)))


def encode_png(rgba: bytes) -> bytes:
    stride = WIDTH * 4
    rows = b"".join(
        b"\0" + rgba[row * stride:(row + 1) * stride]
        for row in range(HEIGHT)
    )
    return (
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", struct.pack(
            ">IIBBBBB", WIDTH, HEIGHT, 8, 6, 0, 0, 0))
        + png_chunk(b"IDAT", zlib.compress(rows, level=9))
        + png_chunk(b"IEND", b"")
    )


def decode(source: bytes) -> bytes:
    if hashlib.sha256(source).hexdigest() != SOURCE_SHA256:
        raise ValueError("folder backdrop source SHA-256 mismatch")
    if int.from_bytes(source[0:2], "big") != WIDTH \
            or int.from_bytes(source[2:4], "big") != HEIGHT:
        raise ValueError("folder backdrop dimensions mismatch")
    intensity = bytearray()
    cursor = 10
    while len(intensity) < WIDTH * HEIGHT:
        if cursor + 2 > len(source):
            raise ValueError("truncated folder backdrop RLE")
        count, value = source[cursor], source[cursor + 1]
        cursor += 2
        if count == 0 or len(intensity) + count > WIDTH * HEIGHT:
            raise ValueError("invalid folder backdrop RLE run")
        intensity.extend([value] * count)
    rgba = bytearray(WIDTH * HEIGHT * 4)
    for y in range(HEIGHT):
        primitive = 20 + 30 * y // (HEIGHT - 1)
        for x in range(WIDTH):
            # constructor_menu05 uses xOffset=floor(440*-80/1280)=-28.
            source_x = min(WIDTH - 1, x - X_OFFSET)
            texel = intensity[y * WIDTH + source_x]
            # G_CC blend: (TEXEL0-PRIMITIVE)*ENV_ALPHA+PRIMITIVE,
            # with environment alpha 0x14 and the per-line 20..50 primitive.
            shade = (texel * 20 + primitive * 235 + 127) // 255
            pixel = (y * WIDTH + x) * 4
            rgba[pixel:pixel + 4] = bytes((shade, shade, shade, 255))
    return bytes(rgba)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tex3ds", default="tex3ds")
    args = parser.parse_args()
    rgba = decode(args.input.read_bytes())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="ge-frontend-background-") as tmp:
        png = Path(tmp) / "folder-background.png"
        png.write_bytes(encode_png(rgba))
        subprocess.run([
            args.tex3ds, "-f", "rgba4", "-z", "none",
            "-o", str(args.output), str(png),
        ], check=True)
    print(f"converted exact {WIDTH}x{HEIGHT} folder backdrop -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
