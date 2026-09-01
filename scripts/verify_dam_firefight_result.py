#!/usr/bin/env python3
"""Validate a completed authored Dam firefight probe result."""

from __future__ import annotations

import argparse
from pathlib import Path


def parse_result(path: Path) -> tuple[dict[str, str], list[str]]:
    values: dict[str, str] = {}
    lines = path.read_text(encoding="utf-8").splitlines()
    for line in lines:
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values, lines


def csv_ints(value: str) -> list[int]:
    return [int(field) for field in value.split(",")]


def guard_death_complete(fields: list[str]) -> bool:
    """Mirror ge_original_stage_guard_snapshot_death_complete telemetry."""
    room_id = int(fields[2])
    action_type = int(fields[3])
    active_linked = int(fields[8])
    return action_type in (4, 5) or (active_linked == 0 and room_id == 255)


def verify_route(values: dict[str, str]) -> None:
    reached, total = csv_ints(values.get("route_targets", "0,0"))
    if reached == total == 11:
        return

    simulation_frames = int(values.get("simulation_frames", "0"), 0)
    frame_budget = int(values.get("frames", "0"), 0)
    displacement = float(values.get("displacement", "0"))
    target_trace = values.get("target_trace", "")
    blocker = values.get("movement_blocker", "missing")
    raise AssertionError(
        "authored route incomplete: "
        f"{reached}/{total} targets after {simulation_frames} simulation ticks "
        f"({frame_budget} displayed frames), displacement={displacement:.3f}, "
        f"movement_blocker={blocker}, target_trace={target_trace}. "
        "A progressing trace with no movement blocker exhausted the harness "
        "budget; a stationary tail or populated blocker indicates a movement/"
        "STAN regression."
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("result", type=Path)
    args = parser.parse_args()
    values, lines = parse_result(args.result)

    verify_route(values)
    assert values.get("status") == "complete", values.get("status")

    pp7 = csv_ints(values["pp7"])
    assert pp7[0] > 0, pp7
    assert pp7[1] == pp7[0], pp7
    assert pp7[3] > 0 and pp7[4] > 0, pp7

    hits = values["guard_hit_test"].split(",")
    assert int(hits[2]) > 0 and int(hits[3]) > 0, hits

    opening_guards = [
        next(line for line in lines if line.startswith(f"guard={slot},{chr_id},"))
        .split("=", 1)[1].split(",")
        for slot, chr_id in ((24, 6), (25, 7))
    ]
    dead_guards = [guard for guard in opening_guards
                   if guard_death_complete(guard)]
    assert dead_guards, opening_guards
    assert int(hits[4]) in (24, 25), hits

    guard_combat = values["guard_combat"].split(",")
    assert int(guard_combat[0]) > 0, guard_combat
    assert int(guard_combat[2]) > 0, guard_combat

    unknown = values["guard_ai_unknown"].split(",")
    assert int(unknown[0]) == 0, unknown

    player = values["player_combat"].split(",")
    assert float(player[2]) == 1.0 and float(player[3]) == 1.0, player
    assert float(player[0]) < 1.0 or int(player[5]) == 1, player
    assert int(player[6]) == 0, player

    print(
        "Dam authored firefight verified: "
        f"{pp7[0]} PP7 shots, {pp7[4]} damaging guard hits, "
        f"{guard_combat[0]} guard fire dispatches, "
        f"{len(dead_guards)} original opening-guard death(s) and "
        "player damage, zero unresolved AI opcodes"
    )


if __name__ == "__main__":
    main()
