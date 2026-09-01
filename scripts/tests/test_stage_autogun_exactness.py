#!/usr/bin/env python3
"""Verify the live beam-age owner retains the unchanged decompiled body."""

import importlib.util
import sys
from pathlib import Path

repo = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location(
    "extract", repo / "scripts/extract_dam_guard_chr_scheduler_slice.py")
module = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(module)

canonical = module.function_text(
    (repo / "src/game/gunfire.c").read_text(), "gunAdvanceBeamTimer")
retained = (repo / "port/src/ge_original_stage_autogun_beam_exact.c").read_text()
assert canonical in retained
assert len(sys.argv) == 2
full_props = Path(sys.argv[1]).read_text()
propobj = (repo / "src/game/propobj.c").read_text()
gunfire = (repo / "src/game/gunfire.c").read_text()
renderer = (repo / "port/src/ge_3ds_original_autogun_beam.c").read_text()
renderer_header = (
    repo / "port/include/ge_3ds_original_autogun_beam.h").read_text()
lifecycle = (
    repo / "port/src/ge_original_stage_autogun_lifecycle.c").read_text()
pitem_cleanup = (
    repo / "port/src/ge_original_stage_autogun_pitem_cleanup.c").read_text()
pitem_models = (repo / "port/src/ge_original_pitem_models.c").read_text()
main_3ds = (repo / "platform/3ds/source/main.c").read_text()
oddtextures = (repo / "assets/oddtextures.c").read_text()
assert module.function_text(propobj, "objTick") in full_props
assert module.function_text(propobj, "objFree") in full_props
for required in (
    "GUN_B9_CANNON_SHORT_SFX",
    "bondviewCallRecordDamageKills",
    "stanTestLineUnobstructed",
    "beam_local->weaponnum = ITEM_FNP90",
    "sndDeactivate(record->unkC4)",
    "sndDeactivate(record->unkC8)",
):
    assert required in full_props
canonical_renderer = module.function_text(gunfire, "sub_GAME_7F061E18")
for required in (
    "radius = 30.0f",
    "startoffset = flash->unk28",
    "dist = flash->unk24",
    "matrix_scalar_multiply(0.1f",
    "right.f[0] * 0.9f",
    "image = flareimage3",
):
    assert required in canonical_renderer
for required in (
    "GE_ORIGINAL_AUTOGUN_BEAM_RADIUS = 30",
    "(float)(int16_t)value",
    "beam->direction[axis] * distance * 10.0f",
    "right[axis] * 0.9f",
    "local[2] * 0.1f",
    "beam->maximum_distance < start_offset + distance",
):
    assert required in renderer
assert '"FLAREORANGELINE.bin"' in renderer_header
assert "s_flareimage3[]" in oddtextures
assert "IMAGE_FLAREORANGELINE, 0x10, 0x20" in oddtextures
for forbidden in (
    "objTick(",
    "gunAdvanceBeamTimer(",
    "randomGetNext(",
    "bondviewCallRecordDamageKills(",
    "sndPlaySfx(",
):
    assert forbidden not in renderer
for forbidden in (
    "ge_original_stage_autogun_lifecycle_tick_exact(",
    "ge_original_stage_autogun_lifecycle_advance_beam_exact(",
):
    assert forbidden not in main_3ds
owned_cleanup = lifecycle[
    lifecycle.index("ge_original_stage_autogun_lifecycle_cleanup_owned_exact"):
    lifecycle.index("const char *ge_original_stage_autogun_lifecycle_status_name")
]
assert owned_cleanup.index("objFree(") < owned_cleanup.index(
    "providers->release_model(")
pitem_release = module.function_text(
    pitem_models, "ge_original_pitem_model_release_instance")
for required in (
    "!instance->active || &instance->model != model_instance",
    "free(instance->render_positions)",
    "free(instance->rwdata)",
    "memset(instance, 0, sizeof(*instance))",
):
    assert required in pitem_release
for required in (
    "ge_original_pitem_model_release_instance(",
    "ge_original_stage_autogun_lifecycle_cleanup_owned_exact(",
):
    assert required in pitem_cleanup
assert "(int (*" not in pitem_cleanup
print("autogun lifecycle exactness: unchanged objTick tracking/fire/damage/SFX, "
      "gunAdvanceBeamTimer and equivalent FNP90 beam geometry retained; "
      "3DS frame loop does not double tick; objFree precedes one stable-address "
      "Pitem release")
