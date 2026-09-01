#include "ge_gbi_clip.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(float left, float right)
{
    return fabsf(left - right) < 0.00001f;
}

static GeGbiProcessedVertex make_vertex(float x, float y, float z, float w,
                                        float texture_s, uint8_t red)
{
    GeGbiProcessedVertex vertex;

    memset(&vertex, 0, sizeof(vertex));
    vertex.clip[0] = x;
    vertex.clip[1] = y;
    vertex.clip[2] = z;
    vertex.clip[3] = w;
    vertex.ndc[0] = x / w;
    vertex.ndc[1] = y / w;
    vertex.ndc[2] = z / w;
    vertex.texture[0] = texture_s;
    vertex.rgba[0] = red;
    vertex.rgba[1] = 20U;
    vertex.rgba[2] = 30U;
    vertex.rgba[3] = 255U;
    vertex.has_ndc = 1U;
    vertex.has_screen = 1U;
    return vertex;
}

static void assert_inside(const GeGbiProcessedVertex *vertex)
{
    const float w = vertex->clip[3];

    assert(w > 0.0f);
    assert(vertex->clip[0] >= -w - 0.00001f);
    assert(vertex->clip[0] <= w + 0.00001f);
    assert(vertex->clip[1] >= -w - 0.00001f);
    assert(vertex->clip[1] <= w + 0.00001f);
    assert(vertex->clip[2] >= -w - 0.00001f);
    assert(vertex->clip[2] <= w + 0.00001f);
    assert(vertex->clip_flags == 0U);
    assert(vertex->has_ndc != 0U);
}

static void test_invalid_and_nonfinite_input(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;

    input[0] = make_vertex(0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0U);
    input[1] = input[0];
    input[2] = input[0];
    assert(ge_gbi_clip_triangle(NULL, &result)
           == GE_GBI_CLIP_INVALID_ARGUMENT);
    assert(ge_gbi_clip_triangle(input, NULL)
           == GE_GBI_CLIP_INVALID_ARGUMENT);
    input[1].clip[2] = NAN;
    assert(ge_gbi_clip_triangle(input, &result)
           == GE_GBI_CLIP_NONFINITE_INPUT);
    assert(result.triangle_count == 0U);
    input[1] = input[0];
    input[2].texture[0] = INFINITY;
    assert(ge_gbi_clip_triangle(input, &result)
           == GE_GBI_CLIP_NONFINITE_INPUT);
    assert(result.triangle_count == 0U);
}

static void test_inside_triangle_preserves_attributes(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;

    input[0] = make_vertex(-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 10U);
    input[1] = make_vertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 20U);
    input[2] = make_vertex(0.0f, 0.5f, 0.0f, 1.0f, 2.0f, 30U);

    assert(ge_gbi_clip_triangle(input, &result) == GE_GBI_CLIP_OK);
    assert(result.triangle_count == 1U);
    for (size_t index = 0U; index < 3U; ++index) {
        const GeGbiProcessedVertex *output
            = &result.triangles[0].vertices[index];

        assert(memcmp(output->clip, input[index].clip,
                      sizeof(output->clip)) == 0);
        assert(memcmp(output->texture, input[index].texture,
                      sizeof(output->texture)) == 0);
        assert(memcmp(output->rgba, input[index].rgba,
                      sizeof(output->rgba)) == 0);
        assert_inside(output);
        assert(output->has_screen == 0U);
    }
}

static void test_trivial_rejection(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;
    size_t index;

    for (index = 0U; index < 3U; ++index) {
        input[index] = make_vertex(-2.0f, (float)index * 0.25f, 0.0f,
                                   1.0f, (float)index, 10U);
        input[index].clip_flags = GE_GBI_CLIP_LEFT;
    }
    assert(ge_gbi_clip_triangle(input, &result) == GE_GBI_CLIP_OK);
    assert(result.triangle_count == 0U);
}

static void test_one_outside_vertex_becomes_two_triangles(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;
    size_t triangle_index;
    size_t boundary_vertices = 0U;

    input[0] = make_vertex(2.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0U);
    input[0].clip_flags = GE_GBI_CLIP_RIGHT;
    input[1] = make_vertex(0.0f, -1.0f, 0.0f, 1.0f, 2.0f, 100U);
    input[2] = make_vertex(0.0f, 1.0f, 0.0f, 1.0f, 4.0f, 200U);

    assert(ge_gbi_clip_triangle(input, &result) == GE_GBI_CLIP_OK);
    assert(result.triangle_count == 2U);
    for (triangle_index = 0U; triangle_index < result.triangle_count;
            ++triangle_index) {
        size_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
            const GeGbiProcessedVertex *vertex
                = &result.triangles[triangle_index].vertices[vertex_index];

            assert_inside(vertex);
            assert(vertex->has_screen == 0U);
            if (nearly_equal(vertex->clip[0], 1.0f)) {
                ++boundary_vertices;
                if (vertex->clip[1] < 0.0f) {
                    assert(nearly_equal(vertex->texture[0], 1.0f));
                    assert(vertex->rgba[0] == 50U);
                } else {
                    assert(nearly_equal(vertex->texture[0], 2.0f));
                    assert(vertex->rgba[0] == 100U);
                }
            }
        }
    }
    /* One fan endpoint is intentionally shared by both output triangles. */
    assert(boundary_vertices == 3U);
}

static void test_clips_multiple_planes_and_nonpositive_w(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;
    size_t triangle_index;

    input[0] = make_vertex(-3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 10U);
    input[0].clip_flags = GE_GBI_CLIP_LEFT;
    input[1] = make_vertex(3.0f, 0.0f, 0.0f, 1.0f, 1.0f, 20U);
    input[1].clip_flags = GE_GBI_CLIP_RIGHT;
    input[2] = make_vertex(0.0f, 0.5f, 0.0f, -0.5f, 2.0f, 30U);
    input[2].clip_flags = GE_GBI_CLIP_NONPOSITIVE_W;

    assert(ge_gbi_clip_triangle(input, &result) == GE_GBI_CLIP_OK);
    assert(result.triangle_count > 0U);
    assert(result.triangle_count <= GE_GBI_CLIP_MAX_TRIANGLES);
    for (triangle_index = 0U; triangle_index < result.triangle_count;
            ++triangle_index) {
        size_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
            assert_inside(
                &result.triangles[triangle_index].vertices[vertex_index]);
        }
    }
}

static void test_intersection_uses_homogeneous_distance(void)
{
    GeGbiProcessedVertex input[3];
    GeGbiClipResult result;
    size_t triangle_index;
    size_t intersection_count = 0U;

    /* The right-plane distances are -2 and +2, so both edges intersect at
     * t=0.5 even though their W values differ. */
    input[0] = make_vertex(3.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0U);
    input[1] = make_vertex(0.0f, -1.0f, 0.0f, 2.0f, 10.0f, 100U);
    input[2] = make_vertex(0.0f, 1.0f, 0.0f, 2.0f, 20.0f, 200U);

    assert(ge_gbi_clip_triangle(input, &result) == GE_GBI_CLIP_OK);
    assert(result.triangle_count == 2U);
    for (triangle_index = 0U; triangle_index < result.triangle_count;
            ++triangle_index) {
        size_t vertex_index;

        for (vertex_index = 0U; vertex_index < 3U; ++vertex_index) {
            const GeGbiProcessedVertex *vertex
                = &result.triangles[triangle_index].vertices[vertex_index];

            if (nearly_equal(vertex->clip[0], vertex->clip[3])) {
                ++intersection_count;
                assert(nearly_equal(vertex->clip[0], 1.5f));
                assert(nearly_equal(vertex->ndc[0], 1.0f));
                assert(nearly_equal(vertex->texture[0], 5.0f)
                       || nearly_equal(vertex->texture[0], 10.0f));
            }
        }
    }
    assert(intersection_count == 3U);
}

int main(void)
{
    test_invalid_and_nonfinite_input();
    test_inside_triangle_preserves_attributes();
    test_trivial_rejection();
    test_one_outside_vertex_becomes_two_triangles();
    test_clips_multiple_planes_and_nonpositive_w();
    test_intersection_uses_homogeneous_distance();
    puts("ge_gbi_clip tests passed");
    return 0;
}
