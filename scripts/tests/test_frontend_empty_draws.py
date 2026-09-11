#!/usr/bin/env python3
"""Execute the production frontend's optional draw gates at empty boundaries."""
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'platform/3ds/source/main.c').read_text()
source = source[source.index('static void draw_original_frontend_list('):]
source = source[:source.index('static Ge3dsOriginalFrontendPage original_frontend_page(')]
gates = []
for condition in (
    'draw_list->background_vertex_count != 0U',
    'draw_list->box_vertex_count > draw_list->background_vertex_count',
    'normal_glyph_count != 0U',
):
    match = re.search(r'if \(' + re.escape(condition)
                      + r'\)\s*C3D_DrawArrays\([^;]+;', source)
    assert match, condition
    gates.append(match.group())

unit = r'''
#include <assert.h>
#include <stddef.h>
#define GPU_TRIANGLES 0
#define ORIGINAL_HUD_VERTEX_OFFSET 100
static unsigned calls;
static void C3D_DrawArrays(int primitive, size_t first, size_t count) {
    assert(primitive == 0 && first >= 100 && count > 0 && count % 3 == 0);
    ++calls;
}
typedef struct {size_t background_vertex_count, box_vertex_count;} List;
static void emit(const List *draw_list, size_t normal_glyph_count) {
''' + '\n'.join(gates) + r'''
}
int main(void) {
    for (size_t background = 0; background <= 12; background += 6)
    for (size_t boxes = background; boxes <= 24; boxes += 6)
    for (size_t glyphs = 0; glyphs <= 24; glyphs += 6) {
        List list = {background, boxes}; calls = 0; emit(&list, glyphs);
        assert(calls == (background > 0) + (boxes > background) + (glyphs > 0));
    }
    return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(path / 'test.c'),
                    '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('Frontend optional draws: empty submissions omitted; nonempty ranges retained')
