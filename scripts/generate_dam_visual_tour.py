#!/usr/bin/env python3
"""Generate deterministic original-camera views from Dam's authored pads."""

from __future__ import annotations

import argparse
import hashlib
import itertools
import json
import math
from pathlib import Path
import re
import tempfile
import os


LEVEL_SCALE = 0.23363999
EYE_HEIGHT = 175.0
SPAWN_PAD = 33

# Authored marker pads following the route a normal Dam playthrough takes from
# the insertion point, down the road, through the tunnel, and onto the dam.
# Closely-spaced opposite-facing marker pairs are represented once so the
# diagnostic tour stays on the middle of the useful line of travel instead of
# bouncing between the two sides of a gate or corridor.
MAIN_PLAYER_ROUTE_PADS = (
    33, 32, 31, 30, 29, 34, 35, 39, 38, 41, 42, 43, 45,
    47, 49, 51, 53, 55,
)
DEFAULT_DIRECTIONS = ("forward", "left", "right")
AUTHORED_ROUTE_NAMES = ("main", "modem", "alarms", "bungee", "objectives")

# These are not hand-picked coordinates.  They are stable identities in the
# decompiled setup which are checked against the matching prop/AI records by
# parse_mission_targets before they are used.
MODEM_PROP_INDICES = (290, 292)
ALARM_TAGS = (0, 1, 2, 3)
ALARM_PROP_INDICES = (310, 312, 314, 316)
GATE_PROP_INDICES = (267, 268)
ARMOUR_PROP_INDICES = (318, 319)


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def parse_vector(text: str) -> list[float]:
    values = [float(value.strip().removesuffix("f")) for value in text.split(",")]
    if len(values) != 3:
        raise ValueError(f"unexpected vector: {text}")
    return values


def parse_pads(source: bytes) -> list[dict[str, object]]:
    text = source.decode("utf-8")
    section = text[text.index("PadRecord padlist[] = {"):
                   text.index("BoundPadRecord pad3dlist[] = {")]
    pattern = re.compile(
        r'^\s*\{ \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, "([^"]*)", 0 \},$',
        re.MULTILINE,
    )
    pads = [
        {
            "index": index,
            "position": parse_vector(match.group(1)),
            "up": parse_vector(match.group(2)),
            "look": parse_vector(match.group(3)),
            "stan": match.group(4),
        }
        for index, match in enumerate(pattern.finditer(section))
    ]
    if len(pads) <= SPAWN_PAD:
        raise ValueError(f"only found {len(pads)} Dam pads")
    return pads


def parse_pad_names(source: bytes, pad_count: int) -> list[str | None]:
    text = source.decode("utf-8")
    section = text[text.index("char *padnames[] = {"):]
    names = re.findall(r'^\s*"([^"]*)",', section, re.MULTILINE)
    return [*names[:pad_count], *([None] * max(0, pad_count - len(names)))]


def parse_bound_pads(source: bytes) -> list[dict[str, object]]:
    text = source.decode("utf-8")
    section = text[text.index("BoundPadRecord pad3dlist[] = {"):
                   text.index("s32 propDefs[] = {")]
    pattern = re.compile(
        r'^\s*\{ \{([^}]*)\}, \{([^}]*)\}, \{([^}]*)\}, "([^"]*)", '
        r'0, \{([^}]*)\} \},$', re.MULTILINE,
    )
    pads = [
        {
            "index": index,
            "position": parse_vector(match.group(1)),
            "look": parse_vector(match.group(2)),
            "up": parse_vector(match.group(3)),
            "stan": match.group(4),
        }
        for index, match in enumerate(pattern.finditer(section))
    ]
    if not pads:
        raise ValueError("no Dam bound pads found")
    return pads


def parse_prop_bound_pad(text: str, prop_index: int, prop_type: str) -> int:
    pattern = re.compile(
        rf'/\* Type = {re.escape(prop_type)}; index = {prop_index} \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\([^,]+, (\d+)\)',
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"Dam {prop_type} prop {prop_index} was not found")
    encoded = int(match.group(1))
    if encoded < 10000:
        raise ValueError(f"Dam {prop_type} prop {prop_index} is not on a bound pad")
    return encoded - 10000


def parse_tagged_standard_prop(text: str, tag: int) -> tuple[int, int]:
    pattern = re.compile(
        rf'/\* Type = Tag; index = (\d+) \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\({tag}, [^\n]+\n'
        rf'\s*/\* Type = StandardProp; index = (\d+) \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\([^,]+, (\d+)\)',
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"Dam tagged standard prop {tag} was not found")
    return int(match.group(2)), int(match.group(3))


def parse_tagged_bound_prop(text: str, tag: int, prop_index: int,
                            prop_type: str) -> int:
    """Resolve a bound prop only when the authored tag owns that prop."""
    pattern = re.compile(
        rf'/\* Type = Tag; index = \d+ \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\({tag}, [^\n]+\n'
        rf'\s*/\* Type = {re.escape(prop_type)}; index = {prop_index} \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\([^,]+, (\d+)\)',
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(
            f"Dam tag {tag} does not own {prop_type} prop {prop_index}")
    encoded = int(match.group(1))
    if encoded < 10000:
        raise ValueError(
            f"Dam tagged {prop_type} prop {prop_index} is not on a bound pad")
    return encoded - 10000


def parse_door_bound_pad(text: str, prop_index: int) -> int:
    """Resolve setupDoor's direct bound-pad namespace from authored source."""
    pattern = re.compile(
        rf'/\* Type = Door; index = {prop_index} \*/\s*\n'
        rf'\s*_mkword\([^\n]*?_mkword\([^,]+, (\d+)\)',
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"Dam Door prop {prop_index} was not found")
    return int(match.group(1))


def parse_armour_pad(text: str, prop_index: int) -> tuple[int, int, int]:
    """Resolve an authored body-armour record's model, pad and amount."""
    pattern = re.compile(
        rf'/\* Type = Armour; index = {prop_index} \*/\s*\n'
        rf'\s*_mkword\(384, _mkshort\(0, 21\)\), '
        rf'_mkword\((\d+), (\d+)\),[^\n]*?, (\d+), 0,$',
        re.MULTILINE,
    )
    match = pattern.search(text)
    if match is None:
        raise ValueError(f"Dam Armour prop {prop_index} was not found")
    return tuple(map(int, match.groups()))


def parse_mission_targets(source: bytes,
                          bound_pad_count: int,
                          pad_count: int | None = None) -> dict[str, object]:
    """Resolve mission landmarks from their exact setup prop/AI records."""
    text = source.decode("utf-8")
    modem = [parse_prop_bound_pad(text, index, prop_type)
             for index, prop_type in zip(MODEM_PROP_INDICES,
                                         ("SingleMonitor", "StandardProp"))]
    alarms = [parse_tagged_bound_prop(text, tag, index, "Alarm")
              for tag, index in zip(ALARM_TAGS, ALARM_PROP_INDICES)]
    backup = [parse_tagged_standard_prop(text, tag) for tag in (6, 7)]
    gates = [parse_door_bound_pad(text, index)
             for index in GATE_PROP_INDICES]
    armour = [parse_armour_pad(text, index)
              for index in ARMOUR_PROP_INDICES]
    exit_match = re.search(
        r'u8 ai_24\[] = \{.*?if_bond_in_room_with_pad\(0x([0-9a-f]{4}),',
        text, re.IGNORECASE | re.DOTALL,
    )
    if exit_match is None:
        raise ValueError("Dam bungee room test was not found in ai_24")
    encoded_exit = int(exit_match.group(1), 16)
    bungee_pad = ((encoded_exit & 0xff) << 8) | (encoded_exit >> 8)
    bound_indices = [*modem, *alarms, *gates]
    if any(index >= bound_pad_count for index in bound_indices):
        raise ValueError(f"Dam mission bound pad is out of range: {bound_indices}")
    if pad_count is not None and any(pad >= pad_count
                                     for _model, pad, _amount in armour):
        raise ValueError(f"Dam armour pad is out of range: {armour}")
    return {
        "modem_prop_indices": list(MODEM_PROP_INDICES),
        "modem_bound_pads": modem,
        "alarm_prop_indices": list(ALARM_PROP_INDICES),
        "alarm_tags": list(ALARM_TAGS),
        "alarm_bound_pads": alarms,
        "backup_tags": [6, 7],
        "backup_prop_indices": [prop for prop, _pad in backup],
        "backup_pads": [pad for _prop, pad in backup],
        "gate_prop_indices": list(GATE_PROP_INDICES),
        "gate_bound_pads": gates,
        "armour_prop_indices": list(ARMOUR_PROP_INDICES),
        "armour_model_ids": [model for model, _pad, _amount in armour],
        "armour_pads": [pad for _model, pad, _amount in armour],
        "armour_initial_amounts": [amount for _model, _pad, amount in armour],
        "bungee_ai_list": 24,
        "bungee_pad": bungee_pad,
    }


def parse_guard_spawns(source: bytes, pads: list[dict[str, object]]) \
        -> list[dict[str, object]]:
    """Resolve every authored Guard record to its exact setup pad."""
    text = source.decode("utf-8")
    section = text[text.index("s32 propDefs[] = {"):
                   text.index("s32 intro[] = {")]
    pattern = re.compile(
        r'/\* Type = Guard; index = (\d+) \*/\s*\n'
        r'\s*_mkword\([^\n]+?\),\s*_mkword\((\d+),\s*(\d+)\),',
    )
    guards = []
    for match in pattern.finditer(section):
        prop_index, chr_num, pad_index = map(int, match.groups())
        if pad_index >= len(pads):
            raise ValueError(
                f"Dam guard {chr_num} references invalid pad {pad_index}")
        pad = pads[pad_index]
        guards.append({
            "prop": prop_index,
            "chr": chr_num,
            "pad": pad_index,
            "position_raw": pad["position"],
            "stan": pad["stan"],
        })
    if len(guards) != 36 or len({guard["chr"] for guard in guards}) != 36:
        raise ValueError(f"expected 36 unique authored Dam guards, got {len(guards)}")
    return guards


def parse_waypoints(source: bytes, pad_count: int) -> list[dict[str, object]]:
    text = source.decode("utf-8")
    tables = {
        int(index): [int(value) for value in re.findall(r"\d+", values)][:-1]
        for index, values in re.findall(
            r"s32 path_table_(\d+)\[] = \{([^}]*)\};", text
        )
    }
    section = text[text.index("waypoint pathwaypoints[] = {"):
                   text.index("char *padnames[] = {")]
    records = re.findall(
        r'^\s*\{ (0x[0-9a-f]+), &path_table_(\d+), '
        r'(0x[0-9a-f]+), 0x[0-9a-f]+ \},$',
        section, re.IGNORECASE | re.MULTILINE,
    )
    waypoints = []
    for index, (pad_text, table_text, group_text) in enumerate(records):
        table_index = int(table_text)
        if table_index != index or table_index not in tables:
            raise ValueError(f"unexpected Dam waypoint table {table_index} at {index}")
        pad = int(pad_text, 16)
        if pad >= pad_count:
            raise ValueError(f"Dam waypoint {index} pad is out of range: {pad}")
        waypoints.append({
            "index": index,
            "pad": pad,
            "group": int(group_text, 16),
            "neighbors": tables[table_index],
        })
    if len(waypoints) != len(tables) or not waypoints:
        raise ValueError("Dam waypoint table is incomplete")
    for waypoint in waypoints:
        index = int(waypoint["index"])
        for neighbor in waypoint["neighbors"]:
            if neighbor >= len(waypoints) or index not in waypoints[neighbor]["neighbors"]:
                raise ValueError(f"Dam waypoint edge {index}->{neighbor} is not reciprocal")
    return waypoints


def normalize_xz(x: float, z: float) -> list[float]:
    length = math.hypot(x, z)
    if length <= 1e-9:
        raise ValueError("route contains coincident consecutive pads")
    return [x / length, 0.0, z / length]


def route_look(route: list[dict[str, object]], index: int) -> list[float]:
    """Return a centered route tangent, using authored X/Z pad positions."""
    segment = route[index].get("route_segment")
    previous_index = index - 1
    following_index = index + 1
    if previous_index < 0 or route[previous_index].get("route_segment") != segment:
        previous_index = index
    if (following_index >= len(route)
            or route[following_index].get("route_segment") != segment):
        following_index = index
    if previous_index == following_index:
        authored = [float(value) for value in route[index]["look"]]
        return normalize_xz(-authored[0], -authored[2])
    previous = route[previous_index]["position"]
    following = route[following_index]["position"]
    return normalize_xz(float(following[0]) - float(previous[0]),
                        float(following[2]) - float(previous[2]))


def waypoint_component(waypoints: list[dict[str, object]],
                       start: int) -> set[int]:
    visited = {start}
    pending = [start]
    for index in pending:
        for neighbor in waypoints[index]["neighbors"]:
            if neighbor not in visited:
                visited.add(neighbor)
                pending.append(neighbor)
    return visited


def nearest_waypoint(waypoints: list[dict[str, object]],
                     pads: list[dict[str, object]], position: list[float],
                     allowed: set[int] | None = None) -> int:
    candidates = allowed if allowed is not None else set(range(len(waypoints)))
    if not candidates:
        raise ValueError("no candidate Dam waypoints")

    def distance(index: int) -> float:
        pad_position = pads[int(waypoints[index]["pad"])]["position"]
        return math.hypot(float(pad_position[0]) - position[0],
                          float(pad_position[2]) - position[2])

    return min(candidates, key=lambda index: (distance(index), index))


def waypoint_route(waypoints: list[dict[str, object]], start: int,
                   target: int) -> list[int]:
    """Use the canonical table order and unweighted BFS used by the decomp."""
    previous: dict[int, int] = {}
    visited = {start}
    pending = [start]
    for index in pending:
        if index == target:
            break
        for neighbor in waypoints[index]["neighbors"]:
            if neighbor not in visited:
                visited.add(neighbor)
                previous[neighbor] = index
                pending.append(neighbor)
    if target not in visited:
        raise ValueError(f"no authored Dam waypoint route {start}->{target}")
    result = [target]
    while result[-1] != start:
        result.append(previous[result[-1]])
    result.reverse()
    return result


def append_segment(result: list[dict[str, object]],
                   pad_indices: list[int], by_index: dict[int, dict[str, object]],
                   segment: str) -> None:
    for pad_index in pad_indices:
        if pad_index not in by_index:
            raise ValueError(f"authored route pad was not resolved: {pad_index}")
        result.append({**by_index[pad_index], "route_segment": segment})


def build_authored_route(route_name: str,
                         resolved: list[dict[str, object]],
                         pads: list[dict[str, object]],
                         bound_pads: list[dict[str, object]],
                         waypoints: list[dict[str, object]],
                         targets: dict[str, object]) -> list[dict[str, object]]:
    by_index = {int(pad["index"]): pad for pad in resolved}
    if route_name == "main":
        result: list[dict[str, object]] = []
        append_segment(result, list(MAIN_PLAYER_ROUTE_PADS), by_index, "main")
        return result

    spawn_waypoint = nearest_waypoint(waypoints, pads,
                                      pads[SPAWN_PAD]["position"])
    modem_bound_indices = [int(value) for value in targets["modem_bound_pads"]]
    modem_position = [
        sum(float(bound_pads[index]["position"][axis])
            for index in modem_bound_indices) / len(modem_bound_indices)
        for axis in range(3)
    ]
    modem_waypoint = nearest_waypoint(waypoints, pads, modem_position)
    modem_waypoints = waypoint_route(waypoints, spawn_waypoint, modem_waypoint)

    alarm_bound_indices = [int(value) for value in targets["alarm_bound_pads"]]
    alarm_positions = [bound_pads[index]["position"]
                       for index in alarm_bound_indices]
    first_alarm_waypoint = nearest_waypoint(waypoints, pads, alarm_positions[-1])
    first_alarm_route = waypoint_route(waypoints, spawn_waypoint,
                                       first_alarm_waypoint)

    # The road/tower navigation is a separate authored waypoint component from
    # the insertion/security-building component.  Start that component at the
    # point closest to the first alarm, then follow exact links to each of the
    # three downstream alarms.
    second_alarm_waypoint = nearest_waypoint(waypoints, pads,
                                             alarm_positions[0])
    road_component = waypoint_component(waypoints, second_alarm_waypoint)
    road_start = nearest_waypoint(waypoints, pads, alarm_positions[-1],
                                  road_component)
    downstream_alarms = [
        nearest_waypoint(waypoints, pads, position, road_component)
        for position in alarm_positions[:3]
    ]
    alarm_segments = [first_alarm_route]
    current = road_start
    for target in downstream_alarms:
        alarm_segments.append(waypoint_route(waypoints, current, target))
        current = target

    bungee_pad = int(targets["bungee_pad"])
    if bungee_pad not in by_index:
        raise ValueError(f"Dam bungee pad was not resolved: {bungee_pad}")
    bungee_waypoint = nearest_waypoint(waypoints, pads,
                                       pads[bungee_pad]["position"],
                                       road_component)
    bungee_from_road = waypoint_route(waypoints, road_start, bungee_waypoint)
    bungee_after_alarms = waypoint_route(waypoints, current, bungee_waypoint)

    result = []

    def add_waypoint_segment(indices: list[int], name: str) -> None:
        for waypoint_index in indices:
            pad_index = int(waypoints[waypoint_index]["pad"])
            if pad_index not in by_index:
                raise ValueError(f"authored route pad was not resolved: {pad_index}")
            result.append({**by_index[pad_index], "route_segment": name,
                           "waypoint": waypoint_index,
                           "waygroup": int(waypoints[waypoint_index]["group"])})

    if route_name == "modem":
        add_waypoint_segment(modem_waypoints, "modem-approach")
    elif route_name == "alarms":
        for index, segment in enumerate(alarm_segments):
            add_waypoint_segment(segment, f"alarm-{index + 1}")
    elif route_name == "bungee":
        add_waypoint_segment(bungee_from_road, "bungee-approach")
        append_segment(result, [bungee_pad], by_index, "bungee-exit")
    elif route_name == "objectives":
        add_waypoint_segment(modem_waypoints, "modem-approach")
        # Continue from the modem endpoint. Restarting first_alarm_route at
        # spawn made a controller route aim directly back across the entire
        # level instead of following the authored reciprocal waypoint graph.
        connected_alarm_segments = [
            waypoint_route(waypoints, modem_waypoint, first_alarm_waypoint),
            *alarm_segments[1:],
        ]
        for index, segment in enumerate(connected_alarm_segments):
            add_waypoint_segment(segment, f"alarm-{index + 1}")
        add_waypoint_segment(bungee_after_alarms, "bungee-after-alarms")
        append_segment(result, [bungee_pad], by_index, "bungee-exit")
    else:
        raise ValueError(f"unknown authored route: {route_name}")
    return result


def oriented_look(forward: list[float], direction: str) -> list[float]:
    if direction == "forward":
        return forward
    if direction == "back":
        return [-forward[0], 0.0, -forward[2]]
    if direction == "left":
        return [forward[2], 0.0, -forward[0]]
    if direction == "right":
        return [-forward[2], 0.0, forward[0]]
    raise ValueError(f"unsupported direction: {direction}")


def stan_pack_id(name: str) -> int:
    match = re.fullmatch(r"([pq])(\d+)([a-z])([0-7]?)", name)
    if match is None:
        raise ValueError(f"invalid STAN tile name: {name}")
    letter, number_text, file_letter, subtri_text = match.groups()
    number = int(number_text)
    if number > 32767:
        raise ValueError(f"STAN tile number is out of range: {name}")
    high = ((ord(letter) - ord("p")) << 15) | number
    low = (ord(file_letter) - ord("a")) * 8 + (int(subtri_text) if subtri_text else 0)
    return (high << 8) | low


def parse_stan_tiles(source: bytes) -> dict[int, dict[str, object]]:
    matches = re.findall(
        r"StandTile tile_(\d+) = \{\s*\n\s*(0x[0-9a-f]+), (0x[0-9a-f]+),"
        r".*?\n    \{\n(.*?)\n    \}\n\};",
        source.decode("utf-8"), re.IGNORECASE | re.DOTALL,
    )
    if not matches:
        raise ValueError("no Dam STAN tiles found")
    result: dict[int, dict[str, object]] = {}
    for tile_index, packed, room, point_block in matches:
        key = int(packed, 16)
        room_id = int(room, 16)
        points = [
            [float(x), float(y), float(z)]
            for x, y, z in re.findall(
                r"\{(-?\d+),\s*(-?\d+),\s*(-?\d+),\s*0x[0-9a-f]+\}",
                point_block, re.IGNORECASE,
            )
        ]
        if len(points) < 3:
            raise ValueError(f"STAN tile {tile_index} has fewer than three points")
        if key in result and int(result[key]["room"]) != room_id:
            raise ValueError(f"STAN id {key:#x} belongs to multiple rooms")
        result[key] = {"room": room_id, "points": points,
                       "tile_index": int(tile_index)}
    return result


def stan_floor_y(points: list[list[float]], x: float, z: float) -> float:
    for first, second, third in itertools.combinations(points, 3):
        a = [second[i] - first[i] for i in range(3)]
        b = [third[i] - first[i] for i in range(3)]
        normal = [a[1] * b[2] - a[2] * b[1],
                  a[2] * b[0] - a[0] * b[2],
                  a[0] * b[1] - a[1] * b[0]]
        if abs(normal[1]) > 1e-9:
            plane = sum(normal[i] * first[i] for i in range(3))
            return (plane - normal[0] * x - normal[2] * z) / normal[1]
    raise ValueError("STAN tile has no nonvertical floor plane")


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(data)
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, path)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--setup", type=Path,
                        default=Path("assets/obseg/setup/UsetupdamZ.c"))
    parser.add_argument("--stan", type=Path,
                        default=Path("assets/obseg/stan/Tbg_dam_all_p_stanZ.c"))
    parser.add_argument("--output", type=Path,
                        default=Path("build/visual-probe/dam-authored.geview"))
    parser.add_argument("--manifest", type=Path,
                        default=Path("build/visual-probe/dam-authored.json"))
    parser.add_argument("--frames", type=int, default=30,
                        help="display frames held at each view")
    parser.add_argument("--route", choices=AUTHORED_ROUTE_NAMES,
                        help="authored player route (default: main)")
    parser.add_argument("--room-coverage", action="store_true",
                        help="use the former one-representative-pad-per-room sweep")
    parser.add_argument("--all-pads", action="store_true",
                        help="include every pad instead of one representative per room")
    parser.add_argument("--pad", type=int, action="append",
                        help="emit only this authored pad (repeatable)")
    parser.add_argument(
        "--directions", default=",".join(DEFAULT_DIRECTIONS),
        help="comma-separated route views: forward,left,right,back "
             "(non-route selections retain authored pad headings)",
    )
    args = parser.parse_args()
    if args.frames <= 0 or args.frames > 3600:
        parser.error("--frames must be in 1..3600")

    setup_bytes = args.setup.read_bytes()
    stan_bytes = args.stan.read_bytes()
    pads = parse_pads(setup_bytes)
    bound_pads = parse_bound_pads(setup_bytes)
    mission_targets = parse_mission_targets(
        setup_bytes, len(bound_pads), len(pads))
    waypoints = parse_waypoints(setup_bytes, len(pads))
    spawn_waypoint = nearest_waypoint(waypoints, pads,
                                      pads[SPAWN_PAD]["position"])
    modem_bound_indices = [
        int(value) for value in mission_targets["modem_bound_pads"]]
    modem_position = [
        sum(float(bound_pads[index]["position"][axis])
            for index in modem_bound_indices) / len(modem_bound_indices)
        for axis in range(3)
    ]
    modem_waypoint = nearest_waypoint(waypoints, pads, modem_position)
    modem_route = waypoint_route(waypoints, spawn_waypoint, modem_waypoint)

    armour_routes: dict[int, dict[str, object] | None] = {}
    for prop, pad_index in zip(mission_targets["armour_prop_indices"],
                               mission_targets["armour_pads"]):
        armour_waypoint = nearest_waypoint(
            waypoints, pads, pads[int(pad_index)]["position"])
        if armour_waypoint not in waypoint_component(waypoints,
                                                     spawn_waypoint):
            armour_routes[int(prop)] = None
            continue
        armour_route = waypoint_route(
            waypoints, spawn_waypoint, armour_waypoint)
        common_length = 0
        while common_length < min(len(modem_route), len(armour_route)) \
                and modem_route[common_length] == armour_route[common_length]:
            common_length += 1
        if common_length == 0 or common_length == len(armour_route):
            raise ValueError("Dam armour has no authored route detour")
        branch_waypoint = armour_route[common_length - 1]
        # The final graph node is nearest the pickup itself. The controller
        # target uses the exact setup position, so retain only the intervening
        # graph nodes on the out-and-back detour.
        approach_waypoints = armour_route[common_length:-1]
        armour_routes[int(prop)] = {
            "branch_waypoint": branch_waypoint,
            "branch_pad": int(waypoints[branch_waypoint]["pad"]),
            "approach": [
                {
                    "waypoint": waypoint_index,
                    "pad": int(waypoints[waypoint_index]["pad"]),
                    "position_raw": pads[int(
                        waypoints[waypoint_index]["pad"])]["position"],
                }
                for waypoint_index in approach_waypoints
            ],
        }
    mission_landmarks = {
        "guards": parse_guard_spawns(setup_bytes, pads),
        "modem": [
            {"prop": prop, "bound_pad": bound,
             "position_raw": bound_pads[bound]["position"],
             "stan": bound_pads[bound]["stan"]}
            for prop, bound in zip(mission_targets["modem_prop_indices"],
                                   mission_targets["modem_bound_pads"])
        ],
        "alarms": [
            {"tag": tag, "prop": prop, "bound_pad": bound,
             "position_raw": bound_pads[bound]["position"],
             "stan": bound_pads[bound]["stan"]}
            for tag, prop, bound in zip(
                mission_targets["alarm_tags"],
                mission_targets["alarm_prop_indices"],
                mission_targets["alarm_bound_pads"])
        ],
        "backup_terminals": [
            {"tag": tag, "prop": prop, "pad": pad,
             "position_raw": pads[pad]["position"],
             "stan": pads[pad]["stan"]}
            for tag, prop, pad in zip(
                mission_targets["backup_tags"],
                mission_targets["backup_prop_indices"],
                mission_targets["backup_pads"])
        ],
        "gates": [
            {"prop": prop, "bound_pad": bound,
             "position_raw": bound_pads[bound]["position"],
             "stan": bound_pads[bound]["stan"]}
            for prop, bound in zip(
                mission_targets["gate_prop_indices"],
                mission_targets["gate_bound_pads"])
        ],
        "armour": [
            {"prop": prop, "model": model, "pad": pad,
             "initial_amount_fixed": amount,
             "position_raw": pads[pad]["position"],
             "stan": pads[pad]["stan"],
             "route_detour": armour_routes[int(prop)]}
            for prop, model, pad, amount in zip(
                mission_targets["armour_prop_indices"],
                mission_targets["armour_model_ids"],
                mission_targets["armour_pads"],
                mission_targets["armour_initial_amounts"])
        ],
        "bungee": {
            "ai_list": mission_targets["bungee_ai_list"],
            "pad": mission_targets["bungee_pad"],
            "position_raw": pads[int(mission_targets["bungee_pad"])]["position"],
            "stan": pads[int(mission_targets["bungee_pad"])]["stan"],
        },
    }
    pad_names = parse_pad_names(setup_bytes, len(pads))
    for index, name in enumerate(pad_names):
        pads[index]["name"] = name
    stan_tiles = parse_stan_tiles(stan_bytes)
    resolved = []
    unresolved = []
    for pad in pads:
        if not pad["stan"]:
            continue
        tile = stan_tiles.get(stan_pack_id(str(pad["stan"])))
        if tile is None:
            unresolved.append(int(pad["index"]))
            continue
        resolved.append({**pad, "room": int(tile["room"]),
                         "stan_points": tile["points"],
                         "stan_tile_index": int(tile["tile_index"])})
    if not resolved or unresolved:
        raise ValueError(f"unresolved Dam pad STAN records: {unresolved}")

    explicit_selection_count = sum((bool(args.pad), args.all_pads,
                                    args.room_coverage, args.route is not None))
    if explicit_selection_count > 1:
        parser.error("--route, --pad, --all-pads, and --room-coverage are mutually exclusive")

    route_name = args.route or "main"
    selection = "main-player-route"
    if args.pad:
        requested = set(args.pad)
        selected = [pad for pad in resolved if int(pad["index"]) in requested]
        found = {int(pad["index"]) for pad in selected}
        if found != requested:
            raise ValueError(f"requested pads were not resolved: {sorted(requested - found)}")
        selection = "explicit-pads"
    elif args.all_pads:
        selected = resolved
        selection = "all-pads"
    elif args.room_coverage:
        selected = []
        seen_rooms: set[int] = set()
        spawn = next(pad for pad in resolved if int(pad["index"]) == SPAWN_PAD)
        selected.append(spawn)
        seen_rooms.add(int(spawn["room"]))
        for pad in resolved:
            room = int(pad["room"])
            if room not in seen_rooms:
                selected.append(pad)
                seen_rooms.add(room)
        selection = "first-pad-per-room"
    else:
        selected = build_authored_route(route_name, resolved, pads, bound_pads,
                                        waypoints, mission_targets)
        selection = ("main-player-route" if route_name == "main"
                     else f"authored-{route_name}-route")

    directions = [value.strip() for value in args.directions.split(",")
                  if value.strip()]
    if not directions:
        parser.error("--directions must contain at least one direction")
    invalid_directions = sorted(set(directions) -
                                {"forward", "left", "right", "back"})
    if invalid_directions:
        parser.error(f"unsupported --directions: {','.join(invalid_directions)}")
    route_mode = selection.endswith("-route")
    emitted_directions = directions if route_mode else ["authored"]

    lines = [
        "GEVIEW1",
        "# frames room runtime-x runtime-y runtime-z look-x look-y look-z "
        "up-x up-y up-z label",
    ]
    manifest_views = []
    for route_index, pad in enumerate(selected):
        raw_position = [float(value) for value in pad["position"]]
        floor_y = stan_floor_y(pad["stan_points"], raw_position[0],
                               raw_position[2])
        position = [raw_position[0] / LEVEL_SCALE,
                    floor_y / LEVEL_SCALE + EYE_HEIGHT,
                    raw_position[2] / LEVEL_SCALE]
        authored_look = [float(value) for value in pad["look"]]
        up = [float(value) for value in pad["up"]]
        if route_mode:
            base_look = route_look(selected, route_index)
        else:
            # bondviewLoadSetupIntroSpawnSlice turns the setup pad heading
            # into the player-view direction with the opposite sign.
            base_look = [-value for value in authored_look]
        raw_name = pad.get("name") or f"pad-{int(pad['index']):03d}"
        short_name = str(raw_name).removeprefix("PAD_").removesuffix("_dam_all_p")
        for direction in emitted_directions:
            look = (oriented_look(base_look, direction)
                    if direction != "authored" else base_look)
            segment = str(pad.get("route_segment") or route_name)
            label = (f"route-{route_name}-{route_index:02d}-"
                     f"{short_name}-{direction}"
                     if route_mode else
                     f"pad-{int(pad['index']):03d}-room-{int(pad['room']):03d}-{pad['stan']}")
            numbers = [*position, *look, *up]
            lines.append(
                f"{args.frames} {int(pad['room'])} "
                + " ".join(f"{value:.9g}" for value in numbers)
                + f" {label}"
            )
            manifest_views.append({
                "label": label,
                "route_checkpoint": route_index if route_mode else None,
                "route_segment": segment if route_mode else None,
                "waypoint": pad.get("waypoint"),
                "waygroup": pad.get("waygroup"),
                "direction": direction,
                "pad": int(pad["index"]),
                "pad_name": pad.get("name"),
                "room": int(pad["room"]),
                "stan": pad["stan"],
                "stan_tile_index": pad["stan_tile_index"],
                "floor_y_raw": floor_y,
                "position_raw": pad["position"],
                "position_runtime": position,
                "authored_pad_look": authored_look,
                "look": look,
                "up": up,
                "hold_frames": args.frames,
            })
    encoded = ("\n".join(lines) + "\n").encode()
    manifest = {
        "schema": 1,
        "stage": "Dam",
        "format": "GEVIEW1",
        "level_scale": LEVEL_SCALE,
        "eye_height_runtime": EYE_HEIGHT,
        "selection": selection,
        "route": route_name if route_mode else None,
        "directions": emitted_directions,
        "mission_targets": mission_targets,
        "mission_landmarks": mission_landmarks,
        "source_sha256": {"setup": sha256(setup_bytes), "stan": sha256(stan_bytes)},
        "pad_count": len(pads),
        "view_count": len(manifest_views),
        "total_frames": len(manifest_views) * args.frames,
        "views": manifest_views,
    }
    atomic_write(args.output, encoded)
    atomic_write(args.manifest,
                 (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
    print(f"generated {len(manifest_views)} authored Dam views / "
          f"{manifest['total_frames']} frames -> {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
