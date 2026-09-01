#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
frames=${GE_VISUAL_TOUR_FRAMES:-1}
timeout_seconds=${GE_VISUAL_TOUR_TIMEOUT_SECONDS:-600}
reuse_results=${GE_VISUAL_ALL_REUSE_RESULTS:-0}
summary="${repo_dir}/build/visual-probe/all-stage-visual-tours.tsv"
stages=(
    dam facility runway surface1 bunker1 silo frigate surface2 bunker2
    statue archives streets depot train jungle control cradle caverns aztec
    egyptian cuba
)

cd "${repo_dir}"
if [[ "${GE_VISUAL_ALL_SKIP_BUILD:-0}" != 1 ]]; then
    make 3ds-stage
fi

for stage in "${stages[@]}"; do
    result="${repo_dir}/build/visual-probe/${stage}-visual-tour.result"
    if [[ "${reuse_results}" == 1 && -s "${result}" ]] \
            && grep -q '^status=complete$' "${result}"; then
        echo "Reusing successful ${stage} visual-tour result."
        continue
    fi
    GE_VISUAL_TOUR_FRAMES="${frames}" \
    GE_VISUAL_TOUR_TIMEOUT_SECONDS="${timeout_seconds}" \
    GE_VISUAL_SKIP_BUILD=1 \
        ./scripts/run_stage_visual_tour.sh "${stage}"
done

mkdir -p "$(dirname "${summary}")"
{
    printf 'stage\tviews\tresident_peak\ttexture_peak\tvisible_room_peak\tstream_successes\tactor_status\tmaterialized\tmaterialize_failures\tmission_actors\tobjective_ready\tobjective_blocked\n'
    for stage in "${stages[@]}"; do
        result="${repo_dir}/build/visual-probe/${stage}-visual-tour.result"
        if [[ ! -s "${result}" ]] || ! grep -q '^status=complete$' "${result}"; then
            echo "Missing or failed visual-tour result for ${stage}" >&2
            exit 1
        fi
        value() {
            sed -n "s/^$1=//p" "${result}"
        }
        actor_status=$(value native_actor_tick_status)
        materialize_failures=$(value native_actor_materializer_failed_count)
        objective_blocked=$(value native_objective_evaluation_blocked_count)
        if [[ "${actor_status}" != 1 || "${materialize_failures}" != 0 \
                || "${objective_blocked}" != 0 ]]; then
            echo "Essential runtime validation failed for ${stage}: actor=${actor_status}, materializer_failures=${materialize_failures}, objective_blocked=${objective_blocked}" >&2
            exit 1
        fi
        printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
            "${stage}" "$(value views)" "$(value resident_peak)" \
            "$(value texture_peak)" "$(value visible_room_peak)" \
            "$(value stream_successes)" "${actor_status}" \
            "$(value native_actor_materializer_constructed_count)" \
            "${materialize_failures}" \
            "$(value native_mission_actor_count)" \
            "$(value native_objective_evaluation_ready_count)" \
            "${objective_blocked}"
    done
} > "${summary}"

echo "All 21 authored solo/credits-stage visual tours passed: ${summary}"
