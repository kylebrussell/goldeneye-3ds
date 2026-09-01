#!/usr/bin/env python3
"""Token-check the canonical MoveBond collision/room slice."""

from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def tokens(text: str) -> list[str]:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(
        r"[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFlL]?|==|!=|<=|>=|&&|\|\||->|\S",
        text,
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    module_path = args.repo / "scripts/extract_bond_move_collision_slice.py"
    spec = importlib.util.spec_from_file_location("move_collision_extract", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    generated = module.generate(args.repo)

    for relative, name in module.DATA:
        canonical = module.extract_data((args.repo / relative).read_text(), name)
        assert canonical in generated
        assert f"{name} sha256={module.digest(canonical)}" in generated

    checked = 0
    for relative, name in module.BODY_ORDER:
        canonical = module.extract_function((args.repo / relative).read_text(), name)
        emitted = module.extract_function(generated, name)
        assert tokens(module.adapt_native_abi(canonical)) == tokens(emitted), name
        assert f"{name} sha256={module.digest(canonical)}" in generated
        checked += 1

    assert checked == 30
    print(f"bond move collision exactness: {checked} canonical bodies token-identical")


if __name__ == "__main__":
    main()
