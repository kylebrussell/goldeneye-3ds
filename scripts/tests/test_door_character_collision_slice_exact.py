#!/usr/bin/env python3
"""Token-check the exact ChrRecord door-collision leaves."""

from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def tokens(text: str) -> list[str]:
    text = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    return re.findall(r"[A-Za-z_]\w*|0[xX][0-9A-Fa-f]+|\d+(?:\.\d*)?[fFlL]?|==|!=|&&|\|\||->|\S", text)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    path = args.repo / "scripts/extract_door_character_collision_slice.py"
    spec = importlib.util.spec_from_file_location("door_chr_extract", path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    canonical_source = (args.repo / "src/game/chr.c").read_text()
    generated = module.generate(args.repo)
    for name in module.FUNCTIONS:
        canonical = module.extract_function(canonical_source, name)
        renamed = module.extract_function(generated, module.RENAMES[name])
        renamed = re.sub(rf"\b{module.RENAMES[name]}\b", name, renamed)
        assert tokens(canonical) == tokens(renamed), name
    print(f"door character collision exactness: {len(module.FUNCTIONS)} canonical bodies token-identical")


if __name__ == "__main__":
    main()
