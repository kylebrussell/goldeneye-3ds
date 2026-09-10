#!/usr/bin/env bash

# Runs a scripted frontend (menu) traversal in an isolated Azahar instance and
# collects the per-frame timing result plus framebuffer captures.
#
#   scripts/run_frontend_probe.sh [probe.cfg]
#
# Environment:
#   GE_FRONTEND_PROBE_SKIP_BUILD=1   reuse the staged executable
#   GE_FRONTEND_PROBE_TIMEOUT_SECONDS  default 240
#   GE_FRONTEND_PROBE_LABEL          result/capture directory label

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
azahar_bin=${AZAHAR_BIN:-"${HOME}/Applications/Azahar.app/Contents/MacOS/azahar"}
if [[ ! -x "${azahar_bin}" && -x /Applications/Azahar.app/Contents/MacOS/azahar ]]; then
    azahar_bin=/Applications/Azahar.app/Contents/MacOS/azahar
fi
azahar_app=$(dirname "$(dirname "$(dirname "${azahar_bin}")")")
azahar_sd=${AZAHAR_SD:-"${HOME}/Library/Application Support/Azahar/sdmc"}
probe_source_config=${1:-"${repo_dir}/scripts/frontend-probes/solo-dam-start.cfg"}
probe_timeout_seconds=${GE_FRONTEND_PROBE_TIMEOUT_SECONDS:-240}
label=${GE_FRONTEND_PROBE_LABEL:-$(basename "${probe_source_config}" .cfg)}
stage_dir="${repo_dir}/build/3ds-sd/3ds/goldeneye-3ds"
install_dir="${azahar_sd}/3ds/goldeneye-3ds"
probe_config="${install_dir}/frontend-probe.cfg"
probe_result="${install_dir}/frontend-probe.result"
gameplay_result="${install_dir}/dam-input-probe.result"
capture_dir="${install_dir}/captures"
out_dir="${repo_dir}/build/frontend-probe/${label}"

if [[ ! -x "${azahar_bin}" ]]; then
    echo "Azahar executable not found: ${azahar_bin}" >&2
    exit 2
fi
if pgrep -f '/Azahar.app/Contents/MacOS/azahar' >/dev/null; then
    echo "Azahar is already running; close it before an isolated probe." >&2
    exit 2
fi
if [[ ! -f "${probe_source_config}" ]]; then
    echo "Frontend probe script not found: ${probe_source_config}" >&2
    exit 2
fi

cd "${repo_dir}"
if [[ "${GE_FRONTEND_PROBE_SKIP_BUILD:-0}" != 1 ]]; then
    ./scripts/build_3ds.sh
    cp platform/3ds/goldeneye-3ds.3dsx "${stage_dir}/goldeneye-3ds.3dsx"
fi
if [[ ! -f "${stage_dir}/goldeneye-3ds.3dsx" \
      || ! -f "${stage_dir}/goldeneye.u.gepack" ]]; then
    echo "staged 3DS executable/assets are missing" >&2
    exit 2
fi
mkdir -p "${install_dir}" "${out_dir}"
cp "${stage_dir}/goldeneye-3ds.3dsx" "${install_dir}/"
if ! cmp -s "${stage_dir}/goldeneye.u.gepack" "${install_dir}/goldeneye.u.gepack"; then
    cp "${stage_dir}/goldeneye.u.gepack" "${install_dir}/"
fi
gameplay_frames=$(awk '$1=="gameplay_frames"{print $2}' "${probe_source_config}" || true)
backup_dir=$(mktemp -d "${install_dir}/.frontend-probe-backup.XXXXXX")
saved_paths=()
probe_installed=0

cleanup() {
    local original_status=$?
    local cleanup_failed=0
    local saved
    trap - EXIT INT TERM
    # Cleanup must continue through copy/removal failures. In particular, a
    # full output disk must never leave the emulator running or hide backups.
    set +e
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
    # Collect only this run's files, after the emulator has stopped writing.
    # If moving originals aside failed, remaining originals are not results.
    if [[ "${probe_installed}" == 1 ]]; then
        if [[ -s "${probe_result}" ]]; then
            cp "${probe_result}" "${out_dir}/frontend-probe.result" || cleanup_failed=1
        fi
        if [[ -s "${gameplay_result}" ]]; then
            cp "${gameplay_result}" "${out_dir}/gameplay.result" || cleanup_failed=1
        fi
        if [[ -d "${capture_dir}" ]]; then
            if mkdir -p "${out_dir}/captures"; then
                for capture in "${capture_dir}"/*.ppm; do
                    [[ -f "${capture}" ]] || continue
                    cp "${capture}" "${out_dir}/captures/" || cleanup_failed=1
                done
            else
                cleanup_failed=1
            fi
        fi
        rm -f "${probe_config}" "${probe_result}" "${gameplay_result}" || cleanup_failed=1
        rm -rf "${capture_dir}" || cleanup_failed=1
    fi
    # Restore every original even when collection or another restore failed.
    # Do not move a directory into an uncleared destination directory.
    # The guarded expansion also supports macOS Bash 3.2 with set -u and an
    # empty saved_paths array (a first-ever probe has nothing to restore).
    for saved in ${saved_paths[@]+"${saved_paths[@]}"}; do
        if [[ -e "${install_dir}/${saved}" || -L "${install_dir}/${saved}" ]]; then
            echo "Cannot restore occupied probe path: ${install_dir}/${saved}" >&2
            cleanup_failed=1
        elif ! mv "${backup_dir}/${saved}" "${install_dir}/${saved}"; then
            cleanup_failed=1
        fi
    done
    if ! rmdir "${backup_dir}"; then
        echo "Probe backup retained for recovery: ${backup_dir}" >&2
        cleanup_failed=1
    fi
    if [[ "${cleanup_failed}" != 0 ]]; then
        echo "Probe cleanup encountered errors; inspect the paths above." >&2
        if [[ "${original_status}" == 0 ]]; then original_status=1; fi
    fi
    exit "${original_status}"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Other diagnostics would bypass the frontend or select a stage. Temporarily
# move them aside, including any previous result/capture files, transactionally.
shopt -s nullglob
for existing in "${install_dir}/dam-input-probe.cfg" \
        "${install_dir}/stage.cfg" "${install_dir}/frontend-visual-probe.cfg" \
        "${install_dir}/probe-detail.cfg" "${install_dir}/probe-retraces.cfg" \
        "${install_dir}/world-profile.cfg" "${install_dir}"/*-visual-tour.geview \
        "${probe_config}" "${probe_result}" "${gameplay_result}" "${capture_dir}"; do
    if [[ -e "${existing}" || -L "${existing}" ]]; then
        mv "${existing}" "${backup_dir}/"
        saved_paths+=("$(basename "${existing}")")
    fi
done
shopt -u nullglob
probe_installed=1
cp "${probe_source_config}" "${probe_config}"

open -W -n "${azahar_app}" --args -w "${install_dir}/goldeneye-3ds.3dsx" &
open_pid=$!
azahar_pid=""
for _attempt in {1..50}; do
    azahar_pid=$(pgrep -f '/Azahar.app/Contents/MacOS/azahar' | head -n 1 || true)
    [[ -n "${azahar_pid}" ]] && break
    sleep 0.1
done
if [[ -z "${azahar_pid}" ]]; then
    echo "could not identify the isolated Azahar frontend-probe process" >&2
    exit 1
fi
echo "Frontend probe ${label} is running from ${probe_source_config}."
deadline=$((SECONDS + probe_timeout_seconds))
# Wait for the frontend result, then (if requested) the gameplay result.
while [[ ! -s "${probe_result}" ]] && kill -0 "${open_pid}" 2>/dev/null \
      && (( SECONDS < deadline )); do
    sleep 0.25
done
if [[ -n "${gameplay_frames}" && "${gameplay_frames}" != 0 ]]; then
    while [[ ! -s "${gameplay_result}" ]] && kill -0 "${open_pid}" 2>/dev/null \
          && (( SECONDS < deadline )); do
        sleep 0.25
    done
fi
# Captures scheduled on the last frames may still be pending; give the
# running frontend/gameplay a moment before the emulator is stopped.
sleep 1
if [[ ! -s "${probe_result}" ]]; then
    echo "Frontend probe did not produce a result." >&2
    exit 1
fi
cp "${probe_result}" "${out_dir}/frontend-probe.result"
echo "Frontend probe result: ${out_dir}/frontend-probe.result"
python3 "${repo_dir}/scripts/analyze_3ds_frontend_probe.py" \
    "${out_dir}/frontend-probe.result"
grep -qx 'status=complete' "${out_dir}/frontend-probe.result"
if [[ -n "${gameplay_frames}" && "${gameplay_frames}" != 0 ]]; then
    if [[ ! -s "${gameplay_result}" ]] \
            || ! grep -qx 'status=complete' "${gameplay_result}"; then
        echo "Frontend completed, but requested gameplay probe did not complete." >&2
        exit 1
    fi
fi
