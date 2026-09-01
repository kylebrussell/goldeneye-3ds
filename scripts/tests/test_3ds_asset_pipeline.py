#!/usr/bin/env python3
"""Focused tests for the deterministic 3DS texture and pack pipeline."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path
import stat
import struct
import subprocess
import sys
import tempfile
import unittest

REPO_ROOT = Path(__file__).resolve().parents[2]
CONVERTER = REPO_ROOT / "scripts" / "convert_3ds_textures.py"
PACKER = REPO_ROOT / "scripts" / "pack_3ds_assets.py"
SOURCE_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"


def write_executable(path: Path, contents: str) -> None:
    path.write_text(contents, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR)


def digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def pack_names(path: Path) -> list[str]:
    data = path.read_bytes()
    magic, version, _flags, count, entry_size, index_offset, paths_offset, _data_offset = \
        struct.unpack_from("<8sIIIIQQQ", data)
    if magic != b"GEPACK\0\0" or version != 1 or entry_size != 80:
        raise AssertionError("unexpected pack header")
    names = []
    for index in range(count):
        _hash, path_offset, path_length, _offset, _size = struct.unpack_from(
            "<QIIQQ", data, index_offset + index * 32)
        names.append(data[paths_offset + path_offset:paths_offset + path_offset + path_length].decode())
    return names


class AssetPipelineTests(unittest.TestCase):
    def test_batch_catalog_is_sorted_pruned_and_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            inputs = root / "inputs"
            inputs.mkdir()
            (inputs / "a.bin").write_bytes(b"a")
            (inputs / "B.bin").write_bytes(b"B")
            png_output = root / "output" / "png"
            t3x_output = root / "output" / "t3x"
            png_output.mkdir(parents=True)
            t3x_output.mkdir(parents=True)
            (png_output / "stale.png").write_bytes(b"stale")
            (t3x_output / "stale.t3x").write_bytes(b"stale")
            catalog = root / "output" / "catalog.json"

            tex2png = root / "tex2png"
            write_executable(tex2png, """#!/usr/bin/env python3
from pathlib import Path
import struct, sys
source, destination = Path(sys.argv[1]), Path(sys.argv[2])
destination.mkdir(parents=True, exist_ok=True)
lods = (0, 2) if source.stem == 'B' else (0,)
for lod in lods:
    header = b'\\x89PNG\\r\\n\\x1a\\n' + b'\\x00\\x00\\x00\\x0dIHDR'
    (destination / f'{source.stem}-{lod}.png').write_bytes(header + struct.pack('>II', 8 + lod, 4 + lod))
""")
            tex3ds = root / "tex3ds"
            write_executable(tex3ds, """#!/usr/bin/env python3
from pathlib import Path
import sys
output = Path(sys.argv[sys.argv.index('-o') + 1])
output.write_bytes(b'T3X' + Path(sys.argv[-1]).read_bytes())
""")
            command = [
                sys.executable, str(CONVERTER), "--input", str(inputs),
                "--png-output", str(png_output), "--t3x-output", str(t3x_output),
                "--catalog", str(catalog), "--tex2png", str(tex2png),
                "--tex3ds", str(tex3ds), "--mipmap", "box", "--jobs", "2",
            ]
            subprocess.run(command, check=True, capture_output=True, text=True)
            first_hashes = (digest(catalog), digest(t3x_output / "B-0.t3x"),
                            digest(t3x_output / "B-2.t3x"), digest(t3x_output / "a-0.t3x"))
            self.assertFalse((png_output / "stale.png").exists())
            self.assertFalse((t3x_output / "stale.t3x").exists())
            parsed = json.loads(catalog.read_text(encoding="utf-8"))
            self.assertEqual([item["source"] for item in parsed["textures"]], ["B.bin", "a.bin"])
            self.assertEqual(parsed["texture_count"], 2)
            self.assertEqual(parsed["lod_count"], 3)
            self.assertEqual(parsed["mipmap"], "box")
            self.assertEqual([item["lod"] for item in parsed["textures"][0]["lods"]], [0, 2])
            self.assertNotIn(str(root), catalog.read_text(encoding="utf-8"))

            subprocess.run(command, check=True, capture_output=True, text=True)
            second_hashes = (digest(catalog), digest(t3x_output / "B-0.t3x"),
                             digest(t3x_output / "B-2.t3x"), digest(t3x_output / "a-0.t3x"))
            self.assertEqual(first_hashes, second_hashes)

    def test_extra_directory_is_recursive_and_pack_is_reproducible(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_name:
            root = Path(temporary_name)
            assets = root / "assets"
            extras = root / "extras"
            assets.mkdir()
            (extras / "nested").mkdir(parents=True)
            (assets / "raw.bin").write_bytes(b"raw")
            (extras / "root.t3x").write_bytes(b"root")
            (extras / "nested" / "lod.t3x").write_bytes(b"lod")
            symlink = extras / "ignored-link.t3x"
            symlink.symlink_to(extras / "root.t3x")
            output = root / "assets.gepack"
            command = [
                sys.executable, str(PACKER), "--assets", str(assets),
                "--source-sha1", SOURCE_SHA1,
                "--extra-dir", f"converted/textures={extras}", "--output", str(output),
            ]
            subprocess.run(command, check=True, capture_output=True, text=True)
            first = digest(output)
            self.assertCountEqual(pack_names(output), [
                "raw.bin", "converted/textures/root.t3x",
                "converted/textures/nested/lod.t3x",
            ])
            subprocess.run(command, check=True, capture_output=True, text=True)
            self.assertEqual(first, digest(output))


if __name__ == "__main__":
    unittest.main()
