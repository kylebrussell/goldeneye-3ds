#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d /tmp/ge-stage-autogun-lifecycle.XXXXXX)
trap 'rm -rf "${test_dir}"' EXIT
full_props="${test_dir}/ge_original_full_props.c"

python3 "${repo_dir}/scripts/extract_dam_guard_chr_scheduler_support_slice.py" \
    --full-props "${repo_dir}" "${full_props}"

cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -DVERSION_US \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_original_stage_autogun_lifecycle.c" \
    "${repo_dir}/port/src/ge_original_stage_autogun_lifecycle.c" \
    "${repo_dir}/port/src/ge_original_stage_autogun_pitem_cleanup.c" \
    "${repo_dir}/port/src/ge_original_stage_autogun_beam_exact.c" \
    -lm -o "${test_dir}/test_ge_original_stage_autogun_lifecycle"

"${test_dir}/test_ge_original_stage_autogun_lifecycle"

cc -std=gnu11 -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS \
    -fms-extensions -I"${repo_dir}" -I"${repo_dir}/port/include" \
    -I"${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/port/tests/test_ge_3ds_original_autogun_beam.c" \
    "${repo_dir}/port/src/ge_3ds_original_autogun_beam.c" \
    -lm -o "${test_dir}/test_ge_3ds_original_autogun_beam"

"${test_dir}/test_ge_3ds_original_autogun_beam"
python3 "${repo_dir}/scripts/tests/test_stage_autogun_exactness.py" \
    "${full_props}"

for symbol in ge_original_stage_autogun_lifecycle_construct \
        ge_original_stage_autogun_lifecycle_is_live \
        ge_original_stage_autogun_lifecycle_tick_exact \
        ge_original_stage_autogun_lifecycle_advance_beam_exact \
        ge_original_stage_autogun_lifecycle_beam_snapshot \
        ge_original_stage_autogun_lifecycle_runtime_snapshot \
        ge_original_stage_autogun_lifecycle_cleanup_exact \
        ge_original_stage_autogun_lifecycle_cleanup_owned_exact \
        ge_original_stage_autogun_lifecycle_cleanup_pitem_exact \
        ge_3ds_original_autogun_beams_build_draw_list \
        gunAdvanceBeamTimer; do
    binary="${test_dir}/test_ge_original_stage_autogun_lifecycle"
    if [[ "${symbol}" == ge_3ds_original_autogun_beams_build_draw_list ]]; then
        binary="${test_dir}/test_ge_3ds_original_autogun_beam"
    fi
    nm -g "${binary}" \
        | grep -E "_?${symbol}$" >/dev/null
done

printf 'Stage autogun lifecycle: exact object delegation, beam aging, renderer publication and cleanup ownership\n'
