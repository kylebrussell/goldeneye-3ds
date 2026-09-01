#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
asset_image=${GE_ASSET_IMAGE:-goldeneye-3ds-assets:latest}

python3 "${repo_dir}/scripts/extract_3ds_runtime_segments.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-runtime"

python3 "${repo_dir}/scripts/extract_3ds_blotter_model.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/blotter1"

python3 "${repo_dir}/scripts/extract_3ds_model62.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/chrwppksil"

python3 "${repo_dir}/scripts/extract_3ds_first_person_pp7.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/first-person-pp7"

python3 "${repo_dir}/scripts/extract_3ds_model104.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/window"

python3 "${repo_dir}/scripts/extract_3ds_model178.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/damgatedoor"

python3 "${repo_dir}/scripts/extract_3ds_dam_objective_models.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/dam-objectives"

python3 "${repo_dir}/scripts/extract_3ds_greatguard2.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-models/greatguard2"

python3 "${repo_dir}/scripts/stage_3ds_pitem_models.py" \
    --root "${repo_dir}" \
    --output "${repo_dir}/build/3ds-models/pitem"

python3 "${repo_dir}/scripts/stage_3ds_pitem_models.py" \
    --root "${repo_dir}" \
    --output "${repo_dir}/build/3ds-models/pitem" \
    --check

python3 "${repo_dir}/scripts/stage_3ds_character_models.py" \
    --root "${repo_dir}" \
    --output "${repo_dir}/build/3ds-models/characters"

python3 "${repo_dir}/scripts/stage_3ds_character_models.py" \
    --root "${repo_dir}" \
    --output "${repo_dir}/build/3ds-models/characters" \
    --check

python3 "${repo_dir}/scripts/extract_3ds_bond_animations.py" \
    --rom "${repo_dir}/baserom.u.z64" \
    --output "${repo_dir}/build/3ds-animations/bond"

python3 "${repo_dir}/scripts/extract_3ds_dam_room1.py" \
    --input "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
    --output "${repo_dir}/build/3ds-models/dam-room1"

python3 "${repo_dir}/scripts/extract_3ds_dam_rooms.py" \
    --input "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
    --output "${repo_dir}/build/3ds-levels/dam/rooms"

python3 "${repo_dir}/scripts/extract_3ds_dam_collision.py" \
    --input "${repo_dir}/assets/obseg/stan/Tbg_dam_all_p_stanZ.c" \
    --output "${repo_dir}/build/3ds-levels/dam/collision"

python3 "${repo_dir}/scripts/build_3ds_dam_room_bounds.py" \
    --background "${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
    --rooms "${repo_dir}/build/3ds-levels/dam/rooms" \
    --output "${repo_dir}/build/3ds-levels/dam/room_bounds.gebounds"

python3 "${repo_dir}/scripts/extract_3ds_facility_level.py" \
    --output "${repo_dir}/build/3ds-levels/facility"

python3 "${repo_dir}/scripts/generate_3ds_stage_inventory.py" \
    --output "${repo_dir}/docs/generated/solo_stage_asset_inventory.json"

python3 "${repo_dir}/scripts/extract_3ds_solo_stages.py" \
    --inventory "${repo_dir}/docs/generated/solo_stage_asset_inventory.json" \
    --output "${repo_dir}/build/3ds-levels"

python3 "${repo_dir}/scripts/extract_3ds_credits_stage.py" \
    --root "${repo_dir}" --output "${repo_dir}/build/3ds-levels/cuba"

python3 "${repo_dir}/scripts/generate_3ds_stage_registry.py" \
    --inventory "${repo_dir}/docs/generated/solo_stage_asset_inventory.json" \
    --bundles "${repo_dir}/build/3ds-levels" \
    --output "${repo_dir}/port/include/ge_solo_stage_registry.inc" \
    --check

stage_pack_args=(
    --extra-dir "converted/levels/dam=${repo_dir}/build/3ds-levels/dam"
    --extra-dir "converted/levels/facility=${repo_dir}/build/3ds-levels/facility"
    --extra-dir "converted/levels/cuba=${repo_dir}/build/3ds-levels/cuba"
)
pack_integrity_args=(
    --required "converted/levels/dam/background.bin=${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin"
    --required "converted/levels/dam/collision/collision.gestan=${repo_dir}/build/3ds-levels/dam/collision/collision.gestan"
    --required "converted/levels/dam/room_bounds.gebounds=${repo_dir}/build/3ds-levels/dam/room_bounds.gebounds"
    --required "converted/levels/dam/setup/setup.bin=${repo_dir}/build/u/assets/obseg/setup/UsetupdamZ.bin"
    --required "converted/levels/facility/background.bin=${repo_dir}/build/3ds-levels/facility/background.bin"
    --required "converted/levels/facility/collision/collision.gestan=${repo_dir}/build/3ds-levels/facility/collision/collision.gestan"
    --required "converted/levels/facility/room_bounds.gebounds=${repo_dir}/build/3ds-levels/facility/room_bounds.gebounds"
    --required "converted/levels/facility/collision/setup.bin=${repo_dir}/build/3ds-levels/facility/collision/setup.bin"
    --required "converted/levels/cuba/background.bin=${repo_dir}/build/3ds-levels/cuba/background.bin"
    --required "converted/levels/cuba/collision/collision.gestan=${repo_dir}/build/3ds-levels/cuba/collision/collision.gestan"
    --required "converted/levels/cuba/room_bounds.gebounds=${repo_dir}/build/3ds-levels/cuba/room_bounds.gebounds"
    --required "converted/levels/cuba/collision/setup.bin=${repo_dir}/build/3ds-levels/cuba/collision/setup.bin"
    --required "converted/models/first-person-pp7/GwppkZ.bin=${repo_dir}/build/3ds-models/first-person-pp7/GwppkZ.bin"
    --required "converted/models/first-person-pp7/GwppksilZ.bin=${repo_dir}/build/3ds-models/first-person-pp7/GwppksilZ.bin"
    --required "converted/models/first-person-pp7/GbugZ.bin=${repo_dir}/build/3ds-models/first-person-pp7/GbugZ.bin"
    --required "converted/models/first-person-pp7/Csuit_lf_handZ.bin=${repo_dir}/build/3ds-models/first-person-pp7/Csuit_lf_handZ.bin"
    --required "converted/models/chrwppksil/model.bin=${repo_dir}/build/3ds-models/chrwppksil/model.bin"
    --required "converted/models/damgatedoor/model.bin=${repo_dir}/build/3ds-models/damgatedoor/model.bin"
    --required "converted/models/modembox/model.bin=${repo_dir}/build/3ds-models/dam-objectives/modembox/model.bin"
    --required "converted/models/satdish/model.bin=${repo_dir}/build/3ds-models/dam-objectives/satdish/model.bin"
    --required "converted/models/greatguard2/model.bin=${repo_dir}/build/3ds-models/greatguard2/model.bin"
    --required "converted/models/chrkalash/model.bin=${repo_dir}/build/u/assets/obseg/prop/PchrkalashZ.bin"
)
while IFS= read -r stage_key; do
    stage_pack_args+=(
        --extra-dir "converted/levels/${stage_key}=${repo_dir}/build/3ds-levels/${stage_key}"
    )
    pack_integrity_args+=(
        --required "converted/levels/${stage_key}/background.bin=${repo_dir}/build/3ds-levels/${stage_key}/background.bin"
        --required "converted/levels/${stage_key}/collision/collision.gestan=${repo_dir}/build/3ds-levels/${stage_key}/collision/collision.gestan"
        --required "converted/levels/${stage_key}/room_bounds.gebounds=${repo_dir}/build/3ds-levels/${stage_key}/room_bounds.gebounds"
        --required "converted/levels/${stage_key}/collision/setup.bin=${repo_dir}/build/3ds-levels/${stage_key}/collision/setup.bin"
    )
done < <(python3 -c \
    'import json,sys; [print(s["runtime_key"]) for s in json.load(open(sys.argv[1]))["stages"]]' \
    "${repo_dir}/docs/generated/solo_stage_asset_inventory.json")

docker build -f "${repo_dir}/Dockerfile.3ds-assets" -t "${asset_image}" "${repo_dir}"
docker run --rm \
    -e GE_ASSET_JOBS="${GE_ASSET_JOBS:-4}" \
    -v "${repo_dir}:/workspace" \
    -w /workspace \
    "${asset_image}" \
    sh -lc '
        mkdir -p build
        cc tools/mktex/src/tex2png.c \
           tools/mktex/src/libpdtex/pdtex.c \
           tools/mktex/src/libpdtex/reader.c \
           tools/mktex/src/libpdtex/writer.c \
           -lpng -lz -O3 -o build/tex2png
        python3 scripts/convert_3ds_textures.py \
            --input assets/images/split \
            --images-def assets/images.def \
            --png-output build/3ds-textures/png \
            --t3x-output build/3ds-textures/t3x \
            --catalog build/3ds-textures/catalog.json \
            --tex2png build/tex2png \
            --tex3ds tex3ds \
            --mipmap box \
            --jobs "${GE_ASSET_JOBS}"
        python3 scripts/generate_3ds_texture_catalog.py \
            --input build/3ds-textures/catalog.json \
            --images-def assets/images.def \
            --output build/3ds-textures/catalog.gecat
        python3 scripts/convert_rareware_logo_3ds.py \
            --segment build/3ds-runtime/segments/rarewarelogo.bin \
            --output build/3ds-runtime/rareware-textures \
            --tex3ds tex3ds
        python3 scripts/convert_frontend_background_3ds.py \
            --input assets/ge007.u.2A4D50.usedby7F008DE4.bin \
            --output build/3ds-runtime/frontend/folder-background.t3x \
            --tex3ds tex3ds
    '

python3 "${repo_dir}/scripts/pack_3ds_assets.py" \
    --assets "${repo_dir}/assets" \
    --rom "${repo_dir}/baserom.u.z64" \
    --extra "converted/textures/catalog.json=${repo_dir}/build/3ds-textures/catalog.json" \
    --extra "converted/textures/catalog.gecat=${repo_dir}/build/3ds-textures/catalog.gecat" \
    --extra "converted/textures/COPYICON.t3x=${repo_dir}/build/3ds-textures/t3x/COPYICON-0.t3x" \
    --extra "converted/frontend/folder-background.t3x=${repo_dir}/build/3ds-runtime/frontend/folder-background.t3x" \
    --extra "converted/levels/dam/background.bin=${repo_dir}/build/u/assets/obseg/bg/bg_dam_all_p.bin" \
    --extra "converted/levels/dam/setup/setup.bin=${repo_dir}/build/u/assets/obseg/setup/UsetupdamZ.bin" \
    --extra "converted/models/chrkalash/model.bin=${repo_dir}/build/u/assets/obseg/prop/PchrkalashZ.bin" \
    --extra-dir "converted/textures/t3x=${repo_dir}/build/3ds-textures/t3x" \
    --extra-dir "converted/runtime=${repo_dir}/build/3ds-runtime" \
    --extra-dir "converted/models/blotter1=${repo_dir}/build/3ds-models/blotter1" \
    --extra-dir "converted/models/chrwppksil=${repo_dir}/build/3ds-models/chrwppksil" \
    --extra-dir "converted/models/first-person-pp7=${repo_dir}/build/3ds-models/first-person-pp7" \
    --extra-dir "converted/models/window=${repo_dir}/build/3ds-models/window" \
    --extra-dir "converted/models/damgatedoor=${repo_dir}/build/3ds-models/damgatedoor" \
    --extra-dir "converted/models/modembox=${repo_dir}/build/3ds-models/dam-objectives/modembox" \
    --extra-dir "converted/models/satdish=${repo_dir}/build/3ds-models/dam-objectives/satdish" \
    --extra-dir "converted/models/greatguard2=${repo_dir}/build/3ds-models/greatguard2" \
    --extra-dir "converted/models/pitem=${repo_dir}/build/3ds-models/pitem" \
    --extra-dir "converted/models/characters=${repo_dir}/build/3ds-models/characters" \
    --extra-dir "converted/animations/bond=${repo_dir}/build/3ds-animations/bond" \
    --extra-dir "converted/levels/dam/room1=${repo_dir}/build/3ds-models/dam-room1" \
    "${stage_pack_args[@]}" \
    --output "${repo_dir}/build/3ds-assets/goldeneye.u.gepack"

python3 "${repo_dir}/scripts/verify_3ds_asset_pack.py" \
    --pack "${repo_dir}/build/3ds-assets/goldeneye.u.gepack" \
    --required "converted/frontend/folder-background.t3x=${repo_dir}/build/3ds-runtime/frontend/folder-background.t3x" \
    "${pack_integrity_args[@]}"
