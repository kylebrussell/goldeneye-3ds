#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
manifest=${GE_DAM_MODEM_MANIFEST:-"${repo_dir}/build/visual-probe/dam-modem-authored.json"}
config=${GE_DAM_MODEM_CONFIG:-"${repo_dir}/build/visual-probe/dam-modem-objective.cfg"}
result="${repo_dir}/build/visual-probe/dam-input-probe.result"

if [[ ! -f "${manifest}" ]]; then
    echo "missing authored Dam modem route manifest: ${manifest}" >&2
    exit 2
fi

python3 "${repo_dir}/scripts/generate_dam_modem_route.py" \
    "${manifest}" "${config}" \
    --frames "${GE_DAM_MODEM_PROBE_FRAMES:-6000}" \
    --radius "${GE_DAM_MODEM_PROBE_RADIUS:-135}"

GE_INPUT_PROBE_CONFIG="${config}" \
GE_INPUT_PROBE_TIMEOUT_SECONDS="${GE_DAM_MODEM_PROBE_TIMEOUT_SECONDS:-360}" \
    "${repo_dir}/scripts/run_dam_input_probe.sh"

python3 "${repo_dir}/scripts/verify_dam_modem_result.py" "${result}"
