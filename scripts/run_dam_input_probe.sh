#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
azahar_bin=${AZAHAR_BIN:-"${HOME}/Applications/Azahar.app/Contents/MacOS/azahar"}
if [[ ! -x "${azahar_bin}" && -x /Applications/Azahar.app/Contents/MacOS/azahar ]]; then
    azahar_bin=/Applications/Azahar.app/Contents/MacOS/azahar
fi
azahar_app=$(dirname "$(dirname "$(dirname "${azahar_bin}")")")
azahar_sd=${AZAHAR_SD:-"${HOME}/Library/Application Support/Azahar/sdmc"}
probe_frames=${GE_INPUT_PROBE_FRAMES:-300}
probe_active_frames=${GE_INPUT_PROBE_ACTIVE_FRAMES:-200}
probe_move_x=${GE_INPUT_PROBE_MOVE_X:-0.0}
probe_move_y=${GE_INPUT_PROBE_MOVE_Y:-1.0}
probe_look_x=${GE_INPUT_PROBE_LOOK_X:-0.0}
probe_look_y=${GE_INPUT_PROBE_LOOK_Y:-0.0}
probe_held=${GE_INPUT_PROBE_HELD:-0}
probe_source_config=${GE_INPUT_PROBE_CONFIG:-}
probe_timeout_seconds=${GE_INPUT_PROBE_TIMEOUT_SECONDS:-180}
stage_dir="${repo_dir}/build/3ds-sd/3ds/goldeneye-3ds"
install_dir="${azahar_sd}/3ds/goldeneye-3ds"
probe_config="${install_dir}/dam-input-probe.cfg"
probe_result="${install_dir}/dam-input-probe.result"
result_copy="${repo_dir}/build/visual-probe/dam-input-probe.result"

if [[ ! -x "${azahar_bin}" ]]; then
    echo "Azahar executable not found: ${azahar_bin}" >&2
    exit 2
fi
if pgrep -f '/Azahar.app/Contents/MacOS/azahar' >/dev/null; then
    echo "Azahar is already running; close it before an isolated input probe." >&2
    exit 2
fi

cd "${repo_dir}"
if [[ "${GE_INPUT_PROBE_SKIP_BUILD:-0}" != 1 ]]; then
    ./scripts/build_3ds.sh
    cp platform/3ds/goldeneye-3ds.3dsx \
       "${stage_dir}/goldeneye-3ds.3dsx"
fi
if [[ ! -f "${stage_dir}/goldeneye-3ds.3dsx" \
      || ! -f "${stage_dir}/goldeneye.u.gepack" ]]; then
    echo "staged 3DS executable/assets are missing" >&2
    exit 2
fi
mkdir -p "${install_dir}" "$(dirname "${result_copy}")"
cp "${stage_dir}/goldeneye-3ds.3dsx" \
   "${stage_dir}/goldeneye.u.gepack" "${install_dir}/"
if [[ -n "${probe_source_config}" ]]; then
    cp "${probe_source_config}" "${probe_config}"
else
    printf '%s\n' \
        'GE_INPUT_PROBE 1' \
        "frames ${probe_frames}" \
        "active_frames ${probe_active_frames}" \
        "move_x ${probe_move_x}" \
        "move_y ${probe_move_y}" \
        "look_x ${probe_look_x}" \
        "look_y ${probe_look_y}" \
        "held ${probe_held}" > "${probe_config}"
fi
rm -f "${probe_result}" "${result_copy}"

cleanup() {
    if [[ -s "${probe_result}" ]]; then
        cp "${probe_result}" "${result_copy}"
    fi
    rm -f "${probe_config}" "${probe_result}"
    if [[ -n "${azahar_pid:-}" ]] && kill -0 "${azahar_pid}" 2>/dev/null; then
        kill -TERM "${azahar_pid}" 2>/dev/null || true
        for _quit_attempt in {1..20}; do
            kill -0 "${azahar_pid}" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "${azahar_pid}" 2>/dev/null; then
            kill -KILL "${azahar_pid}" 2>/dev/null || true
        fi
        wait "${azahar_pid}" 2>/dev/null || true
    fi
    if [[ -n "${open_pid:-}" ]] && kill -0 "${open_pid}" 2>/dev/null; then
        kill -TERM "${open_pid}" 2>/dev/null || true
        wait "${open_pid}" 2>/dev/null || true
    fi
}
trap cleanup EXIT INT TERM

open -W -n "${azahar_app}" --args \
    -w "${install_dir}/goldeneye-3ds.3dsx" &
open_pid=$!
azahar_pid=""
for _attempt in {1..50}; do
    azahar_pid=$(pgrep -f '/Azahar.app/Contents/MacOS/azahar' \
        | head -n 1 || true)
    [[ -n "${azahar_pid}" ]] && break
    sleep 0.1
done
if [[ -z "${azahar_pid}" ]]; then
    echo "could not identify the isolated Azahar input-probe process" >&2
    exit 1
fi
if [[ -n "${probe_source_config}" ]]; then
    echo "Dam segmented exact-input probe is running from ${probe_source_config}."
else
    echo "Dam exact-input probe is running for ${probe_frames} displayed frames."
fi
deadline=$((SECONDS + probe_timeout_seconds))
while [[ ! -s "${probe_result}" ]] \
      && kill -0 "${open_pid}" 2>/dev/null \
      && (( SECONDS < deadline )); do
    sleep 0.25
done
if [[ ! -s "${probe_result}" ]]; then
    echo "Dam input probe did not produce a result." >&2
    exit 1
fi
cp "${probe_result}" "${result_copy}"
echo "Verified input probe result: ${result_copy}"
cat "${result_copy}"
grep -qx 'status=complete' "${result_copy}"
