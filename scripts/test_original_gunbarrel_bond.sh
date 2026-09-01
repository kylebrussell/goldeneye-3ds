#!/usr/bin/env bash
set -euo pipefail
repo_dir=${1:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}
python3 "${repo_dir}/port/tests/test_ge_original_gunbarrel_bond_contract.py"
"${repo_dir}/scripts/test_original_gunbarrel_bond_live.sh" "${repo_dir}"
