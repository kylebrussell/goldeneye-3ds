#!/usr/bin/env python3
"""Verify that a controller-only Azahar run completed Dam's authored flow."""

from __future__ import annotations

import argparse
import math
from pathlib import Path


REQUIRED_MISSION_BITS = 0x00000100 | 0x00000400 | 0x00001000


def parse(path: Path) -> tuple[dict[str, str], dict[str, list[str]]]:
    values: dict[str, str] = {}
    repeated: dict[str, list[str]] = {}
    for line in path.read_text().splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key] = value
        repeated.setdefault(key, []).append(value)
    return values, repeated


def integers(values: dict[str, str], key: str, count: int) -> list[int]:
    try:
        result = [int(item, 0) for item in values[key].split(",")]
    except (KeyError, ValueError) as error:
        raise ValueError(f"missing or invalid {key}") from error
    if len(result) != count:
        raise ValueError(f"{key} must contain {count} values")
    return result


def verify(path: Path) -> str:
    values, repeated = parse(path)
    if values.get("status") != "complete":
        try:
            shots, stan_hits, clear_paths, guard_hits, _damage = integers(
                values, "pp7", 5)
            reached, total = integers(values, "route_targets", 2)
        except ValueError:
            shots = stan_hits = clear_paths = guard_hits = 0
            reached = total = 0
        if shots > 0 and stan_hits == 0 and clear_paths == 0:
            raise ValueError(
                f"controller route stopped at {reached}/{total}: all {shots} "
                "PP7 shots failed the canonical player-to-muzzle STAN "
                "traversal before guard/object hit testing; route cadence "
                "cannot validate modem/objectives/exit until that live STAN "
                f"boundary is repaired (guard hits {guard_hits})")
        if shots > 0 and stan_hits + clear_paths == shots:
            try:
                rays_tested = int(values["guard_hit_test"].split(",", 1)[0],
                                  0)
            except (KeyError, ValueError):
                rays_tested = -1
            if rays_tested == 0:
                raise ValueError(
                    f"controller route stopped at {reached}/{total}: all "
                    f"{shots} PP7 shots reached canonical STAN background "
                    "resolution, but the live guard hit-list population "
                    "accepted zero rays; modem/objective/exit validation "
                    "cannot survive the opening combat until that exact "
                    "boundary is repaired")
        raise ValueError("controller route did not complete")
    reached, total = integers(values, "route_targets", 2)
    if reached != total or total < 100:
        raise ValueError("authored start-to-exit route did not reach every stop")
    if int(values.get("rooms_visited", "0"), 0) < 10:
        raise ValueError("route did not traverse a meaningful Dam room set")

    # A completed route is not valid evidence if the actor scheduler failed
    # part-way through it.  In particular, a watch pause used to leave the
    # active-prop service in TIMER_UNBOUND while the rest of the platform
    # continued drawing and accepting route input.  Require the exact healthy
    # end-state published by the live runtime: ready actor loop, successful
    # active-prop/mission services, live monitor/objective ticks, successful
    # lighting/matrix publication, and no active-list binding divergence.
    actor_status = integers(values, "actor_status", 8)
    if actor_status != [1, 0, 0, 1, 1, 0, 0, 0]:
        raise ValueError(
            f"actor scheduler was not healthy for the completed route: "
            f"{actor_status}")
    try:
        simulation_frames = int(values["simulation_frames"], 0)
        actor_ticks = int(values["actor_ticks"], 0)
    except (KeyError, ValueError) as error:
        raise ValueError("missing actor scheduler tick evidence") from error
    if simulation_frames <= 0 or actor_ticks < total \
            or actor_ticks > simulation_frames:
        raise ValueError(
            f"actor scheduler tick evidence is inconsistent "
            f"({actor_ticks}/{simulation_frames})")

    registers = int(values.get("mission_objectives", "0"), 0)
    if registers & REQUIRED_MISSION_BITS != REQUIRED_MISSION_BITS:
        raise ValueError(
            f"mission AI did not set modem, backup and bungee bits "
            f"(0x{registers:08x})")

    objectives: dict[int, tuple[int, int]] = {}
    try:
        for item in values["objective_status"].split(","):
            menu, value, blocker, _criterion = [int(part, 0)
                                                  for part in item.split(":")]
            objectives[menu] = value, blocker
    except (KeyError, ValueError) as error:
        raise ValueError("missing canonical objective evaluations") from error
    if any(objectives.get(menu) != (1, 0) for menu in range(4)):
        raise ValueError(f"not all four authored objectives completed: {objectives}")

    door = integers(values, "door_interaction", 6)
    if door[1] < 2 or door[3] < 2 or door[4] < 2:
        raise ValueError("both gate interactions were not exercised canonically")
    publications: dict[int, list[str]] = {}
    for entry in repeated.get("gate", []):
        parts = entry.split(",")
        if len(parts) != 7:
            raise ValueError("invalid gate publication")
        try:
            publications[int(parts[0], 0)] = parts
            int(parts[1], 0)
            int(parts[2], 0)
            float(parts[3])
            float(parts[4])
            int(parts[5], 0)
            int(parts[6], 0)
        except ValueError as error:
            raise ValueError("invalid gate publication") from error
    gates: dict[int, list[str]] = {}
    for entry in repeated.get("gate_route", []):
        parts = entry.split(",")
        if len(parts) != 10:
            raise ValueError("invalid gate route publication")
        try:
            gates[int(parts[0], 0)] = parts
            int(parts[1], 0)
            int(parts[2], 0)
            int(parts[3], 0)
            int(parts[4], 0)
            float(parts[5])
            int(parts[6], 0)
            float(parts[7])
            float(parts[8])
            int(parts[9], 0)
        except ValueError as error:
            raise ValueError("invalid gate route publication") from error
    for command in (267, 268):
        gate = gates.get(command)
        publication = publications.get(command)
        if gate is None or publication is None \
                or int(gate[1], 0) != 1 \
                or int(publication[1], 0) != 1:
            raise ValueError(f"authored gate {command} was not live/published")
        start_generation, end_generation = int(gate[2], 0), int(gate[3], 0)
        activations = int(gate[4], 0)
        maximum_open, max_frac = float(gate[5]), float(gate[8])
        if start_generation <= 0 or end_generation <= start_generation:
            raise ValueError(
                f"authored gate {command} did not publish a generation change")
        if activations < 1:
            raise ValueError(
                f"authored gate {command} was not activated by controller input")
        if not math.isfinite(maximum_open) or not math.isfinite(max_frac) \
                or max_frac <= 0.0 \
                or maximum_open + max(1.0e-5, abs(max_frac) * 1.0e-5) \
                    < max_frac:
            raise ValueError(f"authored gate {command} never fully opened")
        if int(gate[9], 0) != -1:
            raise ValueError(
                f"authored NO_PORTAL_CLOSE gate {command} gained a portal")
        if end_generation != int(publication[6], 0) \
                or int(gate[6], 0) != int(publication[2], 0) \
                or not math.isclose(float(gate[7]), float(publication[3]),
                                    rel_tol=1.0e-6, abs_tol=1.0e-6) \
                or not math.isclose(max_frac, float(publication[4]),
                                    rel_tol=1.0e-6, abs_tol=1.0e-6) \
                or int(gate[9], 0) != int(publication[5], 0):
            raise ValueError(
                f"authored gate {command} route/final telemetry diverged")
    try:
        both_open_frames = int(values["gate_both_open_frames"], 0)
    except (KeyError, ValueError) as error:
        raise ValueError("missing gate interlock overlap evidence") from error
    if both_open_frames != 0:
        raise ValueError(
            f"Dam security interlock allowed both gates open for "
            f"{both_open_frames} simulation frames")

    attempts, throws, rejections = integers(values, "modem", 3)
    if attempts < 1 or throws < 1 or rejections != 0:
        raise ValueError("covert modem did not complete its original throw path")
    shots, _stan_hits, _clear_paths, _guard_hits, _damage = integers(
        values, "pp7", 5)
    if shots < 4:
        raise ValueError("alarm route did not dispatch the required PP7 shots")
    object_hits, object_damage, objects_destroyed, _object_type, \
        _destroyed_level = integers(values, "pp7_object", 5)
    if object_hits < 4 or object_damage < 4 or objects_destroyed < 4:
        raise ValueError(
            "PP7 fire did not destroy all four authored alarm objects")
    player_combat = values.get("player_combat", "").split(",")
    if len(player_combat) != 7:
        raise ValueError("missing canonical player combat state")
    try:
        player_dead = int(player_combat[5], 0)
        player_invincible = int(player_combat[6], 0)
    except ValueError as error:
        raise ValueError("invalid canonical player combat state") from error
    if player_dead != 0:
        raise ValueError("Bond died before the authored route completed")
    if player_invincible != 0:
        raise ValueError("controller route used non-authentic invincibility")
    armour_probe = values.get("armour_probe", "").split(",")
    if len(armour_probe) != 2:
        raise ValueError("missing canonical armour-pickup evidence")
    try:
        maximum_armour = float(armour_probe[0])
        first_armour_frame = int(armour_probe[1], 0)
    except ValueError as error:
        raise ValueError("invalid canonical armour-pickup evidence") from error
    if not math.isfinite(maximum_armour) or maximum_armour <= 0.0 \
            or first_armour_frame <= 0 or first_armour_frame > simulation_frames:
        raise ValueError("authored Armour prop 318 was not collected canonically")
    unknown, chr7_unknown, *_rest = integers(values, "guard_ai_unknown", 6)
    if unknown != 0 or chr7_unknown != 0:
        raise ValueError("guard AI encountered unsupported authored opcodes")

    exit_fields = values.get("mission_exit", "").split(",")
    if len(exit_fields) != 10:
        raise ValueError("missing canonical Dam exit snapshot")
    if int(exit_fields[7], 0) < 1 or int(exit_fields[6], 0) < 1:
        raise ValueError("bungee exit did not enter original camera/fade flow")
    if int(exit_fields[8], 0) < 1:
        raise ValueError("fresh player input did not request the title stage")
    mission_result = integers(values, "mission_result", 3)
    if mission_result[0] < 1 or mission_result[1] != 0 \
            or mission_result[2] < 1:
        raise ValueError("successful Dam result was not persisted canonically")

    sound = integers(values, "sound", 4)
    if sound[0] < 1 or sound[1] < 1 or sound[2] != 0:
        raise ValueError("original mission/weapon sound path was not healthy")
    average_ms = int(values.get("frame_average_ms", "100000"), 0)
    if average_ms > 50:
        raise ValueError(f"average frame time is too slow ({average_ms} ms)")
    return (f"Dam end-to-end verified: {total} authored stops, "
            f"{values['rooms_visited']} rooms, {shots} PP7 shots, "
            f"mission 0x{registers:08x}, {average_ms} ms/frame")


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
