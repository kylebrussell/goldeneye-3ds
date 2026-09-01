#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
stage=${1:-dam}
azahar_bin=${AZAHAR_BIN:-"${HOME}/Applications/Azahar.app/Contents/MacOS/azahar"}
if [[ ! -x "${azahar_bin}" && -x /Applications/Azahar.app/Contents/MacOS/azahar ]]; then
    azahar_bin=/Applications/Azahar.app/Contents/MacOS/azahar
fi
azahar_app=$(dirname "$(dirname "$(dirname "${azahar_bin}")")")
azahar_sd=${AZAHAR_SD:-"${HOME}/Library/Application Support/Azahar/sdmc"}
tour_frames=${GE_VISUAL_TOUR_FRAMES:-30}
tour_timeout_seconds=${GE_VISUAL_TOUR_TIMEOUT_SECONDS:-600}
tour_view_start=${GE_VISUAL_TOUR_VIEW_START:-0}
tour_view_count=${GE_VISUAL_TOUR_VIEW_COUNT:-0}
stage_dir="${repo_dir}/build/3ds-sd/3ds/goldeneye-3ds"
install_dir="${azahar_sd}/3ds/goldeneye-3ds"
tour_source="${repo_dir}/build/visual-probe/${stage}-authored.geview"
tour_manifest="${repo_dir}/build/visual-probe/${stage}-authored.json"
tour_installed="${install_dir}/${stage}-visual-tour.geview"
tour_result="${install_dir}/${stage}-visual-tour.result"
result_copy="${repo_dir}/build/visual-probe/${stage}-visual-tour.result"
tour_diagnostic="${install_dir}/${stage}-visual-tour.diag"
diagnostic_copy="${repo_dir}/build/visual-probe/${stage}-visual-tour.diag"
selection="${install_dir}/stage.cfg"
selection_backup="${repo_dir}/build/visual-probe/stage.cfg.before-tour"

if [[ ! -x "${azahar_bin}" ]]; then
    echo "Azahar executable not found: ${azahar_bin}" >&2
    exit 2
fi

cd "${repo_dir}"
tour_args=(--stage "${stage}" --frames "${tour_frames}"
           --output "${tour_source}" --manifest "${tour_manifest}")
if [[ -n "${GE_VISUAL_TOUR_DIRECTIONS:-}" ]]; then
    tour_args+=(--directions "${GE_VISUAL_TOUR_DIRECTIONS}")
fi
./scripts/generate_stage_visual_tour.py "${tour_args[@]}"
if ! [[ "${tour_view_start}" =~ ^[0-9]+$ \
        && "${tour_view_count}" =~ ^[0-9]+$ ]]; then
    echo "GE_VISUAL_TOUR_VIEW_START/COUNT must be non-negative integers" >&2
    exit 2
fi
if ((tour_view_start != 0 || tour_view_count != 0)); then
    tour_subset=$(mktemp "${repo_dir}/build/visual-probe/${stage}-subset.XXXXXX")
    awk -v start="${tour_view_start}" -v count="${tour_view_count}" '
        NR <= 2 { print; next }
        {
            view = NR - 3
            if (view >= start && (count == 0 || view < start + count)) print
        }
    ' "${tour_source}" > "${tour_subset}"
    mv "${tour_subset}" "${tour_source}"
    subset_views=$(awk 'NR > 2 { count++ } END { print count + 0 }' \
        "${tour_source}")
    if ((subset_views == 0)); then
        echo "visual-tour subset selected no views" >&2
        exit 2
    fi
    echo "selected ${subset_views} views starting at ${tour_view_start}"
fi
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
rm -f "${tour_result}" "${result_copy}" "${tour_diagnostic}" \
    "${diagnostic_copy}" "${selection_backup}"
if [[ -f "${selection}" ]]; then
    cp "${selection}" "${selection_backup}"
fi
printf '%s\n' "${stage}" > "${selection}"

cleanup() {
    if [[ -s "${tour_result}" ]]; then
        cp "${tour_result}" "${result_copy}"
    fi
    if [[ -s "${tour_diagnostic}" ]]; then
        cp "${tour_diagnostic}" "${diagnostic_copy}"
    fi
    rm -f "${tour_installed}" "${tour_result}" "${tour_diagnostic}"
    if [[ -f "${selection_backup}" ]]; then
        cp "${selection_backup}" "${selection}"
        rm -f "${selection_backup}"
    else
        rm -f "${selection}"
    fi
    if [[ -n "${azahar_pid:-}" ]] && kill -0 "${azahar_pid}" 2>/dev/null; then
        kill -TERM "${azahar_pid}" 2>/dev/null || true
        for _shutdown_poll in {1..20}; do
            kill -0 "${azahar_pid}" 2>/dev/null || break
            sleep 0.1
        done
        if kill -0 "${azahar_pid}" 2>/dev/null; then
            # Azahar treats SIGTERM as an emulation stop and can leave its
            # isolated game-list process alive. The tour result is already
            # closed at this point, so finish only that exact spawned PID.
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
    echo "Azahar is already running; close it before an isolated tour." >&2
    exit 2
fi
open -W -n "${azahar_app}" --args \
    -w "${install_dir}/goldeneye-3ds.3dsx" &
open_pid=$!
azahar_pid=""
for _attempt in {1..50}; do
    azahar_pid=$(pgrep -f '/Azahar.app/Contents/MacOS/azahar' | head -n 1 || true)
    [[ -n "${azahar_pid}" ]] && break
    sleep 0.1
done
if [[ -z "${azahar_pid}" ]]; then
    echo "could not identify the isolated Azahar process" >&2
    exit 1
fi
echo "${stage} authored tour is running unattended; press Ctrl+P for screenshots."
tour_poll_count=$((tour_timeout_seconds * 4))
for ((tour_poll = 0; tour_poll < tour_poll_count; ++tour_poll)); do
    [[ -s "${tour_result}" ]] && break
    if ! kill -0 "${azahar_pid}" 2>/dev/null; then
        break
    fi
    sleep 0.25
done
if [[ -s "${tour_result}" ]]; then
    cp "${tour_result}" "${result_copy}"
    echo "Verified tour result: ${result_copy}"
    cat "${result_copy}"
    if ! grep -q '^status=complete$' "${tour_result}"; then
        echo "Authored tour completed with camera, visibility, or streaming failures." >&2
        exit 1
    fi
else
    echo "No completed tour result was produced." >&2
    exit 1
fi
