#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GE_TEST_GUN_GAMEPLAY=1 bash "${repo_dir}/scripts/test_multiplayer_gun_producer.sh" "$@"
