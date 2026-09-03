#ifndef GE_ORIGINAL_GUN_SIGHT_H
#define GE_ORIGINAL_GUN_SIGHT_H

#include <stddef.h>
#include <stdint.h>
#include "ge_pica_material.h"

#define GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY 16U

typedef struct GeOriginalGunSightSnapshot {
    uint32_t commands[GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY][2];
    size_t command_count;
    uint32_t suppression_reasons;
    uint16_t image_id;
    uint8_t width, height, level, format, depth, flags_s, flags_t;
    uint8_t alpha_mode, texture_mode, selected;
    int32_t texture_offset;
} GeOriginalGunSightSnapshot;

/* Executes the original gunDrawSight -> display_image_at_position ->
 * draw_textured_rectangle chain. Only texSelect's platform texture binding
 * is captured separately from the exact generated RDP commands. */
int ge_original_gun_sight_snapshot(GeOriginalGunSightSnapshot *snapshot);
const char *ge_original_gun_sight_texture_source(void);

/* Decode the original rectangle into normalized texture coordinates for the
 * platform's actual Tex3DS subtexture. An invisible sight yields visible=0. */
int ge_original_gun_sight_build_draw(const GeOriginalGunSightSnapshot *snapshot,
    GePicaTextureRectangleDraw *draw, uint8_t *visible);

#endif
