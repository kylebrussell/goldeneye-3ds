#!/usr/bin/env python3
"""Guard the canonical animation-table stage-rebind lifecycle."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SERVICES = (ROOT / "port/src/ge_original_gameplay_services.c").read_text()
TABLE = (ROOT / "port/src/ge_original_guard_animation_table.c").read_text()


def ordered(source: str, *needles: str) -> None:
    positions = [source.index(needle) for needle in needles]
    assert positions == sorted(positions), (needles, positions)


# Each ABI has its own materialization path and initialization call. A global
# first occurrence can mistake the wide branch for the legacy branch's tail.
reset = SERVICES.split("void ge_original_gameplay_services_reset(void)", 1)[1]
branches = reset.split("#ifdef GE_PORT_WIDE_ANIMATION_REFS\n", 1)[1]
wide, legacy = branches.split("#else\n", 1)
legacy = legacy.split("#endif\n", 1)[0]
ordered(
    wide,
    "ge_original_animation_table_refs_materialize()",
    "&& !ge_guard_animation_tables_initialized",
    "initWeaponAnimGroups();",
    "ge_guard_animation_tables_initialized = true;",
)
assert "ge_guard_animation_table_pointers_expanded" in SERVICES
ordered(
    legacy,
    "if (!ge_guard_animation_table_pointers_expanded)",
    "expand_ani_table_entries((s32 **)&animation_table_ptrs1)",
    "expand_ani_table_entries((s32 **)&animation_table_ptrs2)",
    "ge_guard_animation_table_pointers_expanded = true;",
    "ge_original_guard_animation_materialize_direct_entry(",
    "if (materialized && !ge_guard_animation_tables_initialized)",
    "initWeaponAnimGroups();",
    "ge_guard_animation_tables_initialized = true;",
)
assert "index < (size_t)ANIM_MAX" in legacy
assert "index < (size_t)AIRCRAFT_ANIMATION_MAX" in legacy
assert "entry - native_table_base < ge_animation_segment_size" in TABLE
assert "ge_port_guard_animation_resolve((uint32_t)record_offset)" in TABLE

print("animation table stage rebind lifecycle test passed")
