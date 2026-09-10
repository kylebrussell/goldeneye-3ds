#include "ge_original_texture_hit_type.h"
#include <ultra64.h>
#include <bondconstants.h>
#include "game/image.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

struct image_entry g_Textures[] = {
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) {HS, HT, SZ, F3, F4, F5, F6},
#include <assets/images.def>
#undef IMAGE
    {HIT_DEFAULT, HIT_DEFAULT, 0xFFFF, 0, 0, 0, 0}
};

int main(void)
{
    const size_t count = sizeof(g_Textures) / sizeof(g_Textures[0]);
    assert(ge_original_texture_hit_type_exact(-1) == 0);
    assert(ge_original_texture_hit_type_exact(-2) == -1);
    assert(ge_original_texture_hit_type_exact(INT_MIN) == -1);
    assert(ge_original_texture_hit_type_exact(INT_MAX) == -1);
    for (size_t i = 0; i < count; ++i)
        assert(ge_original_texture_hit_type_exact((int32_t)i) == (int32_t)g_Textures[i].hitTexture);
    assert(ge_original_texture_hit_type_exact((int32_t)count) == -1);
    puts("texture hit type: every authored entry/sentinel and original US -1 predecessor, bounded native lookup passed");
    return 0;
}
