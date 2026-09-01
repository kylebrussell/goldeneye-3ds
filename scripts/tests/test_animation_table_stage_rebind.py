#!/usr/bin/env python3
"""Guard the canonical animation-table stage-rebind lifecycle."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SERVICES = (ROOT / "port/src/ge_original_gameplay_services.c").read_text()
TABLE = (ROOT / "port/src/ge_original_guard_animation_table.c").read_text()


def ordered(*needles: str) -> None:
    positions = [SERVICES.index(needle) for needle in needles]
    assert positions == sorted(positions), (needles, positions)


assert "ge_guard_animation_table_pointers_expanded" in SERVICES
ordered(
    "expand_ani_table_entries((s32 **)&animation_table_ptrs1)",
    "ge_original_guard_animation_materialize_direct_entry(",
    "initWeaponAnimGroups();",
)
assert "index < (size_t)ANIM_MAX" in SERVICES
assert "index < (size_t)AIRCRAFT_ANIMATION_MAX" in SERVICES
assert "entry - native_table_base < ge_animation_segment_size" in TABLE
assert "ge_port_guard_animation_resolve((uint32_t)record_offset)" in TABLE

print("animation table stage rebind lifecycle test passed")
