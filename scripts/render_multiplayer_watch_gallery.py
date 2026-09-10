#!/usr/bin/env python3
"""CPU inspection of tested native watch commands and the real ROM atlas.

This nearest-sample alpha preview is not a PICA/N64 rasterizer or a hardware
screenshot. JSON retains every captured vertex for independent inspection.
"""
import argparse
import json
import math
from pathlib import Path

from PIL import Image, ImageDraw


def render(path, atlas):
    data = json.loads(path.read_text())
    output = Image.new("RGB", (400, 240), (15, 20, 24))
    pixels = output.load()
    coverage = atlas.load()
    vertices = data["vertices"]
    assert len(vertices) % 6 == 0
    for index in range(0, len(vertices), 6):
        a, b, c, aa, cc, d = vertices[index:index + 6]
        assert a == aa and c == cc
        left, top, right, bottom = a[0], 240 - a[1], b[0], 240 - c[1]
        assert left < right and top < bottom
        for y in range(max(0, math.ceil(top)), min(240, math.ceil(bottom))):
            for x in range(max(0, math.ceil(left)), min(400, math.ceil(right))):
                u = a[2] + ((x + .5 - left) / (right - left)) * (b[2] - a[2])
                v = a[3] + ((y + .5 - top) / (bottom - top)) * (c[3] - a[3])
                tx, ty = math.floor(u * atlas.width), math.floor((1 - v) * atlas.height)
                assert 0 <= tx < atlas.width and 0 <= ty < atlas.height
                alpha = coverage[tx, ty] / 255 * a[7]
                old = pixels[x, y]
                pixels[x, y] = tuple(round(a[4 + k] * 255 * alpha + old[k] * (1 - alpha)) for k in range(3))
    output.save(path.with_suffix(".png"))
    return output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    atlas = Image.open(args.directory / "bank-gothic.pgm").convert("L")
    atlas.save(args.directory / "bank-gothic.png")
    paths = sorted(args.directory.glob("*.json"))
    assert paths
    rows = (len(paths) + 2) // 3
    gallery = Image.new("RGB", (3 * 420 + 20, rows * 278 + 60), (31, 37, 44))
    draw = ImageDraw.Draw(gallery)
    draw.text((20, 12), "Native watch pages — CPU font/geometry preview; no hardware rendering claim", fill=(232, 236, 240))
    for index, path in enumerate(paths):
        x, y = 20 + index % 3 * 420, 48 + index // 3 * 278
        draw.text((x, y), path.stem, fill=(202, 211, 220))
        gallery.paste(render(path, atlas), (x, y + 18))
    target = args.directory / "watch-gallery.png"
    gallery.save(target)
    print(f"CPU native watch gallery: {target} ({len(paths)} pages; owned vertex JSON + ROM atlas)")


if __name__ == "__main__":
    main()
