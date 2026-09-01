#!/usr/bin/env python3
"""Pin the unresolved calls made directly by canonical bondviewProcessInput."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path


EXPECTED: set[str] = set()


def symbols(path: Path) -> tuple[set[str], set[str]]:
    output = subprocess.run(
        ["nm", "-g", str(path)], check=True, capture_output=True, text=True
    ).stdout
    defined: set[str] = set()
    undefined: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        if len(fields) < 2:
            continue
        kind = fields[-2]
        name = fields[-1]
        if name.startswith("_"):
            name = name[1:]
        if kind.upper() == "U":
            undefined.add(name)
        elif kind.upper() in {"B", "C", "D", "G", "R", "S", "T", "V", "W"}:
            defined.add(name)
    return defined, undefined


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--linked", type=Path)
    parser.add_argument("body", type=Path)
    parser.add_argument("definitions", nargs="+", type=Path)
    args = parser.parse_args()

    _, body_undefined = symbols(args.body)
    definitions: set[str] = set()
    for path in args.definitions:
        defined, _ = symbols(path)
        definitions.update(defined)
    frontier = body_undefined - definitions
    assert frontier == EXPECTED, (
        f"canonical input frontier changed: {sorted(frontier)}"
    )
    print(
        "bond input direct linker frontier: "
        + (", ".join(sorted(frontier)) if frontier else "closed")
    )
    if args.linked is not None:
        _, transitive = symbols(args.linked)
        print(
            f"bond input transitive linker frontier ({len(transitive)}): "
            + ", ".join(sorted(transitive))
        )


if __name__ == "__main__":
    main()
