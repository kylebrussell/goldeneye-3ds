#include "ge_original_gun_sight.h"
#include "ge_gbi_pipeline.h"
#include <string.h>

typedef struct GeSightDecode {
    const GeOriginalGunSightSnapshot *snapshot;
    GePicaTextureRectangleDraw *draw;
    uint8_t count;
} GeSightDecode;

static int sight_draw(const GeGbiPipelineEvent *event, void *context)
{
    GeSightDecode *capture = context;
    const GeOriginalGunSightSnapshot *snapshot = capture->snapshot;
    GeGbiRenderState state;
    GePicaTextureBindingTransform binding;
    if (event->action.kind != GE_GBI_STATE_ACTION_DRAW_TEXTURE_RECTANGLE)
        return 1;
    if (capture->count != 0U || !snapshot->selected) return 0;
    state = *event->state;
    /* texSelect's native binding supplies the converted authored RGBA image;
     * the retained display_image body supplies combiner/tint/opacity. */
    state.texture.enabled = 1U;
    state.rare_texture_valid = 1U;
    state.rare_texture.texture_id = snapshot->image_id;
    state.tiles[0].format = snapshot->format;
    state.tiles[0].size = snapshot->depth;
    state.tiles[0].clamp_mirror_s = snapshot->flags_s;
    state.tiles[0].clamp_mirror_t = snapshot->flags_t;
    binding = (GePicaTextureBindingTransform){
        1.0f / (float)snapshot->width, 1.0f / (float)snapshot->height, 0.0f, 0.0f
    };
    if (ge_pica_texture_rectangle_translate_action(
            &state, &event->action, &binding, capture->draw) != GE_PICA_MATERIAL_OK)
        return 0;
    ++capture->count;
    return 1;
}

int ge_original_gun_sight_build_draw(const GeOriginalGunSightSnapshot *snapshot,
    GePicaTextureRectangleDraw *draw, uint8_t *visible)
{
    uint8_t bytes[GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY * 8U];
    GeGbiMemoryMap memory;
    const GeGbiTraversalConfig config = {1U, GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY};
    GeGbiPipelineResult result;
    GeSightDecode capture = {snapshot, draw, 0U};
    size_t index;
    if (visible != NULL) *visible = 0U;
    if (snapshot == NULL || draw == NULL || visible == NULL
            || snapshot->command_count == 0U
            || snapshot->command_count > GE_ORIGINAL_GUN_SIGHT_COMMAND_CAPACITY)
        return 0;
    memset(draw, 0, sizeof(*draw));
    if (snapshot->selected && (snapshot->width == 0U || snapshot->height == 0U
            || snapshot->level != 0U || snapshot->alpha_mode != 4U
            || snapshot->texture_mode != 0U || snapshot->texture_offset != 0))
        return 0;
    for (index = 0U; index < snapshot->command_count * 2U; ++index) {
        const uint32_t word = snapshot->commands[index / 2U][index % 2U];
        bytes[index * 4U] = (uint8_t)(word >> 24);
        bytes[index * 4U + 1U] = (uint8_t)(word >> 16);
        bytes[index * 4U + 2U] = (uint8_t)(word >> 8);
        bytes[index * 4U + 3U] = (uint8_t)word;
    }
    ge_gbi_memory_map_init(&memory);
    if (ge_gbi_memory_map_set_segment(&memory, 1U, bytes,
            snapshot->command_count * 8U) != GE_GBI_RESOLVE_OK) return 0;
    result = ge_gbi_pipeline_execute(&memory,
        (GeGbiAddress){UINT32_C(0x01000000), 0U, 1U},
        GE_GBI_BYTE_ORDER_BIG_ENDIAN, &config, sight_draw, &capture);
    if (result.status != GE_GBI_PIPELINE_OK || result.unsupported_commands != 0U)
        return 0;
    *visible = capture.count;
    return 1;
}
