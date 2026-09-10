#!/usr/bin/env python3
"""Independent canonical byte oracle and hashlib comparison (no native structs)."""
import hashlib
import struct
import sys
from pathlib import Path


def be(fmt, *values):
    return struct.pack(">" + fmt, *values)


def main():
    keys = [0x01020001 + i for i in range(4)]
    ids = [(key << 32) | 1 for key in keys[:2]]
    schemas = [(1, 11, 0, 2, 2), (2, 12, 0, 2, 2),
               (3, 11, 0, 2, 2), (4, 11, 0, 2, 2)]
    schemas += [(256 + i, 1 + i, int(i >= 10), 1, 1) for i in range(12)]
    actor_schema = [(5, 1, 0, 1, 1), (256, 9, 0, 3, 3)]
    expected = b"GEMS" + be("HIIQQ", 1, 7, 0x12345678, 0x0102030405060708, 0x2003)
    expected += bytes(range(96)) + be("QI", 0x2003, 2)
    for kind, systems, fields in [(0, 0x2001, schemas), (1, 2, actor_schema)]:
        expected += be("BQI", kind, systems, len(fields))
        expected += b"".join(be("HBBII", *field) for field in fields)
    expected += be("I", 4)
    for i, key in enumerate(keys):
        expected += be("IIB", key, int(i < 2), int(i < 2))
    expected += be("I", 3)
    values = [be("QQ", *ids), be("II", *keys[2:]), be("QQ", *ids), be("QQ", *ids),
              be("B", 0xAB), be("H", 0xABCD), be("I", 0x89ABCDEF),
              be("Q", 0xFEDCBA9876543210), be("b", -128), be("h", -32768),
              be("i", -(2 ** 31)), be("q", -(2 ** 63)), be("f", -0.25),
              be("B", 1), be("Q", ids[1]), be("I", keys[3])]
    expected += be("QBI", 0, 0, len(schemas))
    for schema, value in zip(schemas, values):
        expected += be("HBI", schema[0], schema[1], schema[3]) + value
    for i, actor_id in enumerate(ids):
        expected += be("QBI", actor_id, 1, 2)
        expected += be("HBIB", 5, 1, 1, i)
        expected += be("HBIfff", 256, 9, 3, i + 1.0, -2.0, 0.0)
    actual = Path(sys.argv[1]).read_bytes()
    assert actual[:-32] == expected, "canonical bytes differ from independent big-endian schema oracle"
    assert actual[-32:] == hashlib.sha256(expected).digest(), "streaming state SHA256 differs from hashlib"
    print(f"mp state wire: {len(expected)} canonical bytes match independent Python oracle and hashlib")


if __name__ == "__main__":
    main()
