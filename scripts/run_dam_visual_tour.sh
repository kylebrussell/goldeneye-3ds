#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
azahar_bin=${AZAHAR_BIN:-"${HOME}/Applications/Azahar.app/Contents/MacOS/azahar"}
if [[ ! -x "${azahar_bin}" && -x /Applications/Azahar.app/Contents/MacOS/azahar ]]; then
    azahar_bin=/Applications/Azahar.app/Contents/MacOS/azahar
fi
azahar_app=$(dirname "$(dirname "$(dirname "${azahar_bin}")")")
azahar_sd=${AZAHAR_SD:-"${HOME}/Library/Application Support/Azahar/sdmc"}
tour_frames=${GE_VISUAL_TOUR_FRAMES:-30}
tour_timeout_seconds=${GE_VISUAL_TOUR_TIMEOUT_SECONDS:-180}
stage_dir="${repo_dir}/build/3ds-sd/3ds/goldeneye-3ds"
install_dir="${azahar_sd}/3ds/goldeneye-3ds"
tour_source="${repo_dir}/build/visual-probe/dam-authored.geview"
tour_manifest="${repo_dir}/build/visual-probe/dam-authored.json"
tour_installed="${install_dir}/dam-visual-tour.geview"
tour_result="${install_dir}/dam-visual-tour.result"
result_copy="${repo_dir}/build/visual-probe/dam-visual-tour.result"

if [[ ! -x "${azahar_bin}" ]]; then
    echo "Azahar executable not found: ${azahar_bin}" >&2
    exit 2
fi

cd "${repo_dir}"
tour_args=(--frames "${tour_frames}")
if [[ -n "${GE_VISUAL_TOUR_ROUTE:-}" ]]; then
    tour_args+=(--route "${GE_VISUAL_TOUR_ROUTE}")
fi
if [[ -n "${GE_VISUAL_TOUR_PAD:-}" ]]; then
    tour_args+=(--pad "${GE_VISUAL_TOUR_PAD}")
fi
if [[ -n "${GE_VISUAL_TOUR_DIRECTIONS:-}" ]]; then
    tour_args+=(--directions "${GE_VISUAL_TOUR_DIRECTIONS}")
fi
./scripts/generate_dam_visual_tour.py "${tour_args[@]}"
if [[ "${GE_VISUAL_SKIP_BUILD:-0}" != 1 ]]; then
    make 3ds-stage
fi
if [[ ! -f "${stage_dir}/goldeneye-3ds.3dsx" \
      || ! -f "${stage_dir}/goldeneye.u.gepack" ]]; then
    echo "staged 3DS executable/assets are missing" >&2
    exit 2
fi
mkdir -p "${install_dir}"
cp "${stage_dir}/goldeneye-3ds.3dsx" \
   "${stage_dir}/goldeneye.u.gepack" "${install_dir}/"
cp "${tour_source}" "${tour_installed}"
rm -f "${tour_result}" "${result_copy}"

cleanup() {
    if [[ -s "${tour_result}" ]]; then
        cp "${tour_result}" "${result_copy}"
    fi
    rm -f "${tour_installed}" "${tour_result}"
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

if pgrep -f '/Azahar.app/Contents/MacOS/azahar' >/dev/null; then
    echo "Azahar is already running; close it before an isolated tour capture." >&2
    exit 2
fi
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
    echo "could not identify the isolated Azahar capture process" >&2
    exit 1
fi
echo "Dam route tour is running; press Ctrl+P for Azahar screenshots."
echo "The isolated run will close automatically after its result is written."
deadline=$((SECONDS + tour_timeout_seconds))
while [[ ! -s "${tour_result}" ]] \
      && kill -0 "${open_pid}" 2>/dev/null \
      && (( SECONDS < deadline )); do
    sleep 0.25
done
if [[ -s "${tour_result}" ]]; then
    if [[ -n "${azahar_pid:-}" ]] \
          && kill -0 "${azahar_pid}" 2>/dev/null; then
        kill -TERM "${azahar_pid}" 2>/dev/null || true
        for _quit_attempt in {1..20}; do
            kill -0 "${azahar_pid}" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "${azahar_pid}" 2>/dev/null; then
            kill -KILL "${azahar_pid}" 2>/dev/null || true
        fi
    fi
elif (( SECONDS >= deadline )); then
    echo "Dam route tour timed out after ${tour_timeout_seconds}s." >&2
fi
wait "${open_pid}" 2>/dev/null || true
if [[ -s "${tour_result}" ]]; then
    cp "${tour_result}" "${result_copy}"
    echo "Verified tour result: ${result_copy}"
    cat "${result_copy}"
    awk -F= '
        $1 == "status" { complete = ($2 == "complete") }
        $1 == "native_dam_guard_tick_count" { guard_ticks = $2 + 0 }
        $1 == "native_dam_guard_rejected_tick_count" {
            guard_rejections = $2 + 0
        }
        $1 == "native_dam_guard_last_status" { guard_status = $2 + 0 }
        $1 == "native_dam_door_interaction_tick_count" {
            door_ticks = $2 + 0
        }
        $1 == "native_dam_mission_tick_count" { mission_ticks = $2 + 0 }
        $1 == "native_dam_alarm_count" { alarm_count = $2 + 0 }
        $1 == "native_dam_objective_count" { objective_count = $2 + 0 }
        $1 == "native_dam_objective_evaluation_ready_count" {
            objective_ready = $2 + 0
        }
        $1 == "native_dam_objective_evaluation_blocked_count" {
            objective_blocked = $2 + 0
        }
        $1 == "native_dam_full_props_activated" { full_props = $2 + 0 }
        END {
            if (!complete || guard_ticks < 1 || guard_rejections != 0 ||
                    guard_status != 0 || door_ticks < 1 ||
                    mission_ticks < 1 || alarm_count != 4 ||
                    objective_count != 4 || objective_ready != 4 ||
                    objective_blocked != 0 || full_props != 1) {
                print "Dam canonical-runtime regression: guard_ticks=" \
                    guard_ticks ", guard_rejections=" guard_rejections \
                    ", guard_status=" guard_status ", door_ticks=" \
                    door_ticks ", mission_ticks=" mission_ticks \
                    ", alarms=" alarm_count ", objectives=" \
                    objective_ready "/" objective_count ", blocked=" \
                    objective_blocked \
                    ", full_props=" full_props > "/dev/stderr"
                exit 1
            }
        }
    ' "${result_copy}"
else
    echo "No completed tour result was produced." >&2
    exit 1
fi
