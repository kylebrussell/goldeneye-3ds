#!/usr/bin/env bash
# Read-only: inspect a retained copy of an existing native campaign save.
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-save-inspector.XXXXXX")
trap 'rm -rf "${build_dir}"' EXIT
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-unused-parameter \
    -Wno-pointer-sign -Wno-missing-braces \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DVERSION_US \
    -I"${repo_dir}" -I"${repo_dir}/port/include" -I"${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/src" \
    -iquote "${repo_dir}" \
    "${repo_dir}/scripts/inspect_campaign_save.c" \
    "${repo_dir}/port/src/random_port.c" \
    -o "${build_dir}/inspect_campaign_save"
"${build_dir}/inspect_campaign_save" "$@"
