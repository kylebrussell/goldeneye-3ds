#!/usr/bin/env python3
"""Validate the checked-in canonical input frontier and Dam provider fixture."""

from __future__ import annotations

import json
import hashlib
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


def load(relative: str) -> dict[str, object]:
    return json.loads((ROOT / relative).read_text())


def check_records(frontier: dict[str, object], key: str, unique_key: str) -> None:
    records = frontier[key]
    assert isinstance(records, list)
    symbols = [record["symbol"] for record in records]
    assert symbols == sorted(set(symbols))
    assert frontier[unique_key] == len(records)
    assert all(record["relocations"] > 0 for record in records)


def main() -> None:
    source = (ROOT / "src/game/bondview2.c").read_text()
    start = source.index("void bondviewProcessInput(")
    opening = source.index("{", start)
    depth = 0
    end = None
    for offset in range(opening, len(source)):
        if source[offset] == "{":
            depth += 1
        elif source[offset] == "}":
            depth -= 1
            if depth == 0:
                end = offset + 1
                break
    assert end is not None
    canonical_body = source[start:end].encode()
    assert len(canonical_body) == 50310
    assert hashlib.sha256(canonical_body).hexdigest() == (
        "24530978a8783de7a243f2212ef79a3c5f2b3a866d38c8c8beaa5f8b9ebd8705"
    )

    manifest = load("docs/generated/bondview_process_input_dependencies.json")
    assert manifest["format"] == "goldeneye-bondview-input-relocations-v1"
    assert len(manifest["object_sha256"]) == 64

    function = manifest["function"]
    assert function == {
        "end": 0xA2D8,
        "name": "bondviewProcessInput",
        "size": 0x2654,
        "text_offset": 0x7C84,
    }

    frontier = manifest["frontier"]
    check_records(frontier, "external_calls", "unique_external_calls")
    check_records(
        frontier, "external_data_symbols", "unique_external_data_symbols"
    )
    check_records(frontier, "internal_calls", "unique_internal_calls")
    check_records(frontier, "internal_data_symbols", "unique_internal_data_symbols")
    external_calls = {record["symbol"] for record in frontier["external_calls"]}
    assert {"joyGetStickX", "joyGetStickY", "joyGetButtons"} <= external_calls

    tranche = manifest["normal_dam_provider_tranche"]
    assert len(tranche["resolved_calls"]) == 8
    assert len(tranche["resolved_data_symbols"]) == 9
    assert tranche["unique_remaining_symbols"] == 65
    remaining_calls = {
        record["symbol"] for record in tranche["remaining_external_calls"]
    } | {record["symbol"] for record in tranche["remaining_internal_calls"]}
    assert {
        "gunTickGameplay",
        "bondviewUpdateSpeedForwards",
        "bondviewUpdateSpeedSideways",
        "bondviewCurrentPlayerUpdateSpeedTheta",
        "stanTestLineUnobstructed",
    } <= remaining_calls

    fixture = load("port/tests/data/bondview_input_normal_dam.json")
    state = fixture["canonical_assumptions"]
    assert fixture["fixture"] == "normal-single-player-dam"
    assert state["player_count"] == 1
    assert state["controls_enabled"] is True
    assert state["controls_locked"] is False
    assert state["controller_config_value"] == 0
    assert state["solo_watch_state_value"] == 0
    assert state["aim_control"] == 0
    assert state["bond_dead"] is False
    assert state["in_tank"] is False


if __name__ == "__main__":
    main()
