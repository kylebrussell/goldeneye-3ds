#!/usr/bin/env python3
"""Generate a finite visual tour from any solo stage's authored setup graph."""

from __future__ import annotations

import argparse
from collections import deque
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import re
import struct
import sys
import tempfile


GESTAN_MAGIC = b"GESTAN01"
EYE_HEIGHT = 175.0
DEFAULT_CAPACITY = 384


def load_inventory_module(root: Path):
    path = root / "scripts/generate_3ds_stage_inventory.py"
    spec = importlib.util.spec_from_file_location("ge_stage_tour_inventory", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load authored stage parser: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def atomic_write(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(dir=path.parent, delete=False) as temporary:
        temporary.write(data)
        temporary_path = Path(temporary.name)
    os.replace(temporary_path, path)


def parse_level_rows(text: str) -> dict[str, dict[str, object]]:
    pattern = re.compile(
        r"\{\s*(LEVELID_[A-Z0-9_]+)\s*,\s*"
        r'"bg/bg_([a-z0-9]+)_all_p\.seg"\s*,\s*'
        r'"(Tbg_[a-z0-9]+_all_p_stanZ)"\s*,\s*'
        r"([0-9.]+)\s*,"
    )
    return {
        symbol: {"background": background, "stan": stan,
                 "level_scale": float(scale)}
        for symbol, background, stan, scale in pattern.findall(text)
    }


def stage_catalog(root: Path, inventory) -> dict[str, dict[str, object]]:
    level_ids = inventory.parse_level_ids((root / "src/bondconstants.h").read_text())
    order = inventory.parse_solo_order((root / "src/boss.c").read_text())
    setups = inventory.parse_setup_table((root / "src/game/chraidata.c").read_text())
    worlds = parse_level_rows((root / "src/game/bg.c").read_text())
    result = {}
    campaign_order = [*order, "LEVELID_CUBA"]
    for sequence, symbol in enumerate(campaign_order):
        level_id = level_ids[symbol]
        setup_key = setups[level_id]
        if setup_key is None or symbol not in worlds:
            raise ValueError(f"{symbol} lacks an authored setup/world association")
        label = inventory.stage_label(symbol)
        key = re.sub(r"[^a-z0-9]+", "", label.lower())
        row = worlds[symbol]
        result[key] = {
            "key": key, "label": label, "symbol": symbol,
            "level_id": level_id, "solo_sequence_index": sequence,
            "setup_key": setup_key, "background_key": row["background"],
            "stan_key": row["stan"], "level_scale": row["level_scale"],
            "setup_path": inventory.source_for(root, "setup", setup_key),
            "collision_path": root / "build/3ds-levels" / key
                              / "collision/collision.gestan",
        }
    if len(result) != 21:
        raise ValueError(
            f"expected 20 solo missions plus Cuba, found {len(result)}")
    return result


def vector(text: str) -> list[float]:
    values = [float(value.strip().removesuffix("f")) for value in text.split(",")]
    if len(values) != 3:
        raise ValueError(f"invalid authored vector: {text}")
    return values


def parse_bound_pads(text: str, initializer) -> list[dict[str, object]]:
    body = initializer(text, "BoundPadRecord pad3dlist[] =")
    pattern = re.compile(
        r'\{\s*\{([^}]*)\},\s*\{([^}]*)\},\s*\{([^}]*)\},\s*'
        r'(?:"([^"]*)"|NULL),\s*(?:0|NULL),\s*\{([^}]*)\}\s*\},', re.M)
    result = [
        {"index": index, "position": vector(match.group(1)),
         "look": vector(match.group(2)), "up": vector(match.group(3)),
         "stan_name": match.group(4) or None}
        for index, match in enumerate(pattern.finditer(body))
    ]
    if (result and result[-1]["stan_name"] is None
            and not any(result[-1]["position"])):
        result.pop()
    return result


def parse_waypoints(text: str, pads: list[dict[str, object]], initializer
                    ) -> list[dict[str, object]]:
    tables = {
        int(index): [int(value) for value in re.findall(r"-?\d+", values)
                     if int(value) >= 0]
        for index, values in re.findall(
            r"s32 path_table_(\d+)\[] = \{([^}]*)\};", text)
    }
    start = text.index("waypoint pathwaypoints[] = {")
    body = initializer(text[start:], "waypoint pathwaypoints[] =")
    records = re.findall(
        r"\{\s*(0x[0-9a-f]+|\d+)\s*,\s*"
        r"(?:&path_table_(\d+)|NULL)\s*,\s*"
        r"(0x[0-9a-f]+|\d+)\s*,", body, re.I)
    result = []
    for pad_text, table_text, group_text in records:
        if not table_text:
            continue
        index = len(result)
        table = int(table_text)
        pad = int(pad_text, 0)
        if table != index or table not in tables or not 0 <= pad < len(pads):
            raise ValueError(f"invalid authored waypoint {index}")
        result.append({"index": index, "pad": pad,
                       "group": int(group_text, 0),
                       "neighbors": tables[table]})
    if len(result) != len(tables) or not result:
        raise ValueError("authored waypoint graph is incomplete")
    for waypoint in result:
        index = int(waypoint["index"])
        for neighbor in waypoint["neighbors"]:
            if (neighbor >= len(result)
                    or index not in result[neighbor]["neighbors"]):
                raise ValueError(f"waypoint edge {index}->{neighbor} is not reciprocal")
    return result


def parse_prop_targets(text: str, pads: list[dict[str, object]],
                       bound_pads: list[dict[str, object]], inventory,
                       initializer) -> tuple[list[dict[str, object]],
                                             list[dict[str, object]]]:
    body = initializer(text, "s32 propDefs[] =")
    comments = list(re.finditer(r"/\* Type = ([^;]+); index = (\d+) \*/", body))
    result = []
    skipped = []
    for offset, comment in enumerate(comments):
        end = comments[offset + 1].start() if offset + 1 < len(comments) else len(body)
        record = body[comment.end():end]
        header = re.search(
            r"_mkword\(\s*[^,]+,\s*_mkshort\(\s*[^,]+,\s*(\d+)\s*\)\s*\)",
            record)
        if (header is None
                or int(header.group(1)) not in inventory.OBJECT_PROPDEF_TYPES):
            continue
        pairs = re.findall(
            r"_mkword\(\s*(-?(?:0x[0-9a-f]+|\d+))\s*,\s*"
            r"(-?(?:0x[0-9a-f]+|\d+))\s*\)", record, re.I)
        if not pairs:
            raise ValueError(f"object prop {comment.group(2)} has no model/pad pair")
        pad_id = int(pairs[0][1], 0)
        prop_type = comment.group(1)
        # setupDoor indexes g_CurrentSetup.boundpads directly. Door records
        # therefore use the un-offset bound-pad namespace even though the
        # same small integer is also a valid ordinary pad index. Treating a
        # gate as an ordinary prop sent the diagnostic route to an unrelated
        # authored waypoint and could never exercise its real interaction.
        if prop_type == "Door" and 0 <= pad_id < len(bound_pads):
            pad_kind = "bound-pad"
            pad_index = pad_id
            pad = bound_pads[pad_index]
        elif 0 <= pad_id < len(pads):
            pad_kind = "pad"
            pad_index = pad_id
            pad = pads[pad_index]
        elif 10000 <= pad_id < 10000 + len(bound_pads):
            pad_kind = "bound-pad"
            pad_index = pad_id - 10000
            pad = bound_pads[pad_index]
        # The source generator prints negative signed pad sentinels as either
        # -N or their 16-bit unsigned spelling (Surface uses 65530..65535).
        elif pad_id < 0 or 0xff00 <= pad_id <= 0xffff:
            skipped.append({"prop_index": int(comment.group(2)),
                            "prop_type": comment.group(1),
                            "encoded_pad": pad_id, "reason": "sentinel"})
            continue
        else:
            # A few original setups retain dormant object records whose pad
            # indices are outside both live authored pad arrays.  Record them
            # as unpositioned evidence; never manufacture a coordinate.
            skipped.append({"prop_index": int(comment.group(2)),
                            "prop_type": comment.group(1),
                            "encoded_pad": pad_id,
                            "reason": "outside-authored-pad-arrays"})
            continue
        result.append({
            "prop_index": int(comment.group(2)), "prop_type": prop_type,
            "encoded_pad": pad_id, "pad_kind": pad_kind,
            "pad_index": pad_index, "position": pad["position"],
        })
    if not result:
        raise ValueError("setup contains no positioned authored props")
    return result, skipped


def parse_collision(path: Path) -> dict[int, dict[str, object]]:
    data = path.read_bytes()
    if len(data) < 32:
        raise ValueError(f"collision blob is truncated: {path}")
    magic, version, tile_count, point_count, _spawn, _room, points_offset = \
        struct.unpack_from(">8s6I", data)
    if (magic != GESTAN_MAGIC or version != 1
            or points_offset != 32 + tile_count * 20
            or len(data) != points_offset + point_count * 8):
        raise ValueError(f"collision blob has invalid structure: {path}")
    result = {}
    for index in range(tile_count):
        source_index, first, count, room, _special, tile_id, _mid, _tail = \
            struct.unpack_from(">IIHBBIHH", data, 32 + index * 20)
        if first + count > point_count or tile_id in result:
            raise ValueError(f"collision tile {index} is invalid")
        points = [
            list(struct.unpack_from(">hhhH", data,
                                    points_offset + (first + point) * 8)[:3])
            for point in range(count)
        ]
        result[tile_id] = {"tile_index": index, "source_index": source_index,
                           "room": room, "points": points}
    return result


def stan_pack_id(name: str) -> int:
    match = re.fullmatch(r"([pq])(\d+)([a-z])([0-7]?)", name)
    if match is None:
        raise ValueError(f"invalid authored STAN name: {name}")
    prefix, number, letter, subtriangle = match.groups()
    return ((((ord(prefix) - ord("p")) << 15) | int(number)) << 8) \
        | ((ord(letter) - ord("a")) * 8
           + (int(subtriangle) if subtriangle else 0))


def resolve_pad(pad: dict[str, object], collision: dict[int, dict[str, object]]
                ) -> dict[str, object]:
    name = pad.get("stan_name")
    tile = collision.get(stan_pack_id(str(name))) if name else None
    if tile is None:
        raise ValueError(f"pad {pad['index']} STAN {name!r} was not resolved")
    return {**pad, "room": tile["room"], "stan_tile_index": tile["tile_index"],
            "stan_points": tile["points"]}


def distance_squared(left: list[float], right: list[float]) -> float:
    return sum((float(left[axis]) - float(right[axis])) ** 2 for axis in range(3))


def nearest_waypoint(position: list[float], waypoints: list[dict[str, object]],
                     pads: list[dict[str, object]]) -> int:
    return min(range(len(waypoints)), key=lambda index: (
        distance_squared(position, pads[int(waypoints[index]["pad"])]["position"]),
        index))


def shortest_paths(waypoints: list[dict[str, object]], start: int
                   ) -> tuple[dict[int, int], dict[int, int]]:
    previous = {start: -1}
    distances = {start: 0}
    pending = deque([start])
    while pending:
        index = pending.popleft()
        for neighbor in waypoints[index]["neighbors"]:
            if neighbor not in previous:
                previous[neighbor] = index
                distances[neighbor] = distances[index] + 1
                pending.append(neighbor)
    return previous, distances


def build_route(spawn_pad: int, prop_targets: list[dict[str, object]],
                waypoints: list[dict[str, object]], pads: list[dict[str, object]]
                ) -> tuple[list[dict[str, object]], dict[int, list[dict[str, object]]]]:
    target_evidence: dict[int, list[dict[str, object]]] = {}
    for target in prop_targets:
        waypoint = nearest_waypoint(target["position"], waypoints, pads)
        target_evidence.setdefault(waypoint, []).append(target)
    spawn_waypoint = nearest_waypoint(pads[spawn_pad]["position"], waypoints, pads)
    unvisited = set(target_evidence)
    unvisited.discard(spawn_waypoint)
    route = [{"waypoint": spawn_waypoint, "segment": 0}]
    current = spawn_waypoint
    segment = 0
    while unvisited:
        previous, distances = shortest_paths(waypoints, current)
        reachable = unvisited.intersection(distances)
        if not reachable:
            target = min(unvisited, key=lambda value: (
                min(item["prop_index"] for item in target_evidence[value]), value))
            segment += 1
            route.append({"waypoint": target, "segment": segment})
            visited_targets = {target}
        else:
            target = min(reachable, key=lambda value: (distances[value], value))
            path = []
            cursor = target
            while cursor != current:
                path.append(cursor)
                cursor = previous[cursor]
            route.extend({"waypoint": value, "segment": segment}
                         for value in reversed(path))
            visited_targets = unvisited.intersection(path)
        current = target
        unvisited.difference_update(visited_targets)
    return route, target_evidence


def normalize_xz(x: float, z: float, fallback: list[float]) -> list[float]:
    length = math.hypot(x, z)
    if length <= 1e-9:
        x, z = -float(fallback[0]), -float(fallback[2])
        length = math.hypot(x, z)
    if length <= 1e-9:
        raise ValueError("authored route has no usable heading")
    return [x / length, 0.0, z / length]


def route_heading(route: list[dict[str, object]], index: int) -> list[float]:
    segment = route[index]["route_segment"]
    previous = index - 1
    following = index + 1
    if previous < 0 or route[previous]["route_segment"] != segment:
        previous = index
    if following >= len(route) or route[following]["route_segment"] != segment:
        following = index
    left = route[previous]["position"]
    right = route[following]["position"]
    return normalize_xz(float(right[0]) - float(left[0]),
                        float(right[2]) - float(left[2]), route[index]["look"])


def oriented_look(forward: list[float], direction: str) -> list[float]:
    if direction == "forward": return forward
    if direction == "back": return [-forward[0], 0.0, -forward[2]]
    if direction == "left": return [forward[2], 0.0, -forward[0]]
    if direction == "right": return [-forward[2], 0.0, forward[0]]
    raise ValueError(f"unsupported direction: {direction}")


def floor_y(points: list[list[float]], x: float, z: float) -> float:
    for first_index in range(len(points) - 2):
        first = points[first_index]
        for second_index in range(first_index + 1, len(points) - 1):
            second = points[second_index]
            for third in points[second_index + 1:]:
                a = [second[i] - first[i] for i in range(3)]
                b = [third[i] - first[i] for i in range(3)]
                normal = [a[1] * b[2] - a[2] * b[1],
                          a[2] * b[0] - a[0] * b[2],
                          a[0] * b[1] - a[1] * b[0]]
                if abs(normal[1]) > 1e-9:
                    plane = sum(normal[i] * first[i] for i in range(3))
                    return (plane - normal[0] * x - normal[2] * z) / normal[1]
    raise ValueError("authored STAN tile has no floor plane")


def generate(root: Path, stage_key: str, frames: int, directions: list[str],
             capacity: int) -> tuple[bytes, dict[str, object]]:
    inventory = load_inventory_module(root)
    catalog = stage_catalog(root, inventory)
    if stage_key not in catalog:
        raise ValueError(f"unknown stage {stage_key!r}; choices: {','.join(catalog)}")
    stage = catalog[stage_key]
    setup_bytes = stage["setup_path"].read_bytes()
    setup_text = setup_bytes.decode("utf-8")
    pads = [{**pad, "index": index, "stan_name": pad["stan_name"]}
            for index, pad in enumerate(inventory.parse_pads(setup_text))]
    bound_pads = parse_bound_pads(setup_text, inventory.initializer)
    if stage_key == "cuba":
        # The credits stage canonically has only the zero bound-pad sentinel
        # and no navigation graph. Its normal spawn is still an authored
        # ordinary pad, so a spawn-only view is the complete finite tour.
        waypoints = []
        props = []
        skipped_props = []
    else:
        waypoints = parse_waypoints(setup_text, pads, inventory.initializer)
        props, skipped_props = parse_prop_targets(
            setup_text, pads, bound_pads, inventory, inventory.initializer)
    spawns, _items = inventory.parse_intro_source(setup_text)
    normal = [spawn for spawn in spawns if spawn["demo_slot"] == 0]
    if len(normal) != 1:
        raise ValueError("setup does not have one canonical normal spawn")
    spawn_pad = normal[0]["pad_index"]
    collision_bytes = stage["collision_path"].read_bytes()
    collision = parse_collision(stage["collision_path"])
    if waypoints:
        route_path, target_evidence = build_route(
            spawn_pad, props, waypoints, pads)
    else:
        route_path, target_evidence = [], {}
    resolved_spawn = resolve_pad(pads[spawn_pad], collision)
    selected = [{**resolved_spawn, "waypoint": None, "waygroup": None,
                 "route_segment": "spawn", "target_props": []}]
    for item in route_path:
        waypoint_index = item["waypoint"]
        waypoint = waypoints[waypoint_index]
        pad = resolve_pad(pads[int(waypoint["pad"])], collision)
        selected.append({
            **pad, "waypoint": waypoint_index, "waygroup": waypoint["group"],
            "route_segment": f"component-{item['segment']}",
            "target_props": target_evidence.get(waypoint_index, []),
        })
    if len(selected) * len(directions) > capacity:
        raise ValueError(
            f"{stage_key} needs {len(selected) * len(directions)} views, "
            f"exceeding runtime capacity {capacity}; use fewer --directions")

    lines = ["GEVIEW1", "# frames room runtime-x runtime-y runtime-z "
             "look-x look-y look-z up-x up-y up-z label"]
    views = []
    scale = float(stage["level_scale"])
    for route_index, pad in enumerate(selected):
        raw_position = [float(value) for value in pad["position"]]
        raw_floor = floor_y(pad["stan_points"], raw_position[0], raw_position[2])
        position = [raw_position[0] / scale, raw_floor / scale + EYE_HEIGHT,
                    raw_position[2] / scale]
        authored_look = [float(value) for value in pad["look"]]
        base_look = route_heading(selected, route_index)
        up = [float(value) for value in pad["up"]]
        for direction in directions:
            look = oriented_look(base_look, direction)
            label = (f"{stage_key}-route-{route_index:03d}-"
                     f"pad-{int(pad['index']):03d}-{direction}")
            lines.append(f"{frames} {int(pad['room'])} "
                         + " ".join(f"{value:.9g}"
                                    for value in [*position, *look, *up])
                         + f" {label}")
            views.append({
                "label": label, "route_checkpoint": route_index,
                "route_segment": pad["route_segment"],
                "direction": direction, "pad": int(pad["index"]),
                "waypoint": pad["waypoint"], "waygroup": pad["waygroup"],
                "room": int(pad["room"]), "stan": pad["stan_name"],
                "stan_tile_index": pad["stan_tile_index"],
                "floor_y_raw": raw_floor, "position_raw": raw_position,
                "position_runtime": position, "authored_pad_look": authored_look,
                "look": look, "up": up, "hold_frames": frames,
                "target_props": pad["target_props"],
            })
    manifest = {
        "schema": 1, "format": "GEVIEW1", "stage": stage["label"],
        "stage_key": stage_key, "level_id": stage["level_id"],
        "level_id_symbol": stage["symbol"],
        "solo_sequence_index": stage["solo_sequence_index"],
        "decomp_keys": {"background": stage["background_key"],
                        "setup": stage["setup_key"], "stan": stage["stan_key"]},
        "level_scale": scale, "eye_height_runtime": EYE_HEIGHT,
        "selection": "spawn-and-prop-waypoint-route",
        "directions": directions, "normal_spawn_pad": spawn_pad,
        "pad_count": len(pads), "bound_pad_count": len(bound_pads),
        "waypoint_count": len(waypoints), "positioned_prop_count": len(props),
        "unpositioned_props": skipped_props,
        "target_waypoint_count": len(target_evidence),
        "route_component_count": 1 + max(
            (int(item["segment"]) for item in route_path), default=-1),
        "view_count": len(views), "total_frames": len(views) * frames,
        "runtime": {
            "tour_path": f"sdmc:/3ds/goldeneye-3ds/{stage_key}-visual-tour.geview",
            "result_path": f"sdmc:/3ds/goldeneye-3ds/{stage_key}-visual-tour.result",
            "stage_selection_path": "sdmc:/3ds/goldeneye-3ds/stage.cfg",
            "stage_selection_value": stage_key,
        },
        "source_sha256": {"setup": sha256(setup_bytes),
                          "collision": sha256(collision_bytes)},
        "views": views,
    }
    return ("\n".join(lines) + "\n").encode(), manifest


def main() -> int:
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stage", default="dam",
                        help="registered solo-stage runtime key")
    parser.add_argument("--root", type=Path, default=root)
    parser.add_argument("--frames", type=int, default=30)
    parser.add_argument("--directions", default="forward",
                        help="comma-separated forward,left,right,back")
    parser.add_argument("--capacity", type=int, default=DEFAULT_CAPACITY)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    directions = [value.strip() for value in args.directions.split(",")
                  if value.strip()]
    invalid = set(directions) - {"forward", "left", "right", "back"}
    if args.frames <= 0 or args.frames > 3600:
        parser.error("--frames must be in 1..3600")
    if not directions or invalid:
        parser.error(f"invalid --directions: {','.join(sorted(invalid))}")
    if args.capacity <= 0:
        parser.error("--capacity must be positive")
    output = args.output or Path(
        f"build/visual-probe/{args.stage}-authored.geview")
    manifest_path = args.manifest or Path(
        f"build/visual-probe/{args.stage}-authored.json")
    try:
        encoded, manifest = generate(args.root.resolve(), args.stage,
                                     args.frames, directions, args.capacity)
        atomic_write(output, encoded)
        atomic_write(manifest_path,
                     (json.dumps(manifest, indent=2, sort_keys=True) + "\n").encode())
    except (KeyError, OSError, UnicodeError, ValueError) as error:
        parser.error(str(error))
    print(f"generated {manifest['view_count']} authored {manifest['stage']} views / "
          f"{manifest['total_frames']} frames -> {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
