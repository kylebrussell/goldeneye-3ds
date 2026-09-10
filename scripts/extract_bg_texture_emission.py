"""Native original tex.c command emission for BG hit tile provenance."""
from pathlib import Path
import hashlib
import re
from extract_guard_bullet_hit_slice import find_function

FUNCTIONS = (
    'sub_GAME_7F0CC4C8', 'texSetLutMode', 'texTrySetTileState',
    'texTrySetTileSize', 'texGetWidthAtLod', 'texGetHeightAtLod',
    'texGetLineSizeInBytes', 'texGetSizeInBytes', 'texGetDepthAndSize',
    'texDimensionToMask', 'texModeToGbiMode', 'sub_GAME_7F0CCAFC',
    'sub_GAME_7F0CCB38', 'texWriteTileFromDefinition',
    'texWriteLoadToTmemAddr', 'texWriteTileLods', 'texWriteLoadToTmemZero',
    'texWriteTile', 'texHandleType2', 'texHandleType1', 'texHandleType0',
    'texHandleType4', 'texHandleType3',
)

def generate(repo: Path) -> str:
    source=(repo/'src/game/tex.c').read_text()
    names={name:'ge_bg_emit_'+name for name in FUNCTIONS}
    names.update({name:'ge_bg_emit_'+name for name in (
        'g_TexLutMode','g_TexTileStates','g_TexTileSizes',
        'g_TexCacheItems','g_TexCacheCount','g_Textures')})
    out=['''#include "game/tex.h"
#include <string.h>
#include "ge_original_bg_texture_metadata.inc"
_Static_assert(sizeof(ge_bg_texture_metadata) / sizeof(ge_bg_texture_metadata[0])
    == GE_BG_TEXTURE_COUNT, "Every original texture must have verified metadata");
static s32 ge_bg_emit_g_TexLutMode;
static s_bss_8008D148 ge_bg_emit_g_TexTileStates[8];
static s_bss_8008D2A8 ge_bg_emit_g_TexTileSizes[8];
static struct texcacheitem ge_bg_emit_g_TexCacheItems[2];
static s32 ge_bg_emit_g_TexCacheCount;
static struct image_entry ge_bg_emit_g_Textures[] = {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) {HS, HT, SZ, F3, F4, F5, F6},
#include <assets/images.def>
#undef IMAGE
    {HIT_DEFAULT, HIT_DEFAULT, 0xFFFF, 0, 0, 0, 0}
};
''']
    bodies=[]
    for name in FUNCTIONS:
        body=find_function(source,name)
        if name=='sub_GAME_7F0CCB38':
            body=body.replace('s32 *arg0','struct tex *arg0').replace('arg0[2] & 3','arg0->depth')
        if name=='texWriteLoadToTmemZero': body=body.replace('(s32 *)tex','tex')
        if name in ('texHandleType3','texHandleType4'): body=body.replace('u32 *arg1','struct tex *arg1')
        if name=='texWriteTileLods': body=body.replace('    s32 pad2;', '    s32 pad2 = basetile; /* No-LOD dead temporary. */')
        for old,new in names.items(): body=re.sub(r'\b'+old+r'\b',new,body)
        bodies.append(body)
        out.append('static '+body[:body.index('{')].strip()+';')
    out += ['static '+body for body in bodies]
    out.insert(0,'/* Original tex emission closure SHA-256: '+hashlib.sha256(''.join(find_function(source,n) for n in FUNCTIONS).encode()).hexdigest()+' */')
    out.append('''
static void ge_bg_emission_texture(uint32_t id, unsigned slot, struct tex *texture)
{
    const GeBgTextureMetadata *data = &ge_bg_texture_metadata[id];
    memset(texture, 0, sizeof(*texture));
    texture->texturenum = id;
    /* An opaque emission token, never dereferenced or sent to a renderer.
     * Only the emitted G_SETTILE bits are consumed by this closure. */
    texture->data = (u8 *)(uintptr_t)(0x1000U + id * 8U);
    texture->width = data->widths[0]; texture->height = data->heights[0];
    texture->unk0a = data->palette;
    texture->maxlod = data->maxlod;
    texture->gbiformat = ge_bg_texture_gbi_formats[data->format];
    texture->depth = ge_bg_texture_gbi_depths[data->format];
    texture->lutmodeindex = ge_bg_texture_lut_modes[data->format] >> G_MDSFT_TEXTLUT;
    texture->hasExplicitLods = data->explicit_lods;
    ge_bg_emit_g_TexCacheItems[slot].texturenum = (s16)id;
    memcpy(ge_bg_emit_g_TexCacheItems[slot].widths, data->widths + 1, 7);
    memcpy(ge_bg_emit_g_TexCacheItems[slot].heights, data->heights + 1, 7);
}

void ge_bg_texture_emission_begin(void)
{ ge_bg_emit_sub_GAME_7F0CC4C8(); }

int ge_bg_texture_emission_command(uint32_t word0, uint32_t word1,
    int8_t *format, int8_t *depth)
{
    const uint32_t id = word1 & 0xfffU, detail = (word1 >> 12) & 0xfffU;
    const unsigned type = word0 & 7U;
    const int smode = (word0 >> 22) & 3U, tmode = (word0 >> 20) & 3U;
    const int offset = (word0 >> 18) & 3U;
    const unsigned shifts = (word0 >> 14) & 15U, shiftt = (word0 >> 10) & 15U;
    struct tex texture, detail_texture;
    Gfx commands[256], *end = commands;
    if (id >= GE_BG_TEXTURE_COUNT || type > 4U
            || (type == 1U && detail >= GE_BG_TEXTURE_COUNT)) return 0;
    ge_bg_emission_texture(id,0,&texture);
    ge_bg_emit_g_TexCacheCount = 1;
    switch (type) {
    case 0: end = ge_bg_emit_texHandleType0(end,&texture,smode,tmode,offset,shifts,shiftt,word1 >> 24); break;
    case 1:
        ge_bg_emission_texture(detail,1,&detail_texture);
        ge_bg_emit_g_TexCacheCount = 2;
        end = ge_bg_emit_texHandleType1(end,&texture,smode,tmode,offset,&detail_texture,shifts,shiftt,word1 >> 24);
        break;
    case 2: end = ge_bg_emit_texHandleType2(end,&texture,smode,tmode,offset); break;
    case 3: end = ge_bg_emit_texHandleType3(end,&texture,smode,tmode,offset); break;
    case 4: end = ge_bg_emit_texHandleType4(end,&texture,smode,tmode,offset); break;
    }
    for (Gfx *command = commands; command < end; ++command) {
        const uint32_t word = (uint32_t)command->words.w0;
        if ((word >> 24) == 0xf5U) {
            *format = (int8_t)((word >> 21) & 7U);
            *depth = (int8_t)((word >> 19) & 3U);
        }
    }
    /* Original texLoadFromGdl appends a called water setup list (not walked
     * by the linear CPU hit scan) and resets its texture-emission cache. */
    if (id == 1508U || id == 1511U) ge_bg_emit_sub_GAME_7F0CC4C8();
    return 1;
}
''')
    return '\n\n'.join(out)+'\n'
