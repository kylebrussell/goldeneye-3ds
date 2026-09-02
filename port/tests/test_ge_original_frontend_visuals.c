#include "ge_original_frontend_visuals.h"
#include "ge_original_rareware_logo.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int close_float(float left, float right)
{
    return fabsf(left - right) < 0.00001f;
}

int main(int argc, char **argv)
{
    static const uint8_t ambient[3] = {0x96, 0x96, 0x96};
    static const uint8_t diffuse[3] = {0xff, 0xff, 0xff};
    static const int8_t direction[3] = {77, 77, 46};
    GeOriginalFrontendGeneratedVertex vertex;
    float uv[2];
    float authored[3];
    float projected[3];
    uint8_t normal[3];

    normal[0] = (uint8_t)77;
    normal[1] = (uint8_t)77;
    normal[2] = (uint8_t)46;
    assert(ge_original_frontend_generate_lit_vertex(
        normal, 0x80U, 0.0f, ambient, diffuse, direction, &vertex));
    assert(vertex.lit_rgba[0] == 255U && vertex.lit_rgba[1] == 255U
           && vertex.lit_rgba[2] == 255U && vertex.lit_rgba[3] == 0x80U);

    normal[0] = (uint8_t)-77;
    normal[1] = (uint8_t)-77;
    normal[2] = (uint8_t)-46;
    assert(ge_original_frontend_generate_lit_vertex(
        normal, 0xffU, 0.0f, ambient, diffuse, direction, &vertex));
    assert(vertex.lit_rgba[0] == 150U && vertex.lit_rgba[1] == 150U
           && vertex.lit_rgba[2] == 150U);

    normal[0] = 0U;
    normal[1] = 0U;
    normal[2] = 127U;
    assert(ge_original_frontend_generate_lit_vertex(
        normal, 0xffU, 0.0f, ambient, diffuse, direction, &vertex));
    /* Exact white gelogolight accumulation, rather than the former 105-point
     * diffuse approximation and hand-authored gold multiplier. */
    assert(vertex.lit_rgba[0] == 249U && vertex.lit_rgba[1] == 249U
           && vertex.lit_rgba[2] == 249U);
    assert(close_float(vertex.generated_uv[0], 0.5f));
    assert(close_float(vertex.generated_uv[1], 0.5f));

    assert(ge_original_frontend_generate_lit_vertex(
        normal, 0xffU, -40.0f * 3.14159265358979323846f / 180.0f,
        ambient, diffuse, direction, &vertex));
    assert(close_float(vertex.normal[0], -0.64278761f));
    assert(close_float(vertex.normal[2], 0.76604444f));
    ge_original_frontend_rareware_body_uv(&vertex, uv);
    assert(close_float(uv[0], vertex.generated_uv[0]
        * (float)0x1c81 / 2048.0f - 11.5f / 32.0f));
    assert(close_float(uv[1], vertex.generated_uv[1]
        * (float)0x1426 / 2048.0f - 29.0f / 32.0f));

    {
        static const uint8_t nintendo_ambient[3] = {200, 200, 200};
        static const uint8_t nintendo_diffuse[3] = {0, 0, 0};
        static const int8_t nintendo_direction[3] = {0, 0, 0};
        normal[0] = 0U;
        normal[1] = 0U;
        normal[2] = 127U;
        assert(ge_original_frontend_generate_lit_vertex(
            normal, 0xffU, 0.5f * 3.14159265358979323846f,
            nintendo_ambient, nintendo_diffuse, nintendo_direction,
            &vertex));
        /* ninlogolight has dynamic ambient and a black directional light;
         * the rotating model normal still drives its reflected I8 texture. */
        assert(vertex.lit_rgba[0] == 200U
            && vertex.lit_rgba[1] == 200U
            && vertex.lit_rgba[2] == 200U
            && vertex.lit_rgba[3] == 0xffU);
        assert(close_float(vertex.generated_uv[0], 1.0f));
        assert(close_float(vertex.generated_uv[1], 0.5f));
    }

    assert(GE_ORIGINAL_RAREWARE_FRONT_TEXTURE_OFFSET == 0x4fe8);
    assert(GE_ORIGINAL_RAREWARE_BODY_TEXTURE_OFFSET == 0x5ff0);
    assert(GE_ORIGINAL_RAREWARE_REFLECTION_TEXTURE_BYTES == 2048);
    authored[0] = 0.0f;
    authored[1] = 0.0f;
    authored[2] = 0.0f;
    ge_original_frontend_rareware_project(authored, -40.0f, 880.0f,
                                           projected);
    assert(close_float(projected[0], 200.0f));
    assert(close_float(projected[1], 120.0f));
    assert(close_float(projected[2], 880.0f / 5000.0f));

    {
        const GeOriginalRarewarePassDescriptor *front =
            ge_original_rareware_pass_descriptor(
                GE_ORIGINAL_RAREWARE_PASS_FRONT);
        const GeOriginalRarewarePassDescriptor *letters =
            ge_original_rareware_pass_descriptor(
                GE_ORIGINAL_RAREWARE_PASS_LETTERS);
        const GeOriginalRarewarePassDescriptor *body =
            ge_original_rareware_pass_descriptor(
                GE_ORIGINAL_RAREWARE_PASS_BODY);
        assert(front != NULL && front->display_list_offset == 0x43e8U
            && front->texture_offset == 0x4fe8U
            && front->command_count == 25U
            && front->triangle_count == 18U
            && front->primitive == GE_ORIGINAL_RAREWARE_PRIMITIVE_PRIMARY
            && front->texture_generated == 1U);
        assert(letters != NULL && letters->display_list_offset == 0x44b0U
            && letters->texture_offset == 0x0020U
            && letters->command_count == 85U
            && letters->triangle_count == 8U
            && letters->primitive == GE_ORIGINAL_RAREWARE_PRIMITIVE_PRIMARY
            && letters->texture_generated == 0U);
        assert(body != NULL && body->display_list_offset == 0x4758U
            && body->texture_offset == 0x5ff0U
            && body->command_count == 273U
            && body->triangle_count == 242U
            && body->primitive == GE_ORIGINAL_RAREWARE_PRIMITIVE_SECONDARY
            && body->texture_generated == 1U);
    }
    if (argc > 1) {
        FILE *stream = fopen(argv[1], "rb");
        uint8_t *segment = malloc(GE_ORIGINAL_RAREWARE_SEGMENT_BYTES);
        GeOriginalRarewarePass pass;
        assert(stream != NULL && segment != NULL);
        assert(fread(segment, 1U, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES, stream)
            == GE_ORIGINAL_RAREWARE_SEGMENT_BYTES);
        assert(fgetc(stream) == EOF && fclose(stream) == 0);
        for (pass = GE_ORIGINAL_RAREWARE_PASS_FRONT;
                pass < GE_ORIGINAL_RAREWARE_PASS_COUNT;
                pass = (GeOriginalRarewarePass)((unsigned int)pass + 1U)) {
            GeOriginalRarewareMesh query;
            GeOriginalRarewareMesh built;
            GeOriginalRarewareVertex *vertices;
            assert(ge_original_rareware_mesh_build(
                segment, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES, pass,
                NULL, 0U, &query)
                == GE_ORIGINAL_RAREWARE_CAPACITY_EXCEEDED);
            vertices = calloc(query.required_vertex_count, sizeof(*vertices));
            assert(vertices != NULL);
            assert(ge_original_rareware_mesh_build(
                segment, GE_ORIGINAL_RAREWARE_SEGMENT_BYTES, pass,
                vertices, query.required_vertex_count, &built)
                == GE_ORIGINAL_RAREWARE_OK);
            assert(built.vertex_count == query.required_vertex_count);
            free(vertices);
        }
        free(segment);
    }
    puts("Original frontend fixed-function visual semantics verified");
    return 0;
}
