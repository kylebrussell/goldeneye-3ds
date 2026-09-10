#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
python3 "${repo_dir}/scripts/generate_multiplayer_frontend_data.py" --check
python3 "${repo_dir}/scripts/tests/test_original_multiplayer_frontend.py"
