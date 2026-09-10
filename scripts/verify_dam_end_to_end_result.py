#!/usr/bin/env python3
"""Verify Dam's controller route and completion in its retained campaign save."""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess


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


def verify_route(path: Path) -> str:
    """Check controller telemetry only; callback counts do not prove a save write."""
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
    if values.get("setup_difficulty") != "2" or values.get("difficulty") != "2":
        raise ValueError("all-objective Dam route requires 00 Agent difficulty "
                         "at setup and result")
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
        raise ValueError("successful Dam result did not dispatch its canonical "
                         "provider callbacks")

    sound = integers(values, "sound", 4)
    if sound[0] < 1 or sound[1] < 1 or sound[2] != 0:
        raise ValueError("original mission/weapon sound path was not healthy")
    average_ms = int(values.get("frame_average_ms", "100000"), 0)
    if average_ms > 50:
        raise ValueError(f"average frame time is too slow ({average_ms} ms)")
    return (f"Dam controller route verified: {total} authored stops, "
            f"{values['rooms_visited']} rooms, {shots} PP7 shots, "
            f"mission 0x{registers:08x}, {average_ms} ms/frame")


def verify(path: Path, save: Path, folder: int = 0) -> str:
    """Require both the unchanged route checks and a real, read-only save reload."""
    route = verify_route(path)
    if folder not in range(4):
        raise ValueError("save folder must be 0, 1, 2 or 3")
    try:
        save_bytes = save.read_bytes()
    except OSError as error:
        raise ValueError(f"retained save could not be read: {error}") from error
    inspector = Path(__file__).resolve().with_name("inspect_campaign_save.sh")
    result = subprocess.run(
        ["bash", str(inspector), str(save), str(folder)],
        capture_output=True, text=True)
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip()
        raise ValueError(
            "retained save did not verify Dam 00 Agent completion and "
            f"Facility 00 Agent access (inspector exit {result.returncode}): "
            f"{detail}")
    if save.read_bytes() != save_bytes:
        raise ValueError("retained save changed during verification; retain "
                         "a stable copy before checking completion and unlock")
    try:
        report = json.loads(result.stdout)
    except ValueError as error:
        raise ValueError("invalid retained-save inspection report") from error
    if not isinstance(report, dict) \
            or report.get("format") != "GE_CAMPAIGN_SAVE_INSPECTION_1" \
            or report.get("folder") != folder \
            or report.get("crc_valid") is not True \
            or report.get("loads") != 1 or report.get("writes") != 0 \
            or report.get("acceptance_passed") is not True:
        raise ValueError("retained-save inspection did not confirm read-only "
                         "completion and unlock acceptance")
    return (f"{route}\nRetained save verified: durable Dam 00 Agent completion "
            f"({report['dam_00_time_seconds']} s) and Facility 00 Agent access "
            f"in folder {folder}; CRC valid, 1 reload, 0 writes.\n"
            f"Save source: {save.resolve()} ({len(save_bytes)} bytes, "
            f"SHA-256 {hashlib.sha256(save_bytes).hexdigest()})")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("result", type=Path)
    evidence = parser.add_mutually_exclusive_group(required=True)
    evidence.add_argument("--save", type=Path,
                          help="retained save from this run; required for "
                               "durable completion/unlock acceptance")
    evidence.add_argument("--route-only", action="store_true",
                          help="check controller telemetry without certifying "
                               "saved completion or unlock")
    parser.add_argument("--folder", choices=("0", "1", "2", "3"),
                        help="retained save folder ID (default: 0)")
    args = parser.parse_args()
    if args.route_only and args.folder is not None:
        parser.error("--folder requires --save")
    try:
        if args.route_only:
            route = verify_route(args.result)
            print(f"Route-only check; retained-save persistence not verified.\n{route}")
        else:
            print(verify(args.result, args.save, int(args.folder or "0")))
    except (OSError, ValueError) as error:
        raise SystemExit(str(error)) from error
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
