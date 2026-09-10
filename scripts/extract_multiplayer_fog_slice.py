#!/usr/bin/env python3
"""Own the original US fog state, stage selection and prop visibility gates."""
from pathlib import Path
import argparse
from extract_dam_guard_chr_scheduler_support_slice import definition_text
from extract_dam_guard_chr_scheduler_slice import function_text
from extract_stage_environment_tables import declaration

FUNCTIONS = (
    'fogGetCurrentEnvironmentp', 'fogGetScaledFarFogIntensity',
    'fogGetScaledFarFogIntensitySquared', 'fogLoadCurrentEnvironment',
    'fogLoadFoglessCurrentEnvironment', 'fogLoadLevelEnvironment',
    'fogSwitchToSolosky2', 'fogPositionIsVisibleThroughFog',
    'fogGetNearFogValuesP', 'fogGetPropDistColor',
)

def generate(repo):
    source = (repo/'src/game/bgfog.c').read_text()
    non_eu = source.index('#else\n/**\n * Address 0x80044E10.')
    pieces = [
        '/* Generated from original US bgfog.c and propobj.c; do not edit. */',
        '#include <ultra64.h>\n#include <bondtypes.h>\n#include <bondconstants.h>',
        '#include <float.h>\n#include <string.h>\n#include "game/bgfog.h"',
        '#include "game/bondview.h"\n#include "game/player.h"',
        '#include "ge_original_multiplayer_fog.h"',
        'extern void viSetZRange(f32 near, f32 far);\nextern void viGetZRange(f32 *zrange);',
        'void fogLoadCurrentEnvironment(EnvironmentRecord *);',
        'void fogLoadFoglessCurrentEnvironment(EnvironmentFoglessRecord *);',
        # US has a single floating-point near record; propobj.c uses the EU
        # tag spelling without its declaration. Preserve the fields/algorithm.
        '#define NearFogRecordF NearFogRecord',
        '#define bgGetLevelVisibilityScale ge_original_multiplayer_fog_visibility_scale',
    ]
    for pattern in (
        r'^s32 g_FogSkyIsEnabled;', r'^NearFogRecord \*g_NearFogValuesP;',
        r'^struct FogDetails', r'^f32 g_FarFogIntensity;',
        r'^f32 g_DifferenceFromFarFogIntensity;',
        r'^EnvironmentRecord \*g_EnvironmentFoundp;',
        r'^EnvironmentRecord \* g_EnvironmentMainp;',
        r'^EnvironmentRecord \* g_EnvironmentAltp;',
        r'^f32 g_ScaledFarFogIntensity\s*=\s*FLT_MAX;',
        r'^f32 g_ScaledDifferenceFromFarFogIntensity\s*=\s*0\.0;',
        r'^CurrentEnvironmentRecord g_CurrentEnvironment\s*=',
    ):
        pieces.append(definition_text(source, pattern))
    pieces += [declaration(source[non_eu:], 'EnvironmentRecord fog_tables[] = {'),
        declaration(source, 'EnvironmentFoglessRecord fog_tables2[] = {')]
    for name in FUNCTIONS:
        pieces.append(function_text(source, name))
    pieces.append(function_text((repo/'src/game/propobj.c').read_text(),
                               'chrobjFogVisRangeRelated'))
    initial = definition_text(source, r'^CurrentEnvironmentRecord g_CurrentEnvironment\s*=')
    initial = initial.replace('CurrentEnvironmentRecord g_CurrentEnvironment',
                              'static const CurrentEnvironmentRecord initial')
    pieces += [r'''
void ge_original_multiplayer_fog_reset_exact(void)
{
''' + initial + r'''
    g_CurrentEnvironment = initial;
    g_FogSkyIsEnabled = 0;
    g_NearFogValuesP = NULL;
    memset(&g_CurFogDetails, 0, sizeof(g_CurFogDetails));
    g_FarFogIntensity = g_DifferenceFromFarFogIntensity = 0.0f;
    g_EnvironmentFoundp = g_EnvironmentMainp = g_EnvironmentAltp = NULL;
    g_ScaledFarFogIntensity = FLT_MAX;
    g_ScaledDifferenceFromFarFogIntensity = 0.0f;
}

void ge_original_multiplayer_fog_snapshot_exact(GeOriginalMultiplayerFogSnapshot *out)
{
    out->selected_id = g_EnvironmentFoundp != NULL ? (int32_t)g_EnvironmentFoundp->Id : -1;
    out->main_id = g_EnvironmentMainp != NULL ? (int32_t)g_EnvironmentMainp->Id : -1;
    out->alternate_id = g_EnvironmentAltp != NULL ? (int32_t)g_EnvironmentAltp->Id : -1;
    viGetZRange(out->zrange);
    out->fog_enabled = g_FogSkyIsEnabled != 0;
    out->has_near_fog = g_NearFogValuesP != NULL;
    if (g_NearFogValuesP != NULL) {
        out->near_fog[0] = g_NearFogValuesP->NearFog;
        out->near_fog[1] = g_NearFogValuesP->MaxVisRange;
        out->near_fog[2] = g_NearFogValuesP->MaxObfuscationRange;
    }
    out->distances[0] = g_CurFogDetails.g_CurFogDetails;
    out->distances[1] = g_CurFogDetails.scaled_far_fog_dist;
    out->distances[2] = g_CurFogDetails.far_fog_dist_scaled;
    out->distances[3] = g_CurFogDetails.near_fog_dist_scaled;
    out->intensities[0] = g_FarFogIntensity;
    out->intensities[1] = g_DifferenceFromFarFogIntensity;
    out->intensities[2] = g_ScaledFarFogIntensity;
    out->intensities[3] = g_ScaledDifferenceFromFarFogIntensity;
    out->difference_from_far_intensity = g_CurrentEnvironment.DifferenceFromFarIntensity;
    out->far_intensity = g_CurrentEnvironment.FarIntensity;
    out->red = g_CurrentEnvironment.Red;
    out->green = g_CurrentEnvironment.Green;
    out->blue = g_CurrentEnvironment.Blue;
}
''']
    return '\n\n'.join(pieces) + '\n'

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('repo', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(generate(args.repo))
