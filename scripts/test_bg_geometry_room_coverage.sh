#!/usr/bin/env bash
set -euo pipefail
repo_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
if [[ $# != 1 ]]; then
    echo 'Usage: test_bg_geometry_room_coverage.sh complete-original-asset-pack.gepack' >&2
    exit 64
fi
build_dir=$(mktemp -d /tmp/ge-bg-coverage-mutation.XXXXXX)
trap 'rm -rf "${build_dir}"' EXIT
python3 - "${repo_dir}" "${build_dir}" "$1" <<'PY'
import os, subprocess, sys
from pathlib import Path
repo, output, pack=map(Path,sys.argv[1:])
source=(repo/'port/src/ge_original_bg_geometry.c').read_text()
signature='bool bgTestBulletHitBackground(coord3d *from, coord3d *to, s32 room, HitThing *hit)\n{'
assert source.count(signature)==1
mutated=output/'disabled-room.c'
mutated.write_text(source.replace(signature,signature+'\n    if (room == 2) return FALSE;'))
for abi in ('native','short-enum'):
    env=dict(os.environ,GE_BG_TEST_SOURCE=str(mutated),GE_BG_TEST_ABIS=abi)
    result=subprocess.run(['bash',str(repo/'scripts/test_bg_geometry.sh'),str(pack)],
        env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    if result.returncode==0 or 'eligible centroid missed room 2' not in result.stdout:
        print(result.stdout)
        raise SystemExit('Room2 collision mutation escaped centroid coverage: '+abi)
    print('BG room coverage: complete room2 collision loss detected in '+abi)
PY
