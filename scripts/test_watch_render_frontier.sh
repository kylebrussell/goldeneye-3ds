#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-watch-frontier.XXXXXX")
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_watch_render_slice.py" \
    "${repo_dir}" "${build_dir}/watch.c" \
    --report "${build_dir}/watch.json"

cc -std=c11 -Wall -Wextra -Werror \
    "${build_dir}/watch.c" \
    "${repo_dir}/port/tests/test_ge_original_watch_dispatch.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_watch"
"${build_dir}/test_watch"
python3 "${repo_dir}/scripts/tests/test_watch_render_frontier.py"
