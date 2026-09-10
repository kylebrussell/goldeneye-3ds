#ifndef GE_ORIGINAL_TEXTURE_HIT_TYPE_H
#define GE_ORIGINAL_TEXTURE_HIT_TYPE_H
#include <stdint.h>

/* Canonical US hitTexture nibble, including chraiDefaultWeaponFireHandler's
 * texture -1 read into the preceding original image.c data. Other invalid
 * indices return -1; callers must report a frontier rather than indexing. */
int32_t ge_original_texture_hit_type_exact(int32_t texture);
#endif
