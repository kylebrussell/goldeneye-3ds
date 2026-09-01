#!/usr/bin/env python3
"""Lock the native gunbarrel model adapter to title.c's canonical contract."""

from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
TITLE = (ROOT / "src/game/title.c").read_text(encoding="utf-8")
ADAPTER = (ROOT / "port/src/ge_original_gunbarrel_bond.c").read_text(
    encoding="utf-8"
)
MAP = (ROOT / "build/u/ge007.u.map").read_text(encoding="utf-8")


def require(source: str, pattern: str, label: str) -> re.Match[str]:
    found = re.search(pattern, source, re.MULTILINE | re.DOTALL)
    if found is None:
        raise SystemExit(f"missing canonical {label}")
    return found


require(MAP, r"0x0*4144\s+ANIM_DATA_bond_eye_walk", "walk offset")
require(MAP, r"0x0*4298\s+ANIM_DATA_bond_eye_fire", "fire offset")
for value, name in ((137, "start"), (212, "speedup"), (230, "shot")):
    require(TITLE, rf"#define BOND_EYE_[A-Z_]+\s+{value}\b", name)

canonical_order = [
    "modelTickAnim(chrModelInstance, 1, 1)",
    "subcalcpos(chrModelInstance)",
    "subcalcmatrices(&renderData, chrModelInstance)",
    "instcalcmatrices(&renderData, gunModelInstance)",
]
adapter_order = [
    "modelTickAnim(bond->body, 1, 1)",
    "subcalcpos(bond->body)",
    "subcalcmatrices(&render_data, bond->body)",
    "instcalcmatrices(&render_data, bond->gun)",
]
last = -1
for call in canonical_order:
    position = TITLE.find(call)
    if position <= last:
        raise SystemExit(f"canonical ordering drift: {call}")
    last = position
last = -1
for call in adapter_order:
    position = ADAPTER.find(call)
    if position <= last:
        raise SystemExit(f"adapter ordering drift: {call}")
    last = position

for exact in (
    "GE_GUNBARREL_WALK_ANIMATION_OFFSET UINT32_C(0x4144)",
    "GE_GUNBARREL_FIRE_ANIMATION_OFFSET UINT32_C(0x4298)",
    "GE_GUNBARREL_ANIMATION_START_TICK UINT32_C(137)",
    "GE_GUNBARREL_ANIMATION_SPEEDUP_TICK UINT32_C(212)",
    "bond->gun->attachedto_objinst = bond->body->obj->Switches[3]",
    "modelSetAnimSpeed(bond->body, 1.6f, 8.0f)",
):
    if exact not in ADAPTER:
        raise SystemExit(f"adapter contract drift: {exact}")

print("canonical gunbarrel Bond body/head/weapon contract passes")
