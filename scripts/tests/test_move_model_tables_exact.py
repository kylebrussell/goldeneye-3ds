#!/usr/bin/env python3
"""Verify the MoveBond model table generator tracks exact authored manifests."""

from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", type=Path)
    args = parser.parse_args()
    module_path = args.repo / "scripts/generate_move_model_tables.py"
    spec = importlib.util.spec_from_file_location("move_model_tables", module_path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    generated = module.generate(args.repo)
    assert generated.count(module.GUARD_HEADER_INCLUDE) == 0
    assert generated.count(module.BUG_HEADER_INCLUDE) == 0
    assert "greatguard2_header is supplied by the exact relocated Dam guard model" in generated
    assert "chrbug_header is supplied by the exact relocated bug model" in generated
    for relative in (
        "assets/obseg/prop/propModelFileHeaders.inc.c",
        "assets/embedded/skeletons/props.inc.c",
        "assets/embedded/skeletons/suit_lf_hand.inc.c",
        "assets/obseg/prop/propItemModelFileRecord.inc.c",
        "assets/obseg/chr/chrModelFileHeaders.inc.c",
        "assets/obseg/chr/chrModelFileRecords.inc.c",
        "assets/obseg/chr/chrHeadHats.inc.c",
    ):
        assert f"{relative} sha256={module.digest(args.repo / relative)}" in generated
    print("MoveBond model tables: 7 canonical manifests hash-pinned")


if __name__ == "__main__":
    main()
