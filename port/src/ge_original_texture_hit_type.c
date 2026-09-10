#include "ge_original_texture_hit_type.h"
#include <ultra64.h>
#include "game/image.h"

enum {
    GE_ORIGINAL_TEXTURE_COUNT = 0
#define IMAGE(...) + 1
#include <assets/images.def>
#undef IMAGE
};

/* Exact src/game/image.c D_800492E4 initializer. Original US map/ELF place
 * these seven words at 0x800492E4, directly before g_Textures at0x80049300.
 * The source texture -1 byte at g_Textures-8 is the high byte of word5.
 * Preserve that data lookup without relying on native linker adjacency. */
static const uint32_t ge_original_image_prefix[7] = {0, 0, 0, 0, 0, 0, 0};

int32_t ge_original_texture_hit_type_exact(int32_t texture)
{
    if (texture == -1)
        return (int32_t)((ge_original_image_prefix[5] >> 24) & 0xfU);
    if (texture < 0 || texture > GE_ORIGINAL_TEXTURE_COUNT) return -1;
    /* The final entry is the original image.c sentinel, not a new default. */
    return (int32_t)g_Textures[texture].hitTexture;
}
