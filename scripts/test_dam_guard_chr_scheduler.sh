#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-guard-scheduler.XXXXXX)
generated_c="${build_dir}/ge_original_dam_guard_chr_scheduler_slice.c"
support_c="${build_dir}/ge_original_dam_guard_chr_scheduler_support_slice.c"
full_support_c="${build_dir}/ge_original_dam_guard_chr_scheduler_full_props_support_slice.c"
trap 'rm -rf "${build_dir}"' EXIT

python3 "${repo_dir}/scripts/extract_dam_guard_chr_scheduler_slice.py" \
    "${repo_dir}" "${generated_c}"
python3 "${repo_dir}/scripts/extract_dam_guard_chr_scheduler_support_slice.py" \
    "${repo_dir}" "${support_c}"
python3 "${repo_dir}/scripts/extract_dam_guard_chr_scheduler_support_slice.py" \
    --full-props "${repo_dir}" "${full_support_c}"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DVERSION_US -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${generated_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_chr_scheduler.c" \
    -fsanitize=address,undefined -lm \
    -o "${build_dir}/test_ge_original_dam_guard_chr_scheduler"

"${build_dir}/test_ge_original_dam_guard_chr_scheduler"

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${support_c}" \
    "${repo_dir}/port/tests/test_ge_original_dam_guard_chr_scheduler_support.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_guard_chr_scheduler_support"

"${build_dir}/test_ge_original_dam_guard_chr_scheduler_support"

# Drive the unchanged complete autoaim dispatch through its real candidate,
# bounds, projection, LOS/same-STAN and target-publication ordering.  Each
# rejection boundary is then isolated without replacing the canonical body.
cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-unused-variable -Wno-unused-function \
    -Wno-unused-but-set-variable -Wno-incompatible-pointer-types \
    -Wno-empty-body -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -Wno-missing-braces -Wno-sign-compare \
    -Wno-uninitialized -Wno-unused-value \
    -Wno-implicit-const-int-float-conversion -Wno-pointer-bool-conversion \
    -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${full_support_c}" \
    "${repo_dir}/port/tests/test_ge_original_autoaim_pipeline.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_autoaim_pipeline"

"${build_dir}/test_ge_original_autoaim_pipeline"

python3 - "${repo_dir}" "${generated_c}" "${support_c}" \
        "${full_support_c}" <<'PY'
import importlib.util
import sys
from pathlib import Path

repo = Path(sys.argv[1])
generated = Path(sys.argv[2]).read_text()
support = Path(sys.argv[3]).read_text()
full_support = Path(sys.argv[4]).read_text()
spec = importlib.util.spec_from_file_location(
    "scheduler_extract",
    repo / "scripts/extract_dam_guard_chr_scheduler_slice.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
sources = {
    "chr": (repo / "src/game/chr.c").read_text(),
    "file": (repo / "src/game/file.c").read_text(),
    "chrprop": (repo / "src/game/chrprop.c").read_text(),
}
for name in module.CHR_FUNCTIONS:
    assert module.function_text(sources["chr"], name) in generated
for name in module.CHRPROP_FUNCTIONS:
    assert module.function_text(sources["chrprop"], name) in generated
support_spec = importlib.util.spec_from_file_location(
    "scheduler_support_extract",
    repo / "scripts/extract_dam_guard_chr_scheduler_support_slice.py")
sys.path.insert(0, str(repo / "scripts"))
support_module = importlib.util.module_from_spec(support_spec)
support_spec.loader.exec_module(support_module)
support_sources = {
    key: (repo / "src/game" / filename).read_text()
    for key, filename in support_module.SOURCE_FILES.items()
}
for source, name in support_module.FUNCTIONS:
    assert module.function_text(support_sources[source], name) in support
for source, name in support_module.FULL_PROPS_FUNCTIONS:
    assert module.function_text(support_sources[source], name) in full_support
print("Dam guard scheduler exactness: unchanged propsTick, chrTick, "
      f"chrUpdateAnim, chrDetectRooms and {len(support_module.FUNCTIONS)} "
      f"support/{len(support_module.FULL_PROPS_FUNCTIONS)} full-props "
      "bodies retained")
PY

for symbol in ge_original_dam_guard_props_tick_exact \
        ge_original_dam_guard_chr_tick_exact \
        ge_original_dam_guard_chr_update_anim_exact \
        ge_original_dam_guard_chr_detect_rooms_exact; do
    nm -g "${build_dir}/test_ge_original_dam_guard_chr_scheduler" \
        | grep -E "_?${symbol}$" >/dev/null
done

printf 'Dam guard scheduler: 4 guards, tail-to-head action then one animation tick per frame\n'
printf 'Dam guard scheduler support: exact aim, player shuffle, active-list delist and debug/model state\n'
printf 'Dam autoaim pipeline: exact candidate, projection, LOS/same-STAN and target publication\n'
