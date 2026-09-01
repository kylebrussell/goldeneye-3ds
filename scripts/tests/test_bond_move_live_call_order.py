#!/usr/bin/env python3
"""Verify the live boundary enters canonical MoveBond once before publish."""

from pathlib import Path
import re


def body(source: str, name: str) -> str:
    match = re.search(rf"\b{name}\s*\([^;]*?\)\s*\{{", source, re.S)
    assert match is not None
    start = source.index("{", match.start())
    depth = 0
    for index in range(start, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[start:index + 1]
    raise AssertionError(f"unterminated {name}")


repo = Path(__file__).resolve().parents[2]
source = (repo / "port/src/ge_original_bond_live.c").read_text()
tick = body(source, "ge_original_bond_move_live_tick")
calls = [
    tick.index("ge_original_input_read_bond_frame("),
    tick.index("MoveBond("),
    tick.index("ge_port_bond_movement_publish("),
]
assert calls == sorted(calls)
assert tick.count("MoveBond(") == 1
assert "bondviewProcessInput(" not in tick
print("bond live call order: input frame -> unchanged MoveBond -> publication")
