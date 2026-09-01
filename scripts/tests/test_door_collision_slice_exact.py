#!/usr/bin/env python3
"""Token-check every mechanically renamed door-collision body."""

from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def tokens(text: str) -> list[str]:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(r"[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?(?:[eE][+-]?\d+)?[fFlL]?|==|!=|<=|>=|&&|\|\||->|\S", text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    module_path = args.repo / "scripts/extract_door_collision_slice.py"
    spec = importlib.util.spec_from_file_location("door_collision_extract", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    generated = module.generate(args.repo)
    reverse = {value: key for key, value in module.RENAMES.items()}
    checked = 0
    for relative, names in module.FUNCTIONS.items():
        canonical_source = (args.repo / relative).read_text()
        for name in names:
            canonical = module.adapt_native_abi(
                module.extract_function(canonical_source, name))
            renamed = module.extract_function(generated, module.RENAMES[name])
            for replacement in sorted(reverse, key=len, reverse=True):
                renamed = re.sub(rf"\b{re.escape(replacement)}\b",
                                 reverse[replacement], renamed)
            assert tokens(canonical) == tokens(renamed), name
            checked += 1
    print(f"door collision exactness: {checked} canonical bodies token-identical")


if __name__ == "__main__":
    main()
