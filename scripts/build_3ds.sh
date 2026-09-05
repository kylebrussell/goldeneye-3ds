#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
toolchain_image=${DEVKITPRO_IMAGE:-devkitpro/devkitarm:latest}

# Performance experiments restore source/header snapshots, sometimes with old
# mtimes. Rebuild every object so a rejected ABI variant cannot survive in an
# otherwise successful release/probe build. Timestamp-only make is unsafe here.
docker run --rm \
    -v "${repo_dir}:/workspace" \
    -w /workspace/platform/3ds \
    "${toolchain_image}" \
    make -B "$@"
