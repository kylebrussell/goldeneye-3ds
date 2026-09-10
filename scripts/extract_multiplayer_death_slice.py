#!/usr/bin/env python3
"""Extract canonical per-player fade/death statements, excluding draw/respawn."""
from pathlib import Path
import argparse
import re
from extract_bond_move_state_slice import extract_function, digest

HELPERS = ('currentPlayerSetFadeColour', 'currentPlayerAdjustFade',
           'currentPlayerStartChrFade', 'currentPlayerUpdateColourScreenProperties',
           'currentPlayerTickChrFade')

def block_at(text, start):
    opening = text.index('{', start)
    depth = 0
    for end in range(opening, len(text)):
        if text[end] == '{': depth += 1
        elif text[end] == '}':
            depth -= 1
            if depth == 0: return text[start:end+1], end+1
    raise ValueError('unclosed canonical block')

def generate(root):
    source = (root/'src/game/bondview2.c').read_text()
    interface = extract_function(source, 'maybe_mp_interface')
    viewport = extract_function(source, 'bondviewMovePlayerUpdateViewport')
    assert viewport.index('currentPlayerUpdateColourScreenProperties();') < viewport.index('currentPlayerTickChrFade();') < viewport.index('MoveBond(stick_x')
    assert interface.index('gunUpdateAndFireBothHands();') < interface.index('if (g_CurrentPlayer->bonddead != 0)')
    start = interface.index('    if (g_CurrentPlayer->bonddead != 0)')
    end = interface.index('                if (currentPlayerIsFadeComplete())', start)
    death = interface[start:end]
    # The public boundary validates 2..4 players, so keep the original MP
    # mission-state branch and omit the unreachable solo music services.
    music_start = death.index('                if (getPlayerCount() == 1)')
    _, solo_end = block_at(death, music_start)
    else_start = death.index('else', solo_end)
    mp, mp_end = block_at(death, else_start)
    assert 'set_missionstate(6);' in mp
    death = death[:music_start] + '                set_missionstate(6);' + death[mp_end:]
    assert death.count('gdl = gameplayBloodOverlayDL(gdl);') == 1
    death = death.replace('                    gdl = gameplayBloodOverlayDL(gdl);', '')
    # End immediately before the original policy/button-driven respawn block.
    assert death.count('{') - death.count('}') == 3
    death += '            }\n        }\n    }\n'
    out = ['/* Generated canonical death/fade simulation state; no display lists. */',
           '#include <ultra64.h>', '#include <bondconstants.h>', '#include <bondtypes.h>',
           '#include "game/player.h"', '#include "game/model.h"',
           'extern s32 g_ClockTimer;', 'extern f32 g_GlobalTimerDelta;',
           's32 die_blood_image_routine(s32 mode);', 'void set_missionstate(s32 state);']
    for name in HELPERS:
        body = extract_function(source, name)
        source_digest = digest(body)
        if name == 'currentPlayerTickChrFade':
            # MIPS converts float to a word then stores its low byte. Direct
            # float->s8 conversion of the authored 128..255 range is C UB.
            assert body.count('(s8)(frac * 255)') == 1
            body = body.replace('(s8)(frac * 255)', '(u8)(s32)(frac * 255)')
            out += ['/* Native MIPS conversion boundary: word conversion then low-byte store. */']
        out += [f'/* {name} sha256={source_digest} */', 'static '+body]
    out += [f'/* viewport source order sha256={digest(viewport)} */',
            f'/* maybe_mp_interface sha256={digest(interface)}; death prefix only, MP branch, draw sink omitted. */',
            'void ge_original_multiplayer_death_pre_move_exact(void) {\n'
            '    currentPlayerUpdateColourScreenProperties();\n'
            '    currentPlayerTickChrFade();\n}',
            'void ge_original_multiplayer_death_post_gun_exact(void) {\n'
            '    s32 doblood;\n    if (g_CurrentPlayer->cameramode == 1) return;\n'+death+'}']
    text = '\n\n'.join(out)+'\n'
    # Namespace retained helpers to coexist with the solo mission-exit owner.
    for name in HELPERS:
        text = re.sub(r'\b'+name+r'\b', 'ge_mp_death_'+name, text)
    return text

if __name__ == '__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('repo',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(generate(args.repo))
