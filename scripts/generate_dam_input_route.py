#!/usr/bin/env python3
"""Build a diagnostic exact-input route from authored Dam waypoints.

The generated file is consumed only when explicitly installed as
dam-input-probe.cfg. The runtime steers through normal controller samples;
it never teleports or mutates player/collision state.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frames", type=int, default=3600)
    parser.add_argument("--radius", type=float, default=120.0)
    parser.add_argument("--limit", type=int)
    parser.add_argument("--held", type=int, default=0,
                        help="portable action bitmask held along the route")
    parser.add_argument("--final-dwell", type=int, default=0,
                        help="frames to hold the final target action in place")
    parser.add_argument("--pulse-period", type=int, default=0,
                        help="pulse the target action every N frames (0 holds it)")
    args = parser.parse_args()
    document = json.loads(args.manifest.read_text())
    views = document.get("views", [])
    if args.limit is not None:
        views = views[: args.limit]
    if not views or len(views) > 160:
        raise SystemExit("route must contain 1..160 authored views")
    if not 1 <= args.frames <= 30000:
        raise SystemExit("frames must be in 1..30000")
    if args.radius <= 0.0:
        raise SystemExit("radius must be positive")
    if args.final_dwell < 0 or args.final_dwell > args.frames:
        raise SystemExit("final dwell must be within the route frame budget")
    if args.pulse_period < 0 or args.pulse_period > args.frames:
        raise SystemExit("pulse period must be within the route frame budget")
    version = 6 if args.pulse_period else 4 if args.final_dwell else 3
    lines = [f"GE_INPUT_PROBE {version}", f"frames {args.frames}",
             f"targets {len(views)}"]
    for index, view in enumerate(views):
        x, _y, z = view["position_runtime"]
        line = (f"target {float(x):.6f} {float(z):.6f} "
                f"{args.radius:.6f} {args.held}")
        if version >= 4:
            line += f" {args.final_dwell if index + 1 == len(views) else 0}"
        if version >= 5:
            line += " 0.0"
        if version >= 6:
            line += f" {args.pulse_period}"
        lines.append(line)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n")
    print(f"generated {len(views)} authored exact-input targets -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
