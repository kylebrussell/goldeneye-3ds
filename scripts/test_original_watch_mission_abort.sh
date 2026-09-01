#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "$0")/.." && pwd)"
test_dir="$(mktemp -d "${TMPDIR:-/tmp}/ge-watch-abort.XXXXXX")"
trap 'rm -rf "${test_dir}"' EXIT

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_watch_mission_abort.c" \
    "${repo_dir}/port/tests/test_ge_original_watch_mission_abort.c" \
    -o "${test_dir}/test_ge_original_watch_mission_abort"

"${test_dir}/test_ge_original_watch_mission_abort"

cc -std=c11 -Wall -Wextra -Werror -pedantic \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I"${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_original_watch_mission_abort.c" \
    "${repo_dir}/port/src/ge_original_watch_mission_abort_services.c" \
    "${repo_dir}/port/tests/test_ge_original_watch_mission_abort_services.c" \
    -o "${test_dir}/test_ge_original_watch_mission_abort_services"

"${test_dir}/test_ge_original_watch_mission_abort_services"
python3 "${repo_dir}/scripts/tests/test_watch_abort_live_wiring.py"
python3 "${repo_dir}/scripts/tests/test_facility_mission_exit_contract.py"
