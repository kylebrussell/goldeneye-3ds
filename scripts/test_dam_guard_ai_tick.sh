#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-guard-ai.XXXXXX)
generated_c="${build_dir}/ge_original_dam_guard_ai_tick_slice.c"
support_c="${build_dir}/ge_original_dam_guard_ai_support_slice.c"
if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

python3 "${repo_dir}/scripts/extract_dam_guard_ai_tick_slice.py" \
    "${repo_dir}" "${generated_c}"
python3 "${repo_dir}/scripts/extract_dam_guard_ai_support_slice.py" \
    "${repo_dir}" "${support_c}"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_DAM_MISSION_FLOW_SLICE \
    -DVERSION_US -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    -c "${repo_dir}/src/game/chrai.c" -o "${build_dir}/chrai.o"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pragma-pack \
    -Wno-switch \
    -Wno-logical-op-parentheses -Wno-int-to-void-pointer-cast \
    -Wno-int-to-pointer-cast \
    -Wno-int-conversion \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAM_GUARD_AI_HOST_OFFSETS \
    -DGE_PORT_DAM_GUARD_AI_GRENADE_TEST \
    -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${generated_c}" \
    "${support_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_ai_tick.c" \
    "${build_dir}/chrai.o" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_guard_ai_tick"

"${build_dir}/test_ge_original_dam_guard_ai_tick"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-unused-function -Wno-pragma-pack \
    -Wno-logical-op-parentheses -Wno-int-to-pointer-cast \
    -Wno-incompatible-pointer-types -Wno-pointer-to-int-cast \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAM_GUARD_AI_HOST_OFFSETS \
    -DGE_PORT_DAM_GUARD_AI_SIGHT_TEST -DVERSION_US -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${support_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_sight.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_guard_sight"

"${build_dir}/test_ge_original_dam_guard_sight"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-pragma-pack -Wno-logical-op-parentheses \
    -Wno-int-to-pointer-cast -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-unused-function \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAM_GUARD_AI_HOST_OFFSETS \
    -DGE_PORT_DAM_GUARD_AI_NAVIGATION_TEST -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${support_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_navigation.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_guard_navigation"

"${build_dir}/test_ge_original_dam_guard_navigation"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -Wno-unused-variable -Wno-pragma-pack -Wno-logical-op-parentheses \
    -Wno-missing-braces -Wno-parentheses \
    -Wno-implicit-const-int-float-conversion \
    -Wno-unused-value -Wno-empty-body -Wno-pointer-bool-conversion \
    -Wno-uninitialized \
    -Wno-int-to-pointer-cast -Wno-incompatible-pointer-types \
    -Wno-pointer-to-int-cast -Wno-unused-function \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAM_GUARD_AI_HOST_OFFSETS \
    -DGE_PORT_DAM_GUARD_AI_STAND_ANIM_TEST \
    -DGE_PORT_DAM_GUARD_AI_ACTION_GRAPH_TEST -DREFRESH_NTSC \
    -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${support_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_stand_anim.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_guard_stand_anim"

"${build_dir}/test_ge_original_dam_guard_stand_anim"

python3 - "${repo_dir}" "${generated_c}" "${support_c}" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
support = Path(sys.argv[3]).read_text()
spec = importlib.util.spec_from_file_location(
    "guard_ai_extract", repo / "scripts/extract_dam_guard_ai_tick_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
setup = (repo / "assets/obseg/setup/UsetupdamZ.c").read_text()
chrai = (repo / "src/game/chrai.c").read_text()
chraction = (repo / "src/game/chraction.c").read_text()
for number in module.LISTS:
    assert module.data_text(setup, number) in generated
for name in module.CASES:
    assert module.case_text(chrai, name) in generated
assert module.function_text(chraction, "chrlvActionTick") in generated
support_spec = importlib.util.spec_from_file_location(
    "guard_ai_support",
    repo / "scripts/extract_dam_guard_ai_support_slice.py")
support_module = importlib.util.module_from_spec(support_spec)
support_spec.loader.exec_module(support_module)
sources = {
    "bg": (repo / "src/game/bg.c").read_text(),
    "bgfog": (repo / "src/game/bgfog.c").read_text(),
    "bondview": (repo / "src/game/bondview.c").read_text(),
    "chr": (repo / "src/game/chr.c").read_text(),
    "chrprop": (repo / "src/game/chrprop.c").read_text(),
    "chraction": (repo / "src/game/chraction.c").read_text(),
    "file": (repo / "src/game/file.c").read_text(),
    "model": (repo / "src/game/model.c").read_text(),
    "objective": (repo / "src/game/objective_status2.c").read_text(),
    "padhall": (repo / "src/game/padhalllv.c").read_text(),
    "propobj": (repo / "src/game/propobj.c").read_text(),
    "stan": (repo / "src/game/stan.c").read_text(),
}
for source, name in (support_module.FUNCTIONS +
                     support_module.HOST_ONLY_FUNCTIONS +
                     support_module.PRODUCTION_NAVIGATION_FUNCTIONS +
                     support_module.PRODUCTION_ROUTE_FUNCTIONS +
                     support_module.PRODUCTION_ACTION_FUNCTIONS +
                     support_module.PRODUCTION_STAND_ANIM_DEPENDENCIES +
                     support_module.PRODUCTION_SIGHT_FUNCTIONS +
                     support_module.PRODUCTION_ACTION_GRAPH_DEPENDENCIES):
    assert support_module.function_text(sources[source], name) in support
assert support_module.function_text(
    sources["bgfog"], "fogGetScaledFarFogIntensitySquared") in support
assert set(support_module.PRODUCTION_SIGHT_FUNCTIONS).isdisjoint(
    support_module.PRODUCTION_ACTION_GRAPH_DEPENDENCIES)
for source, name, renamed in support_module.PRODUCTION_STAND_ANIM_FUNCTIONS:
    assert support_module.function_text(sources[source], name) in support
    assert renamed in support
for source, name, renamed in support_module.PRODUCTION_REMAINING_ACTION_HANDLERS:
    assert support_module.function_text(sources[source], name) in support
    assert renamed in support
service_count = (len(support_module.FUNCTIONS)
                 + len(support_module.HOST_ONLY_FUNCTIONS)
                 + len(support_module.PRODUCTION_NAVIGATION_FUNCTIONS)
                 + len(support_module.PRODUCTION_ROUTE_FUNCTIONS)
                 + len(support_module.PRODUCTION_ACTION_FUNCTIONS)
                 + len(support_module.PRODUCTION_STAND_ANIM_DEPENDENCIES)
                 + len(support_module.PRODUCTION_STAND_ANIM_FUNCTIONS)
                 + len(support_module.PRODUCTION_GRENADE_FUNCTIONS)
                 + len(support_module.PRODUCTION_SIGHT_FUNCTIONS)
                 + len(support_module.PRODUCTION_REMAINING_ACTION_HANDLERS)
                 + len(support_module.PRODUCTION_ACTION_GRAPH_DEPENDENCIES))
print(f"Dam guard exactness: {len(module.LISTS)} authored lists, "
      f"{len(module.CASES)} interpreter cases, unchanged chrlvActionTick, "
      f"{service_count} exact services")
PY

for symbol in ge_original_dam_guard_ai_interpret_exact \
        ge_original_dam_guard_action_tick_exact \
        ge_original_dam_guard_ai_040d ge_original_dam_guard_ai_0413 \
        ge_original_dam_guard_ai_0414; do
    nm -g "${build_dir}/test_ge_original_dam_guard_ai_tick" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in ge_original_dam_guard_tick_stand_exact \
        ge_original_dam_guard_tick_anim_exact \
        ge_original_dam_guard_tick_kneel_exact \
        ge_original_dam_guard_tick_dead_exact \
        ge_original_dam_guard_tick_start_alarm_exact \
        ge_original_dam_guard_tick_bond_die_removed_exact; do
    nm -g "${build_dir}/test_ge_original_dam_guard_stand_anim" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in actor_draws_throws_grenade_at_player_if_possible \
        chrlvThrowGrenade chrGiveWeapon; do
    nm -g "${build_dir}/test_ge_original_dam_guard_ai_tick" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in chrCheckTargetInSight chrCanSeeBond chrSawTargetRecently \
        chrlvGetGuard007SpeedRatingInt fogGetScaledFarFogIntensitySquared; do
    nm -g "${build_dir}/test_ge_original_dam_guard_sight" \
        | grep -E "_?${symbol}$" >/dev/null
done

printf 'Dam guard retained symbols: interpreter, sight/LOS, action tick, representative exact handlers, 040d/0413/0414\n'
