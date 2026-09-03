#include "ge_original_gun_sight.h"
#include "ge_pica_apply.h"
#include "bondtypes.h"
#include "game/bondview.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static struct player player;
struct player *g_CurrentPlayer = &player;
static SCREEN_RATIO_OPTION ratio = SCREEN_RATIO_NORMAL;
SCREEN_RATIO_OPTION get_screen_ratio(void) { return ratio; }

int main(void)
{
    GeOriginalGunSightSnapshot snapshot;
    GePicaTextureRectangleDraw draw;
    GePicaApplyState applied;
    uint8_t visible;
    size_t bit;
    player.crosshair_angle.f[0] = 200.0f;
    player.crosshair_angle.f[1] = 120.0f;
    assert(ge_original_gun_sight_snapshot(&snapshot));
    assert(snapshot.selected && snapshot.width == 32U && snapshot.height == 32U);
    assert(strcmp(ge_original_gun_sight_texture_source(), "CROSSHAIR1.bin") == 0);
    assert(ge_original_gun_sight_build_draw(&snapshot, &draw, &visible) && visible);
    assert(draw.vertices[0].x == 184.0f && draw.vertices[0].y == 104.0f);
    assert(draw.vertices[2].x == 216.0f && draw.vertices[2].y == 136.0f);
    assert(draw.vertices[0].texture_u == 0.0f);
    assert(draw.vertices[0].texture_v == 31.0f / 32.0f);
    assert(draw.vertices[2].texture_v == -1.0f / 32.0f);
    assert(draw.material.blend_enabled && !draw.material.depth_test_enabled);
    assert(draw.material.min_filter == GE_PICA_FILTER_LINEAR
        && draw.material.mag_filter == GE_PICA_FILTER_LINEAR);
    assert(!(draw.material.fallback_flags & GE_PICA_FALLBACK_COMBINER));
    assert(draw.material.color_combine == GE_PICA_COMBINE_TEXTURE0_MODULATE_ENVIRONMENT);
    assert(draw.material.alpha_combine == GE_PICA_ALPHA_TEXTURE0_MODULATE_ENVIRONMENT);
    assert(ge_pica_apply_compile(&draw.material, &applied) == GE_PICA_APPLY_OK);
    assert(applied.color.combine == GE_PICA_APPLY_MODULATE
        && applied.color.source0 == GE_PICA_APPLY_SOURCE_TEXTURE0
        && applied.color.source1 == GE_PICA_APPLY_SOURCE_CONSTANT);
    assert(applied.alpha.combine == GE_PICA_APPLY_MODULATE
        && applied.alpha.source0 == GE_PICA_APPLY_SOURCE_TEXTURE0
        && applied.alpha.source1 == GE_PICA_APPLY_SOURCE_CONSTANT);
    assert(applied.constant_color.red == 255U && applied.constant_color.green == 255U
        && applied.constant_color.blue == 255U && applied.constant_color.alpha == 110U);
    assert(draw.material.environment_color.red == 255U
        && draw.material.environment_color.green == 255U
        && draw.material.environment_color.blue == 255U
        && draw.material.environment_color.alpha == 110U);
    ratio = SCREEN_RATIO_16_9;
    assert(ge_original_gun_sight_snapshot(&snapshot));
    assert(ge_original_gun_sight_build_draw(&snapshot, &draw, &visible) && visible);
    assert(draw.vertices[0].x == 188.0f && draw.vertices[2].x == 212.0f);
    ratio = SCREEN_RATIO_NORMAL;
    for (bit = 0U; bit < 31U; ++bit) {
        player.gunsightmode = (int32_t)(UINT32_C(1) << bit);
        assert(ge_original_gun_sight_snapshot(&snapshot));
        assert(!snapshot.selected && snapshot.suppression_reasons == (UINT32_C(1) << bit));
        assert(ge_original_gun_sight_build_draw(&snapshot, &draw, &visible) && !visible);
    }
    player.gunsightmode = 0;
    player.mpmenuon = 1;
    assert(ge_original_gun_sight_snapshot(&snapshot) && !snapshot.selected);
    player.mpmenuon = 0;
    player.crosshair_angle.f[0] = 8.0f;
    player.crosshair_angle.f[1] = 12.0f;
    assert(ge_original_gun_sight_snapshot(&snapshot));
    assert(ge_original_gun_sight_build_draw(&snapshot, &draw, &visible) && visible);
    assert(draw.vertices[0].x == 0.0f && draw.vertices[0].y == 0.0f);
    assert(draw.vertices[0].texture_u == 0.25f);
    assert(draw.vertices[0].texture_v == 27.0f / 32.0f);
    player.crosshair_angle.f[0] = -40.0f;
    assert(ge_original_gun_sight_snapshot(&snapshot));
    assert(ge_original_gun_sight_build_draw(&snapshot, &draw, &visible) && !visible);
    player.crosshair_angle.f[0] = NAN;
    assert(!ge_original_gun_sight_snapshot(&snapshot));
    player.crosshair_angle.f[0] = FLT_MAX;
    assert(!ge_original_gun_sight_snapshot(&snapshot));
    player.crosshair_angle.f[0] = -FLT_MAX;
    assert(!ge_original_gun_sight_snapshot(&snapshot));
    g_CurrentPlayer = NULL;
    assert(!ge_original_gun_sight_snapshot(&snapshot));
    assert(!ge_original_gun_sight_snapshot(NULL));
    puts("canonical sight: authored RGBA image/110 alpha, moving aim, 4:3/16:9, "
         "31 suppression bits, MP menu and exact clipped/flipped RDP rectangle passed");
    return 0;
}
