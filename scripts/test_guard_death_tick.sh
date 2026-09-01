#!/usr/bin/env bash
set -euo pipefail

repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
GE_TEST_GUARD_DEATH_TICK_ONLY=1 \
    "${repo_dir}/scripts/test_covert_modem_fire.sh" "$@"
