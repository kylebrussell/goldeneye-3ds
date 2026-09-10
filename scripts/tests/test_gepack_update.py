from __future__ import annotations

from dataclasses import FrozenInstanceError
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("gepack_update", ROOT / "scripts/gepack_update.py")
assert SPEC is not None and SPEC.loader is not None
update = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = update
SPEC.loader.exec_module(update)
SOURCE_SHA1 = "abe01e4aeb033b6c0836819f549c791b26cfde83"
HEADER = struct.Struct("<8sIIIIQQQ20s12x")
ENTRY = struct.Struct("<QIIQQ")


def reference_fnv(value: bytes) -> int:
    value_hash = 0xCBF29CE484222325
    for byte in value:
        value_hash ^= byte
        value_hash *= 0x100000001B3
        value_hash &= (1 << 64) - 1
    return value_hash


def fixture_blob(entries: dict[str, bytes], hash_function=reference_fnv, reverse=False) -> bytearray:
    """Independent fixture writer, including intentionally unsafe path fixtures."""
    records = sorted(((hash_function(name.encode()), name.encode(), payload)
                      for name, payload in entries.items()), reverse=reverse)
    paths = b"".join(name for _hash, name, _payload in records)
    path_start = 80 + 32 * len(records)
    payload_start = ((path_start + len(paths) + 15) // 16) * 16
    result = bytearray(payload_start)
    HEADER.pack_into(result, 0, b"GEPACK\0\0", 1, 0, len(records), 80,
                     80, path_start, payload_start, bytes.fromhex(SOURCE_SHA1))
    path_cursor = 0
    for index, (name_hash, name, payload) in enumerate(records):
        ENTRY.pack_into(result, 80 + index * 32, name_hash, path_cursor,
                        len(name), len(result), len(payload))
        result[path_start + path_cursor:path_start + path_cursor + len(name)] = name
        path_cursor += len(name)
        result.extend(payload)
        while len(result) % 16:
            result.append(0)
    return result


class IncrementalPackTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        # macOS /var and /tmp are symlinks. Fixtures explicitly use the physical
        # directory; the tested library must never silently resolve caller paths.
        self.root = Path(self.temporary.name).resolve()
        self.base = self.root / "base.gepack"
        self.destination = self.root / "updated.gepack"
        self.replacement = self.root / "replacement"
        self.replacement.mkdir()
        self.payloads = {
            "canonical/setup.bin": b"canonical\0\xffdata",
            "characters/obsolete.bin": b"old",
            "characters/body.bin": b"old body",
            "characters2/keep.bin": b"untouched prefix sibling",
            "converted/levels/dam.bin": bytes(range(256)),
            "zero.bin": b"",
        }
        self.base.write_bytes(fixture_blob(self.payloads))
        (self.replacement / "body.bin").write_bytes(b"new body")
        (self.replacement / "head.bin").write_bytes(b"new head")

    def run_update(self, **kwargs):
        return update.update_pack(kwargs.get("base", self.base),
                                  kwargs.get("replacements", {"characters": self.replacement}),
                                  kwargs.get("output", self.destination))

    def assert_rejected_without_writes(self, **kwargs) -> None:
        before = {path.relative_to(self.root): path.read_bytes()
                  for path in self.root.rglob("*") if path.is_file() and not path.is_symlink()}
        with self.assertRaises(update.GEPackError):
            self.run_update(**kwargs)
        after = {path.relative_to(self.root): path.read_bytes()
                 for path in self.root.rglob("*") if path.is_file() and not path.is_symlink()}
        self.assertEqual(before, after)

    def test_update_preserves_every_untouched_payload_and_records_full_inventory(self) -> None:
        original_bytes = self.base.read_bytes()
        receipt = self.run_update()
        self.assertEqual(self.base.read_bytes(), original_bytes)
        result = update.read_pack(self.destination)
        self.assertEqual(result.source_sha1, SOURCE_SHA1)
        expected = {name: payload for name, payload in self.payloads.items()
                    if not name.startswith("characters/")}
        expected.update({"characters/body.bin": b"new body", "characters/head.bin": b"new head"})
        self.assertEqual(dict(result.entries), expected)
        self.assertEqual(receipt["operation"], "incremental_pack_update")
        self.assertEqual(receipt["source_rom_sha1"], SOURCE_SHA1)
        self.assertEqual(receipt["removed_entries"], ["characters/obsolete.bin"])
        self.assertEqual(receipt["regenerated_entries"], ["characters/body.bin", "characters/head.bin"])
        self.assertEqual(receipt["preserved_entries"], sorted(set(expected) - set(receipt["regenerated_entries"])))
        for section, path, entries in (("base", self.base, self.payloads),
                                       ("output", self.destination, expected)):
            inventory = receipt[section]
            self.assertEqual(inventory["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
            self.assertEqual(inventory["size"], path.stat().st_size)
            self.assertEqual(inventory["entry_count"], len(entries))
            self.assertEqual(inventory["entries"], [
                {"name": name, "size": len(payload), "sha256": hashlib.sha256(payload).hexdigest(),
                 "path_hash": f"{reference_fnv(name.encode()):016x}"}
                for name, payload in sorted(entries.items())])
        json.dumps(receipt, allow_nan=False)

    def test_encoding_matches_existing_packer_byte_for_byte_and_is_deterministic(self) -> None:
        self.run_update()
        updated = update.read_pack(self.destination)
        assets = self.root / "assets"
        assets.mkdir()
        for name, payload in reversed(list(updated.entries.items())):
            path = assets / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(payload)
        expected = self.root / "reference.gepack"
        subprocess.run([sys.executable, str(ROOT / "scripts/pack_3ds_assets.py"),
                        "--assets", str(assets), "--output", str(expected),
                        "--source-sha1", SOURCE_SHA1], check=True, capture_output=True)
        self.assertEqual(self.destination.read_bytes(), expected.read_bytes())
        second = self.root / "second.gepack"
        self.run_update(output=second)
        self.assertEqual(second.read_bytes(), self.destination.read_bytes())

    def test_noop_replacement_is_byte_identical(self) -> None:
        for name, payload in self.payloads.items():
            if name.startswith("characters/"):
                (self.replacement / name.split("/", 1)[1]).write_bytes(payload)
        (self.replacement / "head.bin").unlink()
        self.run_update()
        self.assertEqual(self.base.read_bytes(), self.destination.read_bytes())

    def test_empty_replacement_explicitly_removes_subtree(self) -> None:
        for path in self.replacement.iterdir():
            path.unlink()
        receipt = self.run_update()
        self.assertFalse(any(name.startswith("characters/")
                             for name in update.read_pack(self.destination).entries))
        self.assertEqual(receipt["regenerated_entries"], [])
        self.assertEqual(receipt["removed_entries"], sorted(name for name in self.payloads
                                                          if name.startswith("characters/")))

    def test_multiple_prefixes_and_nested_unicode_files(self) -> None:
        arena = self.root / "arena"
        (arena / "nested").mkdir(parents=True)
        (arena / "nested" / "épée.bin").write_bytes(b"arena")
        self.run_update(replacements={"arenas/new": arena, "characters": self.replacement})
        self.assertEqual(update.read_pack(self.destination).entries["arenas/new/nested/épée.bin"], b"arena")

    def test_snapshot_is_immutable(self) -> None:
        pack = update.read_pack(self.base)
        with self.assertRaises(TypeError):
            pack.entries["injected"] = b"bad"
        with self.assertRaises(TypeError):
            pack.inventory["injected"] = None
        with self.assertRaises(FrozenInstanceError):
            pack.source_sha1 = "bad"
        with self.assertRaises(FrozenInstanceError):
            pack.inventory["zero.bin"].size = 10
        self.base.write_bytes(b"changed after read")
        self.assertEqual(pack.entries["canonical/setup.bin"], self.payloads["canonical/setup.bin"])

    def test_empty_pack_is_valid(self) -> None:
        self.base.write_bytes(fixture_blob({}))
        self.assertEqual(dict(update.read_pack(self.base).entries), {})
        self.run_update()
        self.assertEqual(len(update.read_pack(self.destination).entries), 2)

    def test_unsafe_prefixes_reject_without_writes(self) -> None:
        for prefix in ("", "/absolute", "../outside", "a/../b", "a/./b", "a//b", "a/",
                       "C:/asset", "a\\b", "a\0b", "a\nb", "./relative", "a\udcff"):
            with self.subTest(prefix=repr(prefix)):
                self.assert_rejected_without_writes(replacements={prefix: self.replacement})

    def test_overlapping_or_file_conflicting_prefixes_reject(self) -> None:
        for prefixes in ({}, {"characters": self.replacement, "characters/body": self.replacement},
                         {"zero.bin": self.replacement}, {"zero.bin/child": self.replacement}):
            with self.subTest(prefixes=list(prefixes)):
                self.assert_rejected_without_writes(replacements=prefixes)

    def test_existing_output_including_base_is_never_overwritten(self) -> None:
        self.destination.write_bytes(b"preexisting output")
        self.assert_rejected_without_writes()
        self.assert_rejected_without_writes(output=self.base)
        self.assert_rejected_without_writes(output=self.replacement)

    def test_missing_destination_parent_is_not_created(self) -> None:
        parent = self.root / "missing"
        self.assert_rejected_without_writes(output=parent / "output.gepack")
        self.assertFalse(parent.exists())

    def test_filesystem_parent_traversal_is_rejected(self) -> None:
        self.assert_rejected_without_writes(base=self.root / "replacement" / ".." / "base.gepack")
        self.assert_rejected_without_writes(output=self.root / "replacement" / ".." / "output.gepack")
        self.assert_rejected_without_writes(replacements={"characters": self.root / "replacement" / ".." / "replacement"})

    def test_input_file_symlinks_and_ancestor_symlinks_rejected(self) -> None:
        alias_file = self.root / "alias.gepack"
        alias_file.symlink_to(self.base)
        with self.assertRaises(update.GEPackError):
            update.read_pack(alias_file)
        self.assert_rejected_without_writes(base=alias_file)
        alias_root = self.root / "alias"
        alias_root.symlink_to(self.root, target_is_directory=True)
        self.assert_rejected_without_writes(base=alias_root / "base.gepack")
        self.assert_rejected_without_writes(replacements={"characters": alias_root / "replacement"})
        self.assert_rejected_without_writes(output=alias_root / "output.gepack")

    def test_replacement_root_and_descendant_symlinks_rejected(self) -> None:
        alias = self.root / "replacement-alias"
        alias.symlink_to(self.replacement, target_is_directory=True)
        self.assert_rejected_without_writes(replacements={"characters": alias})
        for target in (self.base, self.root, self.root / "missing"):
            link = self.replacement / "link"
            link.symlink_to(target)
            self.assert_rejected_without_writes()
            link.unlink()

    def test_output_symlink_even_dangling_is_rejected(self) -> None:
        for target in (self.base, self.root / "missing"):
            self.destination.symlink_to(target)
            self.assert_rejected_without_writes()
            self.assertTrue(self.destination.is_symlink())
            self.destination.unlink()

    def test_nonregular_inputs_rejected_without_blocking(self) -> None:
        pipe = self.replacement / "fifo"
        os.mkfifo(pipe)
        with self.assertRaises(update.GEPackError):
            self.run_update()
        with self.assertRaises(update.GEPackError):
            update.read_pack(pipe)
        self.assertFalse(self.destination.exists())
        self.assertFalse(list(self.root.glob(".gepack-update-*")))

    def test_replacement_unsafe_filename_rejected(self) -> None:
        for name in ("back\\slash", "control\nchar", "colon:name"):
            path = self.replacement / name
            path.write_bytes(b"unsafe")
            self.assert_rejected_without_writes()
            path.unlink()

    def test_new_entry_fnv_collision_rejected(self) -> None:
        normal = update.fnv1a64
        original_hash = normal(b"canonical/setup.bin")
        def collision(value):
            return original_hash if value == b"characters/head.bin" else normal(value)
        with mock.patch.object(update, "fnv1a64", side_effect=collision):
            self.assert_rejected_without_writes()

    def test_file_change_during_snapshot_is_rejected(self) -> None:
        before = self.base.stat()
        after = SimpleNamespace(**{name: getattr(before, name) for name in
                                  ("st_dev", "st_ino", "st_size", "st_mtime_ns", "st_ctime_ns")})
        after.st_mtime_ns += 1
        with mock.patch.object(update.os, "fstat", side_effect=[before, after]):
            with self.assertRaisesRegex(update.GEPackError, "changed while reading"):
                update.read_pack(self.base)

    def test_creation_race_does_not_overwrite_winner_and_removes_temporary(self) -> None:
        original_link = os.link
        def raced_link(source, destination, **kwargs):
            self.destination.write_bytes(b"race winner")
            return original_link(source, destination, **kwargs)
        with mock.patch.object(update.os, "link", side_effect=raced_link):
            with self.assertRaises(update.GEPackError):
                self.run_update()
        self.assertEqual(self.destination.read_bytes(), b"race winner")
        self.assertFalse(list(self.root.glob(".gepack-update-*")))

    def test_publication_failure_leaves_no_output_or_temporary(self) -> None:
        with mock.patch.object(update.os, "link", side_effect=OSError("injected publication failure")):
            self.assert_rejected_without_writes()
        with mock.patch.object(update.os, "fsync", side_effect=OSError("injected flush failure")):
            self.assert_rejected_without_writes()
        self.assertFalse(list(self.root.glob(".gepack-update-*")))

    def test_malformed_header_section_and_padding_matrix(self) -> None:
        original = self.base.read_bytes()
        cases = []
        def changed(offset, fmt, value):
            blob = bytearray(original)
            struct.pack_into(fmt, blob, offset, value)
            return blob
        cases.extend([
            b"", original[:79], changed(0, "8s", b"INVALID!"), changed(8, "I", 2),
            changed(12, "I", 1), changed(16, "I", 0xFFFFFFFF), changed(20, "I", 79),
            changed(24, "Q", 81), changed(32, "Q", 81), changed(40, "Q", len(original) + 16),
            changed(40, "Q", 81), changed(68, "I", 1), original + b"trailer",
            original[:-1],
        ])
        count = len(self.payloads)
        paths_end = HEADER.unpack_from(original)[6] + sum(len(name.encode()) for name in self.payloads)
        self.assertLess(paths_end, HEADER.unpack_from(original)[7])
        cases.append(changed(paths_end, "B", 1))
        for index in range(count):
            _hash, _po, _pl, offset, size = ENTRY.unpack_from(original, 80 + 32 * index)
            if size % 16:
                cases.append(changed(offset + size, "B", 1))
                break
        for index, blob in enumerate(cases):
            with self.subTest(case=index):
                self.base.write_bytes(blob)
                self.assert_rejected_without_writes()

    def test_malformed_entry_matrix(self) -> None:
        original = self.base.read_bytes()
        cases = []
        for field_offset, fmt, value in ((0, "Q", 0), (8, "I", 1), (12, "I", 0),
                                         (12, "I", 0xFFFFFFFF), (16, "Q", 0),
                                         (16, "Q", len(original) + 16), (24, "Q", 2**64 - 1)):
            blob = bytearray(original)
            struct.pack_into(fmt, blob, 80 + field_offset, value)
            cases.append(blob)
        blob = bytearray(original)
        blob[80:112], blob[112:144] = blob[112:144], blob[80:112]
        cases.append(blob)
        blob = bytearray(original)
        first_offset = ENTRY.unpack_from(blob, 80)[3]
        struct.pack_into("Q", blob, 112 + 16, first_offset)
        cases.append(blob)
        blob = bytearray(original)
        struct.pack_into("I", blob, 112 + 8, 0)
        cases.append(blob)
        for index, blob in enumerate(cases):
            with self.subTest(case=index):
                self.base.write_bytes(blob)
                self.assert_rejected_without_writes()

    def test_unsafe_pack_paths_and_invalid_utf8_rejected(self) -> None:
        for name in ("../bad", "/bad", "a\\b", "a//b", "a/", "a/./b", "a\0b", "C:/bad", "a\nb"):
            with self.subTest(name=repr(name)):
                self.base.write_bytes(fixture_blob({name: b"data"}))
                self.assert_rejected_without_writes()
        blob = fixture_blob({"x": b"data"})
        blob[112] = 0xFF
        struct.pack_into("<Q", blob, 80, reference_fnv(b"\xff"))
        self.base.write_bytes(blob)
        self.assert_rejected_without_writes()

    def test_pack_fnv_collision_rejected_even_with_sorted_distinct_names(self) -> None:
        blob = fixture_blob({"a": b"one", "b": b"two"}, hash_function=lambda _value: 10)
        self.base.write_bytes(blob)
        with mock.patch.object(update, "fnv1a64", return_value=10):
            self.assert_rejected_without_writes()

    def test_index_order_rejected_when_all_other_layout_is_canonical(self) -> None:
        self.base.write_bytes(fixture_blob({"a": b"one", "b": b"two"}, reverse=True))
        with self.assertRaisesRegex(update.GEPackError, "not uniquely sorted"):
            update.read_pack(self.base)

    def test_pack_duplicate_names_rejected(self) -> None:
        blob = fixture_blob({"a": b"one", "b": b"two"})
        first_hash, _po, _pl, _ao, _size = ENTRY.unpack_from(blob, 80)
        blob[145] = blob[144]
        struct.pack_into("<Q", blob, 112, first_hash)
        self.base.write_bytes(blob)
        self.assert_rejected_without_writes()


if __name__ == "__main__":
    unittest.main()
