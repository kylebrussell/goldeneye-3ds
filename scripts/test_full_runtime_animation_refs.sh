#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
animation_dir=${1:-"${repo_dir}/build/3ds-animations/bond"}
cd "${repo_dir}"
python3 scripts/build_multiplayer_match_host.py
python3 - "${animation_dir}" <<'PY'
import json,os,subprocess,sys
from pathlib import Path
build=Path('build/host-tests/multiplayer-match')
ledger=json.loads((build/'compile-ledger.json').read_text())
command=next(e for e in ledger if e['source'].endswith('/ge_original_multiplayer_runtime.c'))['command'][:]
command[command.index('-MF')+1]=str(build/'animation-ref-fixture.d')
command[command.index('-c')+1]='port/tests/test_ge_original_full_runtime_animation_refs.c'
command[command.index('-o')+1]=str(build/'animation-ref-fixture.o')
subprocess.run(command+['-Werror=implicit-function-declaration'],check=True)
binary=build/'animation-ref-fixture'
subprocess.run(['cc','-fsanitize=address,undefined','-Wl,-dead_strip',
    str(build/'animation-ref-fixture.o'),str(build/'libmultiplayer-native.a'),'-lm','-o',str(binary)],check=True)
directory=Path(sys.argv[1])
subprocess.run([str(binary),str(directory/'animation_data.bin'),str(directory/'animation_entries.bin')],
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1'),check=True)
PY
