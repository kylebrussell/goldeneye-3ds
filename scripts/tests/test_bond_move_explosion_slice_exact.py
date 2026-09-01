#!/usr/bin/env python3
"""Token-check the exact MoveBond explosion dependency slice."""

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
    path = args.repo / "scripts/extract_bond_move_explosion_slice.py"
    spec = importlib.util.spec_from_file_location("move_explosion_extract", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    generated = module.generate(args.repo)
    checked = 0
    for relative, names in module.DATA.items():
        for name in names:
            canonical = module.extract_data((args.repo / relative).read_text(), name)
            assert tokens(canonical) == tokens(module.extract_data(generated, name)), name
            assert f"{name} sha256={module.digest(canonical)}" in generated
            checked += 1
    for relative, names in module.FUNCTIONS.items():
        for name in names:
            canonical = module.extract_function((args.repo / relative).read_text(), name)
            assert tokens(canonical) == tokens(module.extract_function(generated, name)), name
            assert f"{name} sha256={module.digest(canonical)}" in generated
            checked += 1
    for relative, names in module.TYPEDEFS.items():
        for name in names:
            canonical = module.extract_typedef((args.repo / relative).read_text(), name)
            assert tokens(canonical) == tokens(module.extract_typedef(generated, name)), name
            assert f"{name} sha256={module.digest(canonical)}" in generated
            checked += 1
    assert checked == 112
    print(f"bond move explosion exactness: {checked} canonical bodies/data token-identical")


if __name__ == "__main__":
    main()
