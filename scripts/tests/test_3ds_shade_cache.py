#!/usr/bin/env python3
"""Compare complete shaded vertex bytes against the original scalar shader."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
test = r'''
#include "ge_gbi_vertex.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned scalar_calls;
static GeGbiVertexProcessStatus counted_shade(const GeGbiRenderState *s,
    const GeGbiVertex *v, GeGbiProcessedVertex *p) {
    ++scalar_calls; return ge_gbi_vertex_shade(s,v,p);
}
#define ge_gbi_vertex_shade counted_shade
#include "ge_3ds_shade_cache.h"
#undef ge_gbi_vertex_shade
static uint32_t rng=92187;
static uint32_t next(void) {rng=rng*1664525U+1013904223U;return rng;}
int main(void) {
    GeGbiRenderState state;
    Ge3dsShadeCache cache;
    unsigned vertices=0;
    for (unsigned batch=0;batch<3000;++batch) {
        ge_gbi_state_init(&state);
        state.geometry_mode=(batch%5 ? GE_GBI_GEOMETRY_LIGHTING:0U)
            | (batch%3 ? GE_GBI_GEOMETRY_TEXTURE_GEN:0U)
            | (batch%2 ? GE_GBI_GEOMETRY_TEXTURE_GEN_LINEAR:0U);
        state.valid_look_at=(uint8_t)(batch%7 ? 3:batch%4);
        state.directional_light_count=(uint8_t)(batch%9);
        state.valid_lights=(uint8_t)(next()>>24);
        for(unsigned i=0;i<GE_GBI_LIGHT_COUNT;++i)
            for(unsigned axis=0;axis<3;++axis) {
                state.lights[i].direction[axis]=batch%11 ? (int8_t)next():0;
                state.lights[i].color[axis]=(uint8_t)next();
            }
        for(unsigned i=0;i<2;++i)for(unsigned axis=0;axis<3;++axis)
            state.look_at[i].direction[axis]=batch%13 ? (int8_t)next():0;
        for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)
            state.modelview_stack.entries[0].elements[i][j]=batch%17
                ? (float)((int)(next()%4097)-2048)/1024.0f:0.0f;
        /* Reset includes changing an existing state object in place. */
        ge_3ds_shade_cache_reset(&cache,&state);
        uint32_t colors[12];
        for(unsigned i=0;i<12;++i)colors[i]=batch%19 ? next():0U;
        for(unsigned i=0;i<96;++i) {
            GeGbiVertex vertex={0};
            uint32_t color=colors[i%(batch%2 ? 4U:12U)];
            vertex.red=(uint8_t)color;vertex.green=(uint8_t)(color>>8);
            vertex.blue=(uint8_t)(color>>16);vertex.alpha=(uint8_t)(color>>24);
            vertex.x=(int16_t)next();vertex.y=(int16_t)next();vertex.z=(int16_t)next();
            vertex.texture_s=(int16_t)(batch%2 ? next():i%3);
            vertex.texture_t=(int16_t)(batch%2 ? next():i%3);
            GeGbiVertex original=vertex;
            GeGbiProcessedVertex actual,expected;
            for(size_t b=0;b<sizeof(actual);++b)((uint8_t*)&actual)[b]=(uint8_t)next();
            memcpy(&expected,&actual,sizeof(actual));
            assert(ge_3ds_shade_cached(&cache,&vertex,&actual)
                ==ge_gbi_vertex_shade(&state,&vertex,&expected));
            assert(memcmp(&actual,&expected,sizeof(actual))==0);
            assert(memcmp(&original,&vertex,sizeof(vertex))==0);
            ++vertices;
        }
    }
    GeGbiVertex vertex={0};GeGbiProcessedVertex actual,expected;
    memset(&actual,0xa5,sizeof(actual));expected=actual;
    state.modelview_stack.count=0;
    ge_3ds_shade_cache_reset(&cache,&state);
    assert(ge_3ds_shade_cached(&cache,&vertex,&actual)==GE_GBI_VERTEX_PROCESS_INVALID_STATE);
    assert(memcmp(&actual,&expected,sizeof(actual))==0 && cache.count==0);
    assert(ge_3ds_shade_cached(NULL,&vertex,&actual)==GE_GBI_VERTEX_PROCESS_INVALID_ARGUMENT);
    assert(scalar_calls<vertices);
    printf("Shade cache: %u byte-exact vertices; %u scalar calls avoided; lights, normals, ST, cache eviction, state resets and errors covered\n",vertices,vertices-(scalar_calls-1));
}
'''
with tempfile.TemporaryDirectory(prefix='ge-shade-cache-') as temporary:
    p = Path(temporary)
    (p/'test.c').write_text(test)
    sources = ['ge_gbi_matrix.c', 'ge_gbi_state.c', 'ge_gbi_vertex.c']
    subprocess.run(['cc', '-std=c11', '-O3', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I'+str(repo/'port/include'),
                    '-I'+str(repo/'platform/3ds/include'), str(p/'test.c'),
                    *[str(repo/'port/src'/s) for s in sources], '-lm',
                    '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True)

main = (repo/'platform/3ds/source/main.c').read_text()
region = main[main.index('static bool refresh_stage_windowed_door_shading('):
              main.index('static bool refresh_stage_live_overlays(')]
assert region.count('ge_3ds_shade_cache_reset(&shade_cache, &shading)') == 2
assert region.count('ge_3ds_shade_cached(&shade_cache, &source->source, &shaded)') == 2
assert 'active_matrix = matrix;\n                    ge_3ds_shade_cache_reset' in region
