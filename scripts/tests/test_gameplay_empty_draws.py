#!/usr/bin/env python3
"""Execute production HUD draw gates across empty watch/message transitions."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'platform/3ds/source/main.c').read_text().split('static void renderer_draw(',1)[1]
fields=['bottom_hud_draw.glyph_vertex_count','mission_hud_draw.box_vertex_count','mission_hud_draw.glyph_vertex_count']
gates=[]
for field in fields:
    match=re.search(r'if \('+re.escape(field)+r' != 0U\)\s*C3D_DrawArrays\([^;]+;',source)
    assert match,field
    gates.append(match.group())
unit=r'''
#include <assert.h>
#include <stddef.h>
#define GPU_TRIANGLES 0
#define ORIGINAL_BOTTOM_HUD_VERTEX_OFFSET 100
#define ORIGINAL_HUD_VERTEX_OFFSET 200
static unsigned calls;
static void C3D_DrawArrays(int primitive,size_t start,size_t count) {
    assert(primitive==0 && start>=100 && count>0 && count%3==0);++calls;
}
typedef struct {size_t box_vertex_count,glyph_vertex_count;} Draw;
static void draw(Draw bottom_hud_draw,Draw mission_hud_draw) { GATES }
int main(void) {
 for(size_t boxes=0;boxes<=12;boxes+=6)
 for(size_t glyphs=0;glyphs<=12;glyphs+=6)
 for(size_t bottom=0;bottom<=12;bottom+=6) {
    calls=0;draw((Draw){0,bottom},(Draw){boxes,glyphs});
    assert(calls==(boxes>0)+(glyphs>0)+(bottom>0));
 }
 return 0;
}
'''.replace('GATES','\n'.join(gates))
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Gameplay HUD: empty watch/message draws suppressed; all nonempty boxes/glyphs retained')
