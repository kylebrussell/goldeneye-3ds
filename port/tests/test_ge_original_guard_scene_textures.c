#include "ge_original_model_scene.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

typedef struct TextureVisit {
    uint16_t ids[4];
    size_t count;
    uint16_t reject;
} TextureVisit;

static int visit_texture(void *context, uint16_t texture_id)
{
    TextureVisit *visit = context;
    assert(visit != NULL && visit->count < 4U);
    visit->ids[visit->count++] = texture_id;
    return texture_id != visit->reject;
}

int main(void)
{
    GeDamRoomDrawBatch batches[6];
    TextureVisit visit;

    memset(batches, 0, sizeof(batches));
    batches[0].texture_valid = 1U;
    batches[0].texture.texture_id = 17U;
    batches[0].material.texture_enabled = 1U;
    batches[0].material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    batches[1].texture_valid = 1U;
    batches[1].texture.texture_id = 42U;
    batches[1].material.texture_enabled = 1U;
    batches[1].material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    batches[2].texture_valid = 1U;
    batches[2].texture.texture_id = 17U;
    batches[2].material.texture_enabled = 1U;
    batches[2].material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    batches[3].texture.texture_id = 99U; /* authored fallback, no image */
    batches[4].texture_valid = 1U;
    batches[4].texture.texture_id = 73U;
    batches[4].material.texture_enabled = 1U;
    batches[4].material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;
    batches[5].texture_valid = 1U;
    batches[5].texture.texture_id = 42U;
    batches[5].material.texture_enabled = 1U;
    batches[5].material.texture_source = GE_PICA_TEXTURE_SOURCE_RARE_ID;

    memset(&visit, 0, sizeof(visit));
    visit.reject = UINT16_MAX;
    assert(ge_original_model_scene_visit_textures(
        batches, 6U, &visit, visit_texture));
    assert(visit.count == 3U);
    assert(visit.ids[0] == 17U && visit.ids[1] == 42U
        && visit.ids[2] == 73U);

    memset(&visit, 0, sizeof(visit));
    visit.reject = 42U;
    assert(!ge_original_model_scene_visit_textures(
        batches, 6U, &visit, visit_texture));
    assert(visit.count == 2U && visit.ids[1] == 42U);
    assert(ge_original_model_scene_visit_textures(
        NULL, 0U, &visit, visit_texture));
    assert(!ge_original_model_scene_visit_textures(
        NULL, 1U, &visit, visit_texture));
    return 0;
}
