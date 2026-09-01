#!/usr/bin/env python3
"""Generate a controller-only Dam route that attempts the authored modem objective."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


ACTION_FIRE = 1 << 0
ACTION_USE = 1 << 1
ACTION_NEXT_WEAPON = 1 << 5
MAX_TARGETS = 160


def target_line(x: float, z: float, radius: float, held: int,
                dwell: int, pulse_period: int = 0) -> str:
    return (f"target {x:.6f} {z:.6f} {radius:.6f} "
            f"{held} {dwell} 0.0 {pulse_period}")


def build_route(document: dict[str, object], frames: int,
                radius: float) -> list[str]:
    views = document.get("views")
    landmarks = document.get("mission_landmarks")
    if not isinstance(views, list) or not views:
        raise ValueError("authored modem manifest has no route views")
    if not isinstance(landmarks, dict):
        raise ValueError("authored modem manifest has no mission landmarks")
    modem = landmarks.get("modem")
    gates = landmarks.get("gates")
    if not isinstance(modem, list) or len(modem) != 2:
        raise ValueError("authored modem manifest must identify both tagged props")
    if not isinstance(gates, list) or len(gates) != 2:
        raise ValueError("authored modem manifest must identify both Dam gates")
    level_scale = float(document.get("level_scale", 0.0))
    if not math.isfinite(level_scale) or level_scale <= 0.0:
        raise ValueError("authored modem manifest has an invalid level scale")
    monitor = modem[0]
    if not isinstance(monitor, dict) or monitor.get("prop") != 290 \
            or monitor.get("bound_pad") != 57:
        raise ValueError("authored tag-5 monitor identity does not match Dam setup")
    monitor_raw = monitor.get("position_raw")
    if not isinstance(monitor_raw, list) or len(monitor_raw) != 3:
        raise ValueError("authored tag-5 monitor position is unavailable")
    monitor_x = float(monitor_raw[0]) / level_scale
    monitor_z = float(monitor_raw[2]) / level_scale
    gate_positions: list[tuple[float, float]] = []
    for expected_prop, expected_pad, gate in zip((267, 268), (6, 9), gates):
        if not isinstance(gate, dict) or gate.get("prop") != expected_prop \
                or gate.get("bound_pad") != expected_pad:
            raise ValueError("authored Dam gate identity does not match setup")
        raw = gate.get("position_raw")
        if not isinstance(raw, list) or len(raw) != 3:
            raise ValueError("authored Dam gate position is unavailable")
        gate_positions.append((float(raw[0]) / level_scale,
                               float(raw[2]) / level_scale))
    gate_dx = gate_positions[1][0] - gate_positions[0][0]
    gate_dz = gate_positions[1][1] - gate_positions[0][1]
    gate_length = math.hypot(gate_dx, gate_dz)
    if gate_length <= 0.0:
        raise ValueError("authored Dam gates are coincident")
    gate_dx, gate_dz = gate_dx / gate_length, gate_dz / gate_length
    interaction_radius = min(radius, 100.0)

    lines: list[str] = []
    for view in views:
        if not isinstance(view, dict):
            raise ValueError("authored modem route view is invalid")
        position = view.get("position_runtime")
        if not isinstance(position, list) or len(position) != 3:
            raise ValueError("authored modem route view has no runtime position")
        x, z = float(position[0]), float(position[2])
        if not math.isfinite(x) or not math.isfinite(z):
            raise ValueError("authored modem route contains a non-finite position")
        # Navigation never carries an action. In particular, pulsing Use while
        # still at the insertion point enters Dam's nearby vehicle branch.
        lines.append(target_line(x, z, radius, 0, 0))
        if view.get("pad") == 116:
            first_x, first_z = gate_positions[0]
            second_x, second_z = gate_positions[1]
            lines.extend([
                # Cross the authored initially-open first gate, turn back and
                # close it, then open/cross the interlocked second gate.
                target_line(first_x + gate_dx * 150.0,
                            first_z + gate_dz * 150.0, interaction_radius, 0, 0),
                target_line(first_x, first_z, interaction_radius, 0, 0),
                target_line(first_x, first_z, interaction_radius,
                            ACTION_USE, 1),
                target_line(first_x, first_z, interaction_radius, 0, 180),
                target_line(second_x, second_z, interaction_radius, 0, 0),
                target_line(second_x, second_z, interaction_radius,
                            ACTION_USE, 1),
                target_line(second_x, second_z, interaction_radius, 0, 180),
                target_line(second_x + gate_dx * 150.0,
                            second_z + gate_dz * 150.0,
                            interaction_radius, 0, 0),
            ])

    # Continue toward the exact tag-5 monitor pad. Each duplicate target is a
    # normal controller phase: one A press, the canonical weapon-change dwell,
    # held Z long enough for the original mine-place animation, then a settle
    # window for exact projectile/object and background-AI ticks.
    lines.extend([
        target_line(monitor_x, monitor_z, radius, 0, 0),
        target_line(monitor_x, monitor_z, radius, ACTION_NEXT_WEAPON, 1),
        target_line(monitor_x, monitor_z, radius, 0, 90),
        target_line(monitor_x, monitor_z, radius, ACTION_FIRE, 120),
        target_line(monitor_x, monitor_z, radius, 0, 180),
    ])
    if len(lines) > MAX_TARGETS:
        raise ValueError("authored modem controller route exceeds runtime capacity")
    return ["GE_INPUT_PROBE 6", f"frames {frames}",
            f"targets {len(lines)}", *lines]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frames", type=int, default=6000)
    parser.add_argument("--radius", type=float, default=135.0)
    args = parser.parse_args()
    if not 1 <= args.frames <= 30000:
        raise SystemExit("frames must be in 1..30000")
    if not math.isfinite(args.radius) or args.radius <= 0.0:
        raise SystemExit("radius must be positive")
    try:
        document = json.loads(args.manifest.read_text())
        lines = build_route(document, args.frames, args.radius)
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
        raise SystemExit(str(error)) from error
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n")
    print(f"generated {len(lines) - 3} target Dam modem route -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
