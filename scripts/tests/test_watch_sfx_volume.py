#!/usr/bin/env python3
"""Exercise retained Watch settings reads with the production SFX slot owner."""
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from extract_gun_update_and_fire_slice import extract_function

snd = (ROOT / 'src/snd.c').read_text()
options = (ROOT / 'src/game/options.c').read_text()
services = (ROOT / 'port/src/ge_original_gameplay_services.c').read_text()
body = '\n'.join([
    extract_function(snd, 'sndGetSfxSlotNaturalVolume'),
    extract_function(snd, 'sndGetSfxSlotFirstNaturalVolume'),
    extract_function(options, 'call_sndGetSfxSlotFirstNaturalVolume'),
    extract_function(services, 'ge_service_file_save_settings_exact'),
    extract_function(services, 'deleteCurrentSelectedFolder'),
])
unit = r'''
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/file2.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_sfx_volume.h"
#include <assert.h>
#include <string.h>
u16 *g_sndSfxSlotNaturalVolume;
s16 *g_sndSfxSlotVolume;
static GeOriginalGameplayServiceStats ge_service_stats;
static void *ge_service_settings_context;
static int (*ge_service_persist_settings)(void *, uint8_t, uint8_t, uint16_t);
static uint8_t saved_music, saved_sfx;
static uint16_t saved_options;
static int persist(void *context, uint8_t music, uint8_t sfx, uint16_t options) {
    assert(context == &saved_sfx);
    saved_music = music; saved_sfx = sfx; saved_options = options; return 1;
}
static u16 get_mTrack2Vol(void) {return 0x4321;}
static int get_cur_player_look_vertical_inverted(void) {return 1;}
static int cur_player_get_autoaim(void) {return 1;}
static int cur_player_get_aim_control(void) {return 0;}
static int cur_player_get_sight_onscreen_control(void) {return 1;}
static int cur_player_get_lookahead(void) {return 0;}
static int cur_player_get_ammo_onscreen_setting(void) {return 1;}
static int cur_player_get_screen_setting(void) {return SCREEN_SIZE_WIDESCREEN;}
static int get_screen_ratio(void) {return SCREEN_RATIO_NORMAL;}
static int cur_player_get_control_type(void) {return 2;}
''' + body + r'''
int main(int argc, char **argv) {
    (void)argv;
    ge_service_settings_context = &saved_sfx;
    ge_service_persist_settings = persist;
    /* Omitting initialization recreates the actual native settings read. */
    if (argc == 1) ge_original_sfx_volume_init();
    deleteCurrentSelectedFolder();
    assert(saved_sfx == 255 && saved_music == (0x4321 >> 7));
    assert(saved_options == (OPTION_INVERTLOOK | OPTION_AUTOAIM
        | OPTION_SIGHTONSCREEN | OPTION_DISPLAYAMMO | OPTION_SCREENWIDE | 0x200));
    for (unsigned i=0;i<7;i++) {
        assert(sndGetSfxSlotNaturalVolume(i) == 0x7fff);
        assert(g_sndSfxSlotVolume[i] == 0x7fff);
        g_sndSfxSlotNaturalVolume[i] = 0x1234 + i;
        g_sndSfxSlotVolume[i] = 0x1000 + i;
    }
    ge_original_sfx_volume_init();
    for (unsigned i=0;i<7;i++) {
        assert(sndGetSfxSlotNaturalVolume(i) == 0x1234+i);
        assert(g_sndSfxSlotVolume[i] == (s16)(0x1000+i));
    }
    deleteCurrentSelectedFolder();
    assert(saved_sfx == (0x1234 >> 7));
    assert(ge_service_stats.settings_persist_calls == 2);
    assert(ge_service_stats.settings_persist_failures == 0);
    u16 external_natural[7] = {123,234,345,456,567,678,789};
    s16 external_effective[7] = {1,2,3,4,5,6,7};
    g_sndSfxSlotNaturalVolume = external_natural;
    g_sndSfxSlotVolume = external_effective;
    ge_original_sfx_volume_init();
    assert(g_sndSfxSlotNaturalVolume == external_natural);
    assert(g_sndSfxSlotVolume == external_effective);
    g_sndSfxSlotVolume = NULL;
    ge_original_sfx_volume_init();
    for (unsigned i=0;i<7;i++) assert(g_sndSfxSlotVolume[i] == external_natural[i]);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='ge-watch-volume-') as folder:
    src=Path(folder)/'test.c';src.write_text(unit)
    for abi in ['-fno-short-enums', '-fshort-enums']:
        exe=Path(folder)/('test'+abi)
        flags=['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-comment',
               '-fms-extensions','-fsanitize=address,undefined','-fno-sanitize-recover=all',
               '-D_LANGUAGE_C','-DGE_PORT_USE_ORIGINAL_TYPES','-DAIPARSE',
               '-DGE_PORT_SETUP_DATA','-DGE_PORT_MS_INHERITS',abi,
               '-I'+str(ROOT/'port/include'),'-idirafter',str(ROOT/'include'),
               '-idirafter',str(ROOT/'include/PR'),'-idirafter',str(ROOT/'src')]
        subprocess.run(flags+[str(src),str(ROOT/'port/src/ge_original_sfx_volume.c'),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
        missing = subprocess.run([str(exe),'omit-init'], capture_output=True, text=True)
        assert missing.returncode != 0 and 'null pointer' in missing.stderr, missing.stderr
main = extract_function((ROOT/'platform/3ds/source/main.c').read_text(), 'main')
assert main.index('ge_original_sfx_volume_init();') < main.index('run_application()')
print('Watch settings: real native SFX getters, seven slots, persisted volume/options, retained settings/external owners; missing initialization detected in both enum layouts')
