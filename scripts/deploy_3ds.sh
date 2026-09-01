#!/usr/bin/env bash

set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
target_address=""
skip_build=0
dry_run=0

usage() {
    printf 'Usage: %s [--ip ADDRESS] [--skip-build] [--dry-run]\n' "$0"
    printf '\nWithout --ip, validates the staged SD-card tree and prints copy instructions.\n'
}

while (($# > 0)); do
    case "$1" in
        --ip)
            if (($# < 2)); then
                usage >&2
                exit 2
            fi
            target_address=$2
            shift 2
            ;;
        --skip-build)
            skip_build=1
            shift
            ;;
        --dry-run)
            dry_run=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            printf 'Unknown argument: %s\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

if ((skip_build == 0)); then
    make -C "${repo_dir}" 3ds-stage
fi

stage_dir="${repo_dir}/build/3ds-sd/3ds/goldeneye-3ds"
executable="${stage_dir}/goldeneye-3ds.3dsx"
metadata="${stage_dir}/goldeneye-3ds.smdh"
asset_pack="${stage_dir}/goldeneye.u.gepack"

for required_file in "${executable}" "${metadata}" "${asset_pack}"; do
    if [[ ! -s "${required_file}" ]]; then
        printf 'Missing or empty staged file: %s\n' "${required_file}" >&2
        exit 1
    fi
done

printf 'Validated staged files:\n'
shasum -a 256 "${executable}" "${metadata}" "${asset_pack}"

if [[ -z "${target_address}" ]]; then
    printf '\nCopy this directory to sd:/3ds/goldeneye-3ds/:\n%s\n' "${stage_dir}"
    printf 'For netloader, rerun with --ip ADDRESS after opening 3dslink in Homebrew Menu.\n'
    exit 0
fi

command=(
    docker run --rm --network host
    -v "${repo_dir}:/workspace"
    -w /workspace
    devkitpro/devkitarm:latest
    3dslink build/3ds-sd/3ds/goldeneye-3ds/goldeneye-3ds.3dsx
    -a "${target_address}"
)

if ((dry_run != 0)); then
    printf 'Would run:'
    printf ' %q' "${command[@]}"
    printf '\n'
    exit 0
fi

printf 'Sending goldeneye-3ds.3dsx to %s...\n' "${target_address}"
"${command[@]}"
