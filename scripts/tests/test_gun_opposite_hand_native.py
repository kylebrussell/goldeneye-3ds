#!/usr/bin/env python3
"""Native array-boundary regression for the exact gunTickHandState hand seam.

The two source uses are separately checked by test_bond_input_live_state_exact.
The actual SWITCH_SWAP transition runs in test_multiplayer_match_bootstrap.
This probe also proves that restoring the original evaluation order reproduces
its sanitizer failure while the final selected physical hand remains identical.
"""
import os
import json
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
with tempfile.TemporaryDirectory(prefix='ge-other-hand-') as temporary:
    directory = Path(temporary)
    generated = directory / 'state.c'
    subprocess.run(['python3', str(ROOT/'scripts/extract_bond_input_live_state_slice.py'),
                    str(ROOT), str(generated)], check=True, capture_output=True)
    text = generated.read_text()
    # Preprocess the generated translation unit, so the 3DS profile cannot
    # silently choose the matching-source fallback exercised by the old fix.
    ledger=json.loads((ROOT/'build/host-tests/multiplayer-match/compile-ledger.json').read_text())
    base=next(entry['command'] for entry in ledger if entry['source'].endswith('/ge_original_bond_input_live_state_slice.c'))
    safe_macros=[]
    for arm in (False,True):
        command=base[:]
        for option in ('-MF','-c','-o'):
            index=command.index(option);del command[index:index+2]
        command=[arg for arg in command if arg not in ('-MMD',)]
        if arm:
            command=[arg for arg in command if not arg.startswith(('-DGE_PORT_FULL_GAMEPLAY_RUNTIME','-DGE_PORT_BOND_INPUT_HOST_STATE'))]
            command.append('-D__arm__')
        command+=['-E','-dD',str(generated)]
        preprocessed=subprocess.check_output(command,text=True)
        macro=re.findall(r'^#define GE_PORT_INPUT_OTHER_HAND.*$',preprocessed,re.M)
        assert len(macro)==1 and '(1 - (hand))' in macro[0]
        safe_macros.extend(macro)
    macros=safe_macros+['#define GE_PORT_INPUT_OTHER_HAND(hands, hand) ((hands - hand) + 1)']
    for index, macro in enumerate(macros):
        source = directory/f'probe{index}.c'
        source.write_text('''#include <assert.h>
struct Hand { unsigned char native_storage[4096]; };
struct Player { struct Hand hands[2]; unsigned canonical_tail; };
''' + macro + '''
static struct Hand *other(struct Player *p, volatile unsigned hand)
{ return GE_PORT_INPUT_OTHER_HAND(p->hands, hand); }
int main(void) {
    struct Player players[4];
    for (int count=2; count<=4; count+=2)
        for (int slot=0; slot<count; ++slot)
            for (int hand=0; hand<2; ++hand)
                assert(other(&players[slot],hand)==&players[slot].hands[1-hand]);
}
''')
        binary = directory/f'probe{index}'
        subprocess.run(['cc','-O1','-g','-fsanitize=address,undefined',str(source),'-o',str(binary)],check=True)
        result = subprocess.run([str(binary)], capture_output=True, text=True,
            env=dict(os.environ, ASAN_OPTIONS='detect_leaks=0', UBSAN_OPTIONS='halt_on_error=1'))
        if index < 2:
            assert result.returncode == 0, result.stderr
        else:
            assert result.returncode != 0 and 'out of bounds' in result.stderr, result.stderr
print('other-hand selection: native and 3DS preprocessing, 2/4 physical rosters; removed-fix UBSan control reproduced')
