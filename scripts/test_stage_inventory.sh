#!/usr/bin/env bash
set -euo pipefail

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

python3 -m unittest \
    "${repo_dir}/scripts/tests/test_generate_3ds_stage_inventory.py"
