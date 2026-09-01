#include "ge_dam_sky.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void assert_near(float actual, float expected, float tolerance)
{
    assert(fabsf(actual - expected) <= tolerance);
}

static GeDamSkyCamera default_camera(void)
{
    GeDamSkyCamera camera = {
        {4719.0f / 0.23363999f, -18.0f / 0.23363999f,
         3949.0f / 0.23363999f},
        {-1.0f, 0.0f, -0.000643f},
        {0.0f, 1.0f, 0.0f},
        40.0f, 0.0f, 320.0f, 240.0f, 60.0f, 4.0f / 3.0f,
    };
    return camera;
}

int main(void)
{
    GeDamSkyCamera camera = default_camera();
    GeDamSkyScene scene;
    float offset = 4095.0f;
    size_t index;
    int saw_horizon = 0;
    int saw_cloud = 0;

    ge_dam_sky_tick(&offset, 2);
    assert_near(offset, 1.0f, 0.0001f);
    assert(ge_dam_sky_build(&camera, offset, &scene));
    assert(scene.polygon_vertex_count == 4U);
    assert(scene.vertex_count == 6U);
    for (index = 0U; index < scene.vertex_count; ++index) {
        const GeDamSkyVertex *vertex = &scene.vertices[index];
        assert(vertex->screen_x >= 40.0f && vertex->screen_x <= 360.0f);
        assert(vertex->screen_y >= 0.0f && vertex->screen_y <= 120.001f);
        assert(isfinite(vertex->texture_u));
        assert(isfinite(vertex->texture_v));
        assert(vertex->red >= 16.0f / 255.0f && vertex->red <= 1.0f);
        assert(vertex->green >= 48.0f / 255.0f && vertex->green <= 1.0f);
        assert(vertex->blue >= 96.0f / 255.0f && vertex->blue <= 1.0f);
        if (fabsf(vertex->screen_y - 120.0f) < 0.01f) {
            saw_horizon = 1;
            assert_near(vertex->red, 16.0f / 255.0f, 0.001f);
            assert_near(vertex->green, 48.0f / 255.0f, 0.001f);
            assert_near(vertex->blue, 96.0f / 255.0f, 0.001f);
        } else if (vertex->screen_y < 1.0f) {
            saw_cloud = 1;
            assert(vertex->red > 16.0f / 255.0f);
        }
    }
    assert(saw_horizon && saw_cloud);

    camera.look[1] = -1.0f;
    camera.look[0] = 0.0f;
    camera.look[2] = 0.0f;
    camera.up[0] = -1.0f;
    camera.up[1] = 0.0f;
    assert(ge_dam_sky_build(&camera, offset, &scene));
    assert(scene.vertex_count == 0U);

    puts("Canonical Dam cloud plane passed");
    return 0;
}
