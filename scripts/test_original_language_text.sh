#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
test_dir=$(mktemp -d "${TMPDIR:-/tmp}/ge-language-text.XXXXXX")
trap 'rm -rf "${test_dir}"' EXIT

language_sources=(
    LdamE LarkE LrunE LsevxE LsevE LsiloE LdestE LsevxbE LsevbE
    LstatE LarchE LpeteE LdepoE LtraE LjunE LarecE LcaveE LcradE
    LaztE LcrypE LlenE LgunE LmiscE LoptionsE LpropobjE LtitleE
)
source_paths=()
for source in "${language_sources[@]}"; do
    source_paths+=("${repo_dir}/assets/obseg/text/${source}.c")
done

cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined \
    -fno-omit-frame-pointer -D_LANGUAGE_C \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/src" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    "${repo_dir}/port/src/ge_original_language_text.c" \
    "${repo_dir}/port/tests/test_ge_original_language_text.c" \
    "${source_paths[@]}" \
    -o "${test_dir}/test_ge_original_language_text"

"${test_dir}/test_ge_original_language_text"
