#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=${GE_STAGE_TOUR_TEST_DIR:-${repo_dir}/build/host-tests/stage-tours}
mkdir -p "${test_dir}"

tour_paths=()
for stage in runway silo cradle surface1 surface2; do
    tour="${test_dir}/${stage}.geview"
    python3 "${repo_dir}/scripts/generate_stage_visual_tour.py" \
        --stage "${stage}" --frames 1 --output "${tour}" \
        --manifest "${test_dir}/${stage}.json"
    tour_paths+=("${tour}")
done

python3 -m unittest \
    "${repo_dir}/scripts/tests/test_generate_stage_visual_tour.py"

cc -std=c11 -Wall -Wextra -Werror -pedantic -Wconversion \
    -Wsign-conversion -Wshadow \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_visual_probe_tour.c" \
    "${repo_dir}/port/tests/test_ge_visual_probe_tour.c" \
    -lm -o "${test_dir}/test_ge_visual_probe_tour"
"${test_dir}/test_ge_visual_probe_tour" "${tour_paths[@]}"
