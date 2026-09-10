#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "${repo_dir}"
python3 scripts/build_multiplayer_match_host.py
python3 - "${1:-build/host-tests/multiplayer-assets/multiplayer.gepack}" \
    "${2:-build/host-tests/multiplayer-bodies/multiplayer.gepack}" \
    "${3:-build/multiplayer-port-tests/original-dam-setup/first-person-pp7.gepack}" \
    "${4:-build/3ds-animations/bond}" "${5:-}" <<'PY'
import json,os,subprocess,sys
from pathlib import Path
build=Path('build/host-tests/multiplayer-match')
ledger=json.loads((build/'compile-ledger.json').read_text())
command=next(e for e in ledger if e['source'].endswith('/ge_original_multiplayer_runtime.c'))['command'][:]
command[command.index('-MF')+1]=str(build/'match-bootstrap.d')
command[command.index('-c')+1]='port/tests/test_ge_original_multiplayer_match.c'
command[command.index('-o')+1]=str(build/'match-bootstrap.o')
subprocess.run(command+['-Werror=implicit-function-declaration','-Werror=incompatible-pointer-types'],check=True)
binary=build/'match-bootstrap'
subprocess.run(['cc','-fsanitize=address,undefined','-Wl,-dead_strip',
    str(build/'match-bootstrap.o'),str(build/'libmultiplayer-native.a'),'-lm','-o',str(binary)],check=True)
symbols=subprocess.check_output(['nm','-g',str(binary)],text=True)
for name in ('MoveBond','ge_original_dam_guard_props_tick_exact',
             'ge_original_player_held_items_tick','gunTickGameplay',
             'ge_original_gun_update_and_fire_both_hands_exact','ge_original_multiplayer_death_post_gun',
             'ge_original_multiplayer_respawn','ge_original_multiplayer_session_tick'):
    assert any(line.endswith(' T _'+name) or line.endswith(' T '+name)
               for line in symbols.splitlines()),name
(build/'bootstrap-link-symbols.txt').write_text(symbols)
result=subprocess.run([str(binary)]+[arg for arg in sys.argv[1:] if arg],text=True,capture_output=True,
    env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0',UBSAN_OPTIONS='halt_on_error=1:print_stacktrace=1'))
(build/'bootstrap-transcript.txt').write_text(result.stdout+result.stderr)
print(result.stdout+result.stderr,end='')
result.check_returncode()
if sys.argv[5]:
    print('Native supplies gate: fully populated Caves, pickup/equip/fire/impacts/reload; player damage/death/respawn/results remain separate acceptance gates.')
else:
    print('Bootstrap gate only: actual Temple assets and 2/4-player shared frames; authored supplies and hit/death/respawn progression remain separate acceptance gates.')
PY
