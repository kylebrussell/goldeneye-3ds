#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
toolchain_image=${DEVKITPRO_IMAGE:-devkitpro/devkitarm:latest}

docker run --rm \
    -v "${repo_dir}:/workspace" \
    -w /workspace/platform/3ds \
    "${toolchain_image}" \
    make "$@"
