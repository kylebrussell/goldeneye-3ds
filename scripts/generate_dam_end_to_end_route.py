#!/usr/bin/env python3
"""Generate a controller-only route through Dam's four authored objectives."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path


ACTION_FIRE = 1 << 0
ACTION_USE = 1 << 1
ACTION_NEXT_WEAPON = 1 << 5
MAX_TARGETS = 160
OPENING_ARMOUR_PROP = 318
OPENING_ARMOUR_PAD = 113
OPENING_ARMOUR_PICKUP_RADIUS = 60.0
DEFENSIVE_FIRE_SEGMENT = "modem-approach"
DEFENSIVE_FIRE_PERIOD = 45
GUARD_APPROACH_DISTANCE = 1800.0
GUARD_GROUP_ENGAGEMENT_FRACTIONS = (0.4, 0.8)
GUARD_SINGLE_ENGAGEMENT_FRACTIONS = (0.65, 0.95)
GUARD_ENGAGEMENT_RADIUS = 250.0
GUARD_ENGAGEMENT_DWELL = 300
ROAD_GUARD_IDS = (4, 5)
# Engage the two exposed road guards from the incoming authored pad287 cover
# before crossing their firing lane. Both existing stops remain controller-
# only; a zero edge fraction means the already-reached waypoint is reused.
ROAD_GUARD_ENGAGEMENT_FRACTIONS = (0.0, 0.0)
BACKUP_TERMINAL_TAGS = (6, 7)
BACKUP_TERMINAL_PROPS = (262, 264)
ALARM_TAGS = (0, 1, 2, 3)
ALARM_PROPS = (310, 312, 314, 316)
# ai_24's authored fallback is 350 ticks, followed by its 60-tick black fade
# and three yielded frames before the POSEND/title-input state.  The route
# waits through that exact sequence before supplying the player's fresh edge.
EXIT_SETTLE_FRAMES = 350 + 60 + 3 + 7


def target(x: float, z: float, radius: float, held: int = 0,
           dwell: int = 0, look_y: float = 0.0,
           pulse_period: int = 0, aim_chr: int = -1) -> str:
    return (f"target {x:.6f} {z:.6f} {radius:.6f} {held} {dwell} "
            f"{look_y:.6f} {pulse_period} {aim_chr}")


def runtime_position(landmark: dict[str, object],
                     level_scale: float) -> tuple[float, float]:
    raw = landmark.get("position_raw")
    if not isinstance(raw, list) or len(raw) != 3:
        raise ValueError("mission landmark has no authored position")
    x, z = float(raw[0]) / level_scale, float(raw[2]) / level_scale
    if not math.isfinite(x) or not math.isfinite(z):
        raise ValueError("mission landmark position is not finite")
    return x, z


def authored_armour(document: dict[str, object]) -> dict[str, object]:
    """Return Dam's exact full-armour setup record and route metadata."""
    landmarks = document.get("mission_landmarks")
    level_scale = float(document.get("level_scale", 0.0))
    armour = landmarks.get("armour") if isinstance(landmarks, dict) else None
    if not isinstance(armour, list) or len(armour) != 2:
        raise ValueError("authored Dam armour landmarks are unavailable")
    pickup = next((entry for entry in armour
                   if isinstance(entry, dict)
                   and entry.get("prop") == OPENING_ARMOUR_PROP), None)
    if pickup is None or pickup.get("pad") != OPENING_ARMOUR_PAD \
            or pickup.get("initial_amount_fixed") != 65536:
        raise ValueError("authored full Armour prop 318/pad 113 is unavailable")
    return pickup


def armour_detour_targets(document: dict[str, object],
                          pickup: dict[str, object]) -> tuple[int, list[str]]:
    """Build an out-and-back controller detour on the authored waypoint graph."""
    level_scale = float(document.get("level_scale", 0.0))
    detour = pickup.get("route_detour")
    if not isinstance(detour, dict) \
            or not isinstance(detour.get("branch_waypoint"), int) \
            or not isinstance(detour.get("approach"), list):
        raise ValueError("authored full Armour route detour is unavailable")
    approach = detour["approach"]
    if not approach:
        raise ValueError("authored full Armour route has no approach")
    for entry in approach:
        if not isinstance(entry, dict):
            raise ValueError("authored full Armour approach is invalid")
        runtime_position(entry, level_scale)
    armour_x, armour_z = runtime_position(pickup, level_scale)
    # The exact branch-to-pickup links (45->202->190) are collinear in X/Z,
    # so the pickup target itself traverses them. The caller then emits the
    # already-authored branch view once more to return to the main route.
    return int(detour["branch_waypoint"]), [
        target(armour_x, armour_z, OPENING_ARMOUR_PICKUP_RADIUS)]


def build_route(document: dict[str, object], frames: int,
                radius: float) -> list[str]:
    views = document.get("views")
    landmarks = document.get("mission_landmarks")
    level_scale = float(document.get("level_scale", 0.0))
    if not isinstance(views, list) or not views:
        raise ValueError("authored objective manifest has no route views")
    if not isinstance(landmarks, dict) or level_scale <= 0.0:
        raise ValueError("authored objective manifest has invalid landmarks")
    modem = landmarks.get("modem")
    guards = landmarks.get("guards")
    terminals = landmarks.get("backup_terminals")
    alarms = landmarks.get("alarms")
    gates = landmarks.get("gates")
    bungee = landmarks.get("bungee")
    armour = authored_armour(document)
    armour_branch, armour_targets = armour_detour_targets(document, armour)
    armour_inserted = False
    if not isinstance(modem, list) or len(modem) != 2:
        raise ValueError("authored modem landmarks are unavailable")
    if not isinstance(guards, list) or len(guards) != 36:
        raise ValueError("authored Dam guard landmarks are unavailable")
    if not isinstance(terminals, list) or len(terminals) != 2:
        raise ValueError("authored backup-terminal landmarks are unavailable")
    if not isinstance(alarms, list) or len(alarms) != 4:
        raise ValueError("authored alarm landmarks are unavailable")
    if not isinstance(gates, list) or len(gates) != 2:
        raise ValueError("authored Dam gate landmarks are unavailable")
    if not isinstance(bungee, dict) or bungee.get("pad") != 330:
        raise ValueError("authored Dam bungee exit pad is unavailable")
    if tuple(entry.get("tag") for entry in terminals) != BACKUP_TERMINAL_TAGS:
        raise ValueError("authored backup-terminal tags 6/7 are unavailable")
    if tuple(entry.get("prop") for entry in terminals) != BACKUP_TERMINAL_PROPS:
        raise ValueError("authored backup-terminal props 262/264 are unavailable")
    if tuple(entry.get("tag") for entry in alarms) != ALARM_TAGS:
        raise ValueError("authored alarm tags 0-3 are unavailable")
    if tuple(entry.get("prop") for entry in alarms) != ALARM_PROPS:
        raise ValueError("authored alarm props 310/312/314/316 are unavailable")
    modem_x, modem_z = runtime_position(modem[0], level_scale)
    guard_positions: dict[int, tuple[float, float]] = {}
    for guard in guards:
        if not isinstance(guard, dict) or not isinstance(guard.get("chr"), int):
            raise ValueError("authored Dam guard identity is unavailable")
        chr_num = int(guard["chr"])
        if chr_num in guard_positions:
            raise ValueError("authored Dam guard identities are not unique")
        guard_positions[chr_num] = runtime_position(guard, level_scale)
    terminal_positions = [runtime_position(entry, level_scale)
                          for entry in terminals]
    alarm_positions = [runtime_position(entry, level_scale)
                       for entry in alarms]
    bungee_x, bungee_z = runtime_position(bungee, level_scale)
    gate_positions: list[tuple[float, float]] = []
    for expected_prop, expected_pad, gate in zip((267, 268), (6, 9), gates):
        if not isinstance(gate, dict) or gate.get("prop") != expected_prop \
                or gate.get("bound_pad") != expected_pad:
            raise ValueError("authored Dam gate identity does not match setup")
        gate_positions.append(runtime_position(gate, level_scale))
    gate_dx = gate_positions[1][0] - gate_positions[0][0]
    gate_dz = gate_positions[1][1] - gate_positions[0][1]
    gate_length = math.hypot(gate_dx, gate_dz)
    if gate_length <= 0.0:
        raise ValueError("authored Dam gates are coincident")
    gate_dx, gate_dz = gate_dx / gate_length, gate_dz / gate_length
    interaction_radius = min(radius, 100.0)
    # build_authored_route visits alarm 4 first, then alarms 1, 2 and 3.
    alarm_by_segment = dict(zip(
        ("alarm-1", "alarm-2", "alarm-3", "alarm-4"),
        (alarm_positions[3], alarm_positions[0],
         alarm_positions[1], alarm_positions[2]),
    ))

    result: list[str] = []
    previous_segment: str | None = None
    approached_guards: set[int] = set()

    def finish_segment(segment: str | None) -> None:
        if segment == "modem-approach":
            result.extend([
                target(modem_x, modem_z, radius),
                target(modem_x, modem_z, radius, ACTION_NEXT_WEAPON, 1),
                target(modem_x, modem_z, radius, 0, 90),
                target(modem_x, modem_z, radius, ACTION_FIRE, 150, -0.65),
                target(modem_x, modem_z, radius, 0, 240),
            ])
            for terminal_x, terminal_z in terminal_positions:
                result.extend([
                    target(terminal_x, terminal_z, radius),
                    target(terminal_x, terminal_z, radius,
                           ACTION_USE, 1),
                    target(terminal_x, terminal_z, radius, 0, 30),
                ])
            # The unchanged ai_21 owns the ten-second backup countdown.
            last_x, last_z = terminal_positions[-1]
            result.append(target(last_x, last_z, radius, 0, 720))
        elif segment in alarm_by_segment:
            alarm_x, alarm_z = alarm_by_segment[segment]
            result.extend([
                # Objective 0 is four ObjectiveDestroyObject criteria, not an
                # activation test.  Approach and shoot the exact tagged alarm
                # with normal fire; do not toggle or mutate it through a
                # diagnostic service.
                target(alarm_x, alarm_z, radius),
                # The silenced PP7 is semi-automatic: a continuously held Z
                # supplies only one trigger edge.  Pulse normal controller
                # input so a missed alarm shot can be retried without any
                # object-state or objective mutation by the probe.
                target(alarm_x, alarm_z, radius, ACTION_FIRE, 180,
                       pulse_period=DEFENSIVE_FIRE_PERIOD),
                target(alarm_x, alarm_z, radius, 0, 45),
            ])
        elif segment == "bungee-exit":
            result.extend([
                # ai_24, not the route, sets the bungee objective bit, locks
                # movement, runs the fall/fade/camera sequence and arms the
                # exit. The route supplies only the subsequent player input.
                target(bungee_x, bungee_z, radius, 0,
                       EXIT_SETTLE_FRAMES),
                target(bungee_x, bungee_z, radius, ACTION_FIRE, 1),
            ])

    for view_index, view in enumerate(views):
        if not isinstance(view, dict):
            raise ValueError("authored objective route view is invalid")
        segment = view.get("route_segment")
        if not isinstance(segment, str):
            raise ValueError("authored objective route segment is missing")
        if previous_segment is not None and segment != previous_segment:
            finish_segment(previous_segment)
        position = view.get("position_runtime")
        if not isinstance(position, list) or len(position) != 3:
            raise ValueError("authored objective route position is missing")
        x, z = float(position[0]), float(position[2])
        nearby_guards = {
            chr_num for chr_num, (guard_x, guard_z) in guard_positions.items()
            if math.hypot(x - guard_x, z - guard_z) <= GUARD_APPROACH_DISTANCE
        }
        newly_approached = nearby_guards - approached_guards
        if newly_approached and view_index != 0:
            previous_position = views[view_index - 1].get("position_runtime")
            if not isinstance(previous_position, list) \
                    or len(previous_position) != 3:
                raise ValueError("guard engagement edge is unavailable")
            previous_x = float(previous_position[0])
            previous_z = float(previous_position[2])
            # Stop on the incoming authored route edge before entering each
            # new setup guard group's firing range. The probe supplies only
            # controller input; original auto-aim, fire, damage, AI and death
            # systems own the outcome and no actor state is edited.
            aim_chrs = sorted(newly_approached)
            if tuple(aim_chrs) == ROAD_GUARD_IDS:
                fractions = ROAD_GUARD_ENGAGEMENT_FRACTIONS
                aim_chrs.reverse()
            else:
                fractions = (GUARD_GROUP_ENGAGEMENT_FRACTIONS
                             if len(newly_approached) > 1
                             else GUARD_SINGLE_ENGAGEMENT_FRACTIONS)
            for engagement_index, fraction in enumerate(fractions):
                aim_chr = aim_chrs[min(engagement_index,
                                       len(aim_chrs) - 1)]
                result.append(target(
                    previous_x + (x - previous_x) * fraction,
                    previous_z + (z - previous_z) * fraction,
                    GUARD_ENGAGEMENT_RADIUS,
                    ACTION_FIRE, GUARD_ENGAGEMENT_DWELL,
                    pulse_period=DEFENSIVE_FIRE_PERIOD,
                    aim_chr=aim_chr))
        approached_guards.update(nearby_guards)
        # The controller-only route traverses the same hostile tunnel used by
        # the authored game. Sparse trigger edges keep the silenced PP7 on its
        # unchanged semi-automatic input/fire/damage path; without defensive
        # input, the live guards correctly kill the passive probe before its
        # ninth waypoint.
        result.append(target(
            x, z, radius,
            ACTION_FIRE if segment == DEFENSIVE_FIRE_SEGMENT else 0,
            0, 0.0,
            DEFENSIVE_FIRE_PERIOD
                if segment == DEFENSIVE_FIRE_SEGMENT else 0))
        if view.get("waypoint") == armour_branch:
            if armour_inserted:
                raise ValueError("authored full Armour detour branch is repeated")
            result.extend(armour_targets)
            result.append(target(x, z, radius))
            armour_inserted = True
        if view.get("pad") == 116:
            first_x, first_z = gate_positions[0]
            second_x, second_z = gate_positions[1]
            result.extend([
                target(first_x + gate_dx * 150.0,
                       first_z + gate_dz * 150.0,
                       interaction_radius),
                target(first_x, first_z, interaction_radius),
                target(first_x, first_z, interaction_radius,
                       ACTION_USE, 1),
                target(first_x, first_z, interaction_radius, 0, 180),
                target(second_x, second_z, interaction_radius),
                target(second_x, second_z, interaction_radius,
                       ACTION_USE, 1),
                target(second_x, second_z, interaction_radius, 0, 180),
                target(second_x + gate_dx * 150.0,
                       second_z + gate_dz * 150.0,
                       interaction_radius),
            ])
        previous_segment = segment
    finish_segment(previous_segment)
    if not armour_inserted:
        raise ValueError("authored full Armour detour branch was not traversed")
    if len(result) > MAX_TARGETS:
        raise ValueError("authored end-to-end route exceeds runtime capacity")
    return ["GE_INPUT_PROBE 7", f"frames {frames}",
            f"targets {len(result)}", *result]


def prepend_controller_route(lines: list[str], path: Path,
                             skip_route_targets: int = 0,
                             prelude_targets: list[str] | None = None) \
        -> list[str]:
    """Prefix another version-6 controller route without changing gameplay."""
    opening = path.read_text().splitlines()
    if len(opening) < 3 or opening[0] not in {
            "GE_INPUT_PROBE 6", "GE_INPUT_PROBE 7"} \
            or not opening[1].startswith("frames ") \
            or not opening[2].startswith("targets "):
        raise ValueError("opening controller route is not GE_INPUT_PROBE 6/7")
    try:
        opening_count = int(opening[2].split()[1])
        route_count = int(lines[2].split()[1])
    except (IndexError, ValueError) as error:
        raise ValueError("opening controller route has invalid counts") from error
    opening_targets = opening[3:]
    if opening[0] == "GE_INPUT_PROBE 6":
        opening_targets = [f"{entry} -1" for entry in opening_targets]
    route_targets = lines[3:]
    if opening_count != len(opening_targets) \
            or route_count != len(route_targets) \
            or any(not entry.startswith("target ")
                   for entry in (*opening_targets, *route_targets)):
        raise ValueError("opening controller route target count diverged")
    if skip_route_targets < 0 or skip_route_targets > route_count:
        raise ValueError("invalid number of redundant route targets to skip")
    route_targets = route_targets[skip_route_targets:]
    prelude = list(prelude_targets or [])
    if any(not entry.startswith("target ") for entry in prelude):
        raise ValueError("controller-route prelude contains an invalid target")
    combined_count = len(prelude) + opening_count + len(route_targets)
    if combined_count > MAX_TARGETS:
        raise ValueError("combined end-to-end route exceeds runtime capacity")
    return [lines[0], lines[1], f"targets {combined_count}",
            *prelude, *opening_targets, *route_targets]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--frames", type=int, default=55000)
    parser.add_argument("--radius", type=float, default=135.0)
    parser.add_argument("--opening-route", type=Path,
                        help="controller-only combat route to run first")
    parser.add_argument("--skip-route-targets", type=int, default=0,
                        help="drop redundant leading targets after the prefix")
    args = parser.parse_args()
    if not 1 <= args.frames <= 60000:
        raise SystemExit("frames must be in 1..60000")
    if not math.isfinite(args.radius) or args.radius <= 0.0:
        raise SystemExit("radius must be positive")
    try:
        document = json.loads(args.manifest.read_text())
        lines = build_route(document, args.frames, args.radius)
        if args.opening_route is not None:
            lines = prepend_controller_route(
                lines, args.opening_route, args.skip_route_targets)
        elif args.skip_route_targets != 0:
            raise ValueError("route targets can only be skipped with a prefix")
    except (OSError, ValueError, TypeError, json.JSONDecodeError) as error:
        raise SystemExit(str(error)) from error
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n")
    print(f"generated {len(lines) - 3} target Dam end-to-end route -> "
          f"{args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
