#!/usr/bin/env python3
"""Pin guard lighting to the unchanged chrTick owner in the live loop."""

from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def main() -> None:
    source = (REPO / "platform/3ds/source/main.c").read_text()
    tick = source.index("ge_original_stage_active_props_tick_exact(")
    matrix = source.index(
        "ge_original_stage_guard_runtime_update_matrices(", tick)
    body = source[tick:matrix]
    assert "ge_original_stage_guard_runtime_update_lighting(" not in body
    assert "The unchanged chrTick in propsTick" in body
    assert "GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK" in body
    print("3DS guard lighting: unchanged chrTick is the sole live sample/step owner")


if __name__ == "__main__":
    main()
