#!/usr/bin/env python3
"""Check that the generated animation ABI covers every canonical data array."""

from __future__ import annotations

import argparse
import importlib.util
import re
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()

    module_path = args.repo / "scripts/generate_animation_table_abi.py"
    spec = importlib.util.spec_from_file_location("animation_table_abi", module_path)
    assert spec and spec.loader
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    source = args.repo / "assets/animationtable_data.c"
    canonical = module.ARRAY_RE.findall(source.read_text())
    generated = module.render(source)
    renamed = re.findall(r"^#define (ANIM_DATA_\w+) ge_port_storage_", generated, re.MULTILINE)
    absolute = re.findall(r'^__asm__\("\.global (ANIM_DATA_\w+)', generated, re.MULTILINE)
    assert canonical
    assert renamed == canonical
    assert absolute == canonical
    assert '#include <assets/animationtable_data.c>' in generated
    assert "assets/animationtable_data.h" not in generated
    print(f"animation table ABI exactness: {len(canonical)} authored arrays")


if __name__ == "__main__":
    main()
