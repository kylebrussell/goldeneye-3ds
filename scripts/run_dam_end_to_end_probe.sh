#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
report_dir="${repo_dir}/build/visual-probe"
manifest="${GE_DAM_END_TO_END_MANIFEST:-${report_dir}/dam-end-to-end-authored.json}"
tour="${GE_DAM_END_TO_END_TOUR:-${report_dir}/dam-end-to-end-authored.geview}"
config="${GE_DAM_END_TO_END_CONFIG:-${report_dir}/dam-end-to-end.cfg}"
result="${report_dir}/dam-input-probe.result"

mkdir -p "${report_dir}"

# Derive every navigation stop and mission landmark from the decompiled Dam
# pad, waypoint and setup records.  The generated route carries controller
# input only; mission/objective state remains owned by the original runtime.
python3 "${repo_dir}/scripts/generate_dam_visual_tour.py" \
    --route objectives \
    --directions forward \
    --frames 1 \
    --output "${tour}" \
    --manifest "${manifest}"
python3 "${repo_dir}/scripts/generate_dam_end_to_end_route.py" \
    "${manifest}" "${config}" \
    --frames "${GE_DAM_END_TO_END_PROBE_FRAMES:-55000}" \
    --radius "${GE_DAM_END_TO_END_PROBE_RADIUS:-135}" \
    --opening-route "${repo_dir}/scripts/dam-authored-firefight.cfg" \
    --skip-route-targets 11

GE_INPUT_PROBE_CONFIG="${config}" \
GE_INPUT_PROBE_TIMEOUT_SECONDS="${GE_DAM_END_TO_END_PROBE_TIMEOUT_SECONDS:-1800}" \
    "${repo_dir}/scripts/run_dam_input_probe.sh"

python3 "${repo_dir}/scripts/verify_dam_end_to_end_result.py" "${result}"
