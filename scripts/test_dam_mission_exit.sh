#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(mktemp -d /tmp/ge-dam-mission-exit.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT

if [[ "$(uname -s)" == Darwin ]]; then
    dead_strip=(-Wl,-dead_strip)
else
    dead_strip=(-Wl,--gc-sections)
fi

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pragma-pack \
    -Wno-unused-parameter -Wno-incompatible-pointer-types \
    -Wno-int-conversion -Wno-pointer-to-int-cast \
    -Wno-int-to-pointer-cast -ffunction-sections -fdata-sections \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DAIPARSE \
    -DGE_PORT_SETUP_DATA -DGE_PORT_MS_INHERITS -fms-extensions \
    -DVERSION_US -DBUGFIX_R0 -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" -iquote "${repo_dir}" \
    "${repo_dir}/port/src/ge_original_mission_result.c" \
    "${repo_dir}/port/src/ge_original_dam_mission_exit_services.c" \
    "${repo_dir}/port/tests/test_ge_original_dam_mission_exit_services.c" \
    -fsanitize=address,undefined "${dead_strip[@]}" -lm \
    -o "${build_dir}/test_ge_original_dam_mission_exit_services"

"${build_dir}/test_ge_original_dam_mission_exit_services"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pointer-sign \
    -Wno-incompatible-pointer-types -Wno-int-conversion \
    -Wno-unused-but-set-variable -Wno-unused-variable \
    -Wno-pointer-to-int-cast \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DGE_PORT_SETUP_DATA \
    -DGE_PORT_BLOOD_DECODE_SLICE -DGE_PORT_MS_INHERITS -DAIPARSE \
    -DVERSION_US -DPLAYERFLAG=int -fms-extensions \
    -I "${repo_dir}" -I "${repo_dir}/src/game" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/src/game/blood_animation.c" \
    "${repo_dir}/src/game/blood_decrypt.c" \
    "${repo_dir}/port/tests/test_ge_original_blood_decode.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_ge_original_blood_decode"

"${build_dir}/test_ge_original_blood_decode"

cc -std=c11 -Wall -Wextra -Werror \
    -I "${repo_dir}/port/include" \
    "${repo_dir}/port/src/ge_3ds_fade_overlay.c" \
    "${repo_dir}/port/tests/test_ge_3ds_fade_overlay.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer -lm \
    -o "${build_dir}/test_ge_3ds_fade_overlay"

"${build_dir}/test_ge_3ds_fade_overlay"

cc -std=gnu11 -Wall -Wextra -Werror \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DVERSION_US \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src" -idirafter "${repo_dir}/include" \
    "${repo_dir}/port/src/ge_original_mission_result.c" \
    "${repo_dir}/port/tests/test_ge_original_mission_result.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_ge_original_mission_result"

"${build_dir}/test_ge_original_mission_result"

cc -std=gnu11 -Wall -Wextra -Werror -Wno-comment -Wno-pointer-sign \
    -Wno-missing-braces -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES \
    -DVERSION_US -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/src" \
    "${repo_dir}/port/src/random_port.c" \
    "${repo_dir}/port/src/ge_original_mission_result.c" \
    "${repo_dir}/port/src/ge_3ds_save_provider.c" \
    "${repo_dir}/port/tests/test_ge_3ds_save_provider.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_ge_3ds_save_provider"

"${build_dir}/test_ge_3ds_save_provider"

python3 "${repo_dir}/scripts/extract_dam_mission_hud_slice.py" \
    "${repo_dir}" "${build_dir}/ge_original_dam_mission_hud_slice.c"

solo_language_sources=()
for language_bank in LdamE LarkE LrunE LsevxE LsevE LsiloE LdestE \
        LsevxbE LsevbE LstatE LarchE LpeteE LdepoE LtraE LjunE LarecE \
        LcaveE LcradE LaztE LcrypE; do
    solo_language_sources+=(
        "${repo_dir}/assets/obseg/text/${language_bank}.c")
done

cc -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter \
    -D_LANGUAGE_C -DGE_PORT_USE_ORIGINAL_TYPES -DVERSION_US -DAIPARSE \
    -DGE_PORT_MS_INHERITS -fms-extensions -DPLAYERFLAG=int \
    -I "${repo_dir}" -I "${repo_dir}/port/include" \
    -I "${repo_dir}/src/game" -idirafter "${repo_dir}/include" \
    -idirafter "${repo_dir}/include/PR" -idirafter "${repo_dir}/src" \
    "${build_dir}/ge_original_dam_mission_hud_slice.c" \
    "${solo_language_sources[@]}" \
    "${repo_dir}/assets/obseg/text/LgunE.c" \
    "${repo_dir}/assets/obseg/text/LmiscE.c" \
    "${repo_dir}/assets/obseg/text/LoptionsE.c" \
    "${repo_dir}/assets/obseg/text/LpropobjE.c" \
    "${repo_dir}/assets/obseg/text/LtitleE.c" \
    "${repo_dir}/port/tests/test_ge_original_dam_mission_hud.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_ge_original_dam_mission_hud"

"${build_dir}/test_ge_original_dam_mission_hud"

cc -std=gnu11 -Wall -Wextra -Werror -D_LANGUAGE_C \
    -DGE_PORT_USE_ORIGINAL_TYPES -I "${repo_dir}/port/include" \
    -idirafter "${repo_dir}/include" -idirafter "${repo_dir}/include/PR" \
    "${repo_dir}/assets/font/fontZurichBold.c" \
    "${repo_dir}/assets/font/fontBankGothic.c" \
    "${repo_dir}/port/src/ge_3ds_original_hud.c" \
    "${repo_dir}/port/tests/test_ge_3ds_original_hud.c" \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -o "${build_dir}/test_ge_3ds_original_hud"

"${build_dir}/test_ge_3ds_original_hud"

python3 "${repo_dir}/scripts/tests/test_dam_mission_exit_exact.py"
python3 "${repo_dir}/scripts/tests/test_campaign_terminal_ai_contract.py"
