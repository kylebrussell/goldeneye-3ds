#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-guard-runtime.XXXXXX)
scheduler_c="${build_dir}/ge_original_dam_guard_chr_scheduler_slice.c"
action_c="${build_dir}/ge_original_dam_guard_ai_tick_slice.c"
support_c="${build_dir}/ge_original_dam_guard_ai_support_slice.c"
pose_c="${build_dir}/ge_original_gun_pose_helpers_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_dam_guard_chr_scheduler_slice.py" \
    "${repo_dir}" "${scheduler_c}"
python3 "${repo_dir}/scripts/extract_dam_guard_ai_tick_slice.py" \
    "${repo_dir}" "${action_c}"
python3 "${repo_dir}/scripts/extract_dam_guard_ai_support_slice.py" \
    "${repo_dir}" "${support_c}"
python3 "${repo_dir}/scripts/extract_gun_pose_helpers_slice.py" \
    "${repo_dir}" "${pose_c}"

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value -Wno-switch \
    -Wno-int-conversion \
    -Wno-logical-op-parentheses -Wno-parentheses \
    -Wno-implicit-const-int-float-conversion -ffunction-sections \
    -fdata-sections -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DGE_PORT_DAM_GUARD_SUSTAINED_TEST \
    -DGE_PORT_DAM_GUARD_AI_HOST_OFFSETS \
    -DGE_PORT_DAM_GUARD_AI_DEATH_DISPATCH_TEST \
    -DGE_PORT_DAM_GUARD_AI_ATTACK_DISPATCH_TEST \
    -DGE_PORT_DAM_GUARD_AI_ACTION_GRAPH_TEST \
    -DGE_PORT_DAM_GUARD_AI_GRENADE_TEST \
    -DGE_PORT_GUN_HOST_MODEL_ABI -DVERSION_US -DBUGFIX_R0 \
    -DPLAYERFLAG=int -DREFRESH_NTSC \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${scheduler_c}" "${action_c}" "${support_c}" "${pose_c}" \
    "${repo_dir}/port/src/ge_original_gun_frame_arena.c" \
    "${repo_dir}/port/src/ge_original_bond_input_provider.c" \
    "${repo_dir}/port/src/ge_original_dam_guard_runtime.c" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_chr_scheduler.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_dam_guard_runtime_integration"

"${build_dir}/test_dam_guard_runtime_integration"

for symbol in ge_original_dam_guard_props_tick_exact \
        ge_original_dam_guard_action_tick_exact \
        ge_original_dam_guard_tick_attack_exact \
        ge_original_dam_guard_tick_die_exact \
        ge_original_dam_guard_tick_dead_exact dynAllocate; do
    nm -g "${build_dir}/test_dam_guard_runtime_integration" \
        | grep -E "_?${symbol}$" >/dev/null
done

for symbol in actor_draws_throws_grenade_at_player_if_possible \
        chrlvThrowGrenade chrGiveWeapon; do
    nm -g "${build_dir}/test_dam_guard_runtime_integration" \
        | grep -E "_?${symbol}$" >/dev/null
done

printf '%s\n' \
    'Dam guard runtime integration: sustained exact scheduler/dispatcher, ACT_ATTACK fire, ACT_DIE -> ACT_DEAD, one tick per guard/frame, shared canonical frame arena'
