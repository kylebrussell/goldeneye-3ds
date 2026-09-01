#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)

GE_INPUT_PROBE_CONFIG="${repo_dir}/scripts/dam-authored-firefight.cfg" \
GE_INPUT_PROBE_TIMEOUT_SECONDS="${GE_DAM_FIREFIGHT_PROBE_TIMEOUT_SECONDS:-360}" \
    "${repo_dir}/scripts/run_dam_input_probe.sh"
python3 "${repo_dir}/scripts/verify_dam_firefight_result.py" \
    "${repo_dir}/build/visual-probe/dam-input-probe.result"
