#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${repo_dir}"
python3 scripts/build_multiplayer_match_host.py
python3 - <<'PY'
import json, os, subprocess
from pathlib import Path
build=Path('build/host-tests/multiplayer-match')
ledger=json.loads((build/'compile-ledger.json').read_text())
entry=next(e for e in ledger if e['source'].endswith('/ge_original_multiplayer_runtime.c'))
command=entry['command'][:]
command[command.index('-MF')+1]=str(build/'render-fixture.d')
command[command.index('-c')+1]='port/tests/test_ge_original_prop_render_semantics.c'
command[command.index('-o')+1]=str(build/'render-fixture.o')
command+=['-Werror=implicit-function-declaration']
subprocess.run(command,check=True)
binary=build/'render-fixture'
subprocess.run(['cc','-fsanitize=address,undefined','-Wl,-dead_strip',
    str(build/'render-fixture.o'),str(build/'libmultiplayer-native.a'),'-lm','-o',str(binary)],check=True)
symbols=subprocess.check_output(['nm','-g',str(binary)],text=True)
for name in ('ge_original_prop_render_semantic_exact','randomGetNext','modelGetNodeRwData',
             'sub_GAME_7F06B248','fogGetPropDistColor','chrobjFogVisRangeRelated',
             'ge_original_multiplayer_players_create'):
    assert any(line.endswith(' T _'+name) or line.endswith(' T '+name)
               for line in symbols.splitlines()),name
(build/'render-link-symbols.txt').write_text(symbols)
env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1')
subprocess.run([str(binary)],env=env,check=True)
PY
