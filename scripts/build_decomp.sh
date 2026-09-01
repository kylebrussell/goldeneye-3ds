#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
image_name=${GOLDENEYE_DECOMP_IMAGE:-goldeneye-decomp}

docker build --platform linux/amd64 -t "${image_name}" "${repo_dir}"
docker run --rm --platform linux/amd64 \
    -v "${repo_dir}:/home/dev/project" \
    "${image_name}" \
    make IDO_RECOMP=NO "$@"
