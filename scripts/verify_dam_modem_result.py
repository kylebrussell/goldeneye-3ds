#!/usr/bin/env python3
"""Verify a live input-probe captured the canonical Dam modem objective."""

from __future__ import annotations

import argparse
from pathlib import Path


MODEM_ATTACHED_OBJECTIVE_BIT = 0x00000100


def fields(path: Path) -> dict[str, str]:
    values: dict[str, str] = {}
    for line in path.read_text().splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
    return values


def comma_ints(values: dict[str, str], key: str, count: int) -> list[int]:
    try:
        result = [int(value, 0) for value in values[key].split(",")]
    except (KeyError, ValueError) as error:
        raise ValueError(f"missing or invalid {key}") from error
    if len(result) != count:
        raise ValueError(f"{key} must contain {count} values")
    return result


def verify(path: Path) -> str:
    values = fields(path)
    if values.get("status") != "complete":
        raise ValueError("input route did not complete")
    reached, total = comma_ints(values, "route_targets", 2)
    if reached != total or total < 4:
        raise ValueError("modem route did not reach every target")
    attempts, throws, pose_rejections = comma_ints(values, "modem", 3)
    if attempts < 1 or throws < 1:
        raise ValueError("canonical gun path did not throw the covert modem")
    if pose_rejections != 0:
        raise ValueError("covert-modem throw used an invalid first-person pose")
    try:
        objective_registers = int(values["mission_objectives"], 0)
    except (KeyError, ValueError) as error:
        raise ValueError("missing canonical mission objective snapshot") from error
    if objective_registers & MODEM_ATTACHED_OBJECTIVE_BIT == 0:
        raise ValueError("ai_20 did not observe the modem attached to tag 5")
    return (f"Dam modem objective verified: {throws} throw, "
            f"objective registers 0x{objective_registers:08x}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("result", type=Path)
    args = parser.parse_args()
    try:
        print(verify(args.result))
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
