#include "ge_original_frontend_visuals.h"
#include "ge_original_rareware_logo.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "reference_frontend_visuals.h"

static void check_prepared(const uint8_t normal[3], uint8_t alpha,
    float radians, float degrees, float camera_z, const uint8_t ambient[3],
    const uint8_t diffuse[3], const int8_t direction[3], const float position[3])
{
    GeOriginalFrontendLightingContext lighting;
    GeOriginalFrontendProjectionContext projection;
    GeOriginalFrontendGeneratedVertex old, single, prepared;
    float old_projection[3], new_projection[3], single_projection[3];
    assert(ge_original_frontend_lighting_prepare(radians,ambient,diffuse,direction,&lighting));
    assert(reference_frontend_generate_lit_vertex(normal,alpha,radians,ambient,diffuse,direction,&old));
    assert(ge_original_frontend_generate_lit_vertex(normal,alpha,radians,ambient,diffuse,direction,&single));
    assert(ge_original_frontend_generate_lit_vertex_prepared(normal,alpha,&lighting,&prepared));
    assert(memcmp(&old,&prepared,sizeof(old))==0 && memcmp(&old,&single,sizeof(old))==0);
    reference_frontend_rareware_project(position,degrees,camera_z,old_projection);
    ge_original_frontend_rareware_projection_prepare(degrees,camera_z,&projection);
    ge_original_frontend_rareware_project_prepared(position,&projection,new_projection);
    ge_original_frontend_rareware_project(position,degrees,camera_z,single_projection);
    assert(memcmp(old_projection,new_projection,sizeof(old_projection))==0);
    assert(memcmp(old_projection,single_projection,sizeof(old_projection))==0);
}

static uint32_t random_word(uint32_t *seed)
{ *seed=*seed*UINT32_C(1664525)+UINT32_C(1013904223);return *seed; }

static void exercise_prepared(void)
{
    uint32_t seed=4689;
    for(size_t sample=0;sample<40000;++sample) {
        uint8_t normal[3],ambient[3],diffuse[3];int8_t direction[3];float position[3];
        for(size_t c=0;c<3;++c) {
            normal[c]=sample%16==0?0U:(uint8_t)(random_word(&seed)>>16);
            ambient[c]=(uint8_t)(random_word(&seed)>>16);
            diffuse[c]=(uint8_t)(random_word(&seed)>>16);
            direction[c]=sample%8==0?0:(int8_t)(random_word(&seed)>>16);
            position[c]=(float)(int16_t)(random_word(&seed)>>16);
        }
        check_prepared(normal,(uint8_t)sample,(float)sample/1000.0f,
            (float)sample/30.0f,(float)(sample%5000),ambient,diffuse,direction,position);
    }
    {
        uint8_t normal[3]={127,0,0},ambient[3]={20,40,60},diffuse[3]={200,150,100};
        int8_t direction[3]={77,77,46};
        GeOriginalFrontendLightingContext lighting;
        GeOriginalFrontendGeneratedVertex a,b;
        assert(ge_original_frontend_lighting_prepare(0.4f,ambient,diffuse,direction,&lighting));
        assert(reference_frontend_generate_lit_vertex(normal,81,0.4f,ambient,diffuse,direction,&a));
        memset(ambient,0,3);memset(diffuse,0,3);memset(direction,0,3);
        assert(ge_original_frontend_generate_lit_vertex_prepared(normal,81,&lighting,&b));
        assert(memcmp(&a,&b,sizeof(a))==0);
        assert(!ge_original_frontend_lighting_prepare(0,NULL,diffuse,direction,&lighting));
        assert(!ge_original_frontend_generate_lit_vertex_prepared(normal,81,&lighting,&b));
        assert(memcmp(&a,&b,sizeof(a))==0);
        assert(!ge_original_frontend_lighting_prepare(0,ambient,NULL,direction,&lighting));
        assert(!ge_original_frontend_lighting_prepare(0,ambient,diffuse,NULL,&lighting));
        assert(!ge_original_frontend_lighting_prepare(0,ambient,diffuse,direction,NULL));
        assert(!ge_original_frontend_generate_lit_vertex_prepared(NULL,81,&lighting,&b));
        assert(!ge_original_frontend_generate_lit_vertex_prepared(normal,81,NULL,&b));
        assert(!ge_original_frontend_generate_lit_vertex_prepared(normal,81,&lighting,NULL));
        GeOriginalFrontendProjectionContext projection={0};
        float p[3]={1,2,3},unchanged[3]={1,2,3};
        ge_original_frontend_rareware_project_prepared(p,&projection,p);
        assert(memcmp(p,unchanged,sizeof(p))==0);
        ge_original_frontend_rareware_project_prepared(p,NULL,p);
        assert(memcmp(p,unchanged,sizeof(p))==0);
    }
    puts("prepared frontend: 40000 byte-exact scalar-oracle cases, zero normals/lights, changed snapshot inputs and invalidation passed");
}

#ifdef GE_FRONTEND_PREPARE_BENCH
static void benchmark_prepared(void)
{
    const uint8_t ambient[3]={150,150,150},diffuse[3]={255,255,255};
    const int8_t direction[3]={77,77,46};
    uint8_t normals[780][3];float positions[780][3];
    GeOriginalFrontendGeneratedVertex output[780];float projected[780][3];
    uint32_t seed=964;
    for(size_t v=0;v<780;++v)for(size_t c=0;c<3;++c) {
        normals[v][c]=(uint8_t)(random_word(&seed)>>16);
        positions[v][c]=(float)(int16_t)(random_word(&seed)>>16);
    }
    double elapsed[2];volatile float sums[2]={0};
    for(unsigned mode=0;mode<2;++mode) {
        const clock_t start=clock();
        for(size_t frame=0;frame<2000;++frame) {
            const float radians=(float)frame/1000.0f,degrees=(float)frame/20.0f;
            GeOriginalFrontendLightingContext lighting;
            GeOriginalFrontendProjectionContext projection;
            if(mode) {
                assert(ge_original_frontend_lighting_prepare(radians,ambient,diffuse,direction,&lighting));
                ge_original_frontend_rareware_projection_prepare(degrees,880,&projection);
            }
            for(size_t v=0;v<780;++v) {
                if(mode) {
                    assert(ge_original_frontend_generate_lit_vertex_prepared(normals[v],255,&lighting,&output[v]));
                    ge_original_frontend_rareware_project_prepared(positions[v],&projection,projected[v]);
                } else {
                    assert(reference_frontend_generate_lit_vertex(normals[v],255,radians,ambient,diffuse,direction,&output[v]));
                    reference_frontend_rareware_project(positions[v],degrees,880,projected[v]);
                }
            }
            sums[mode]+=output[frame%780].generated_uv[0]+projected[frame%780][0];
        }
        elapsed[mode]=1000.0*(clock()-start)/CLOCKS_PER_SEC;
    }
    assert(sums[0]==sums[1]);
    printf("frontend 1560000 lit/projected vertices (includes frame preparation): scalar %.3f ms prepared %.3f ms\n",elapsed[0],elapsed[1]);
}
#endif

static int close_float(float left, float right)
{
    return fabsf(left - right) < 0.00001f;
}

int main(int argc, char **argv)
{
    exercise_prepared();
#ifdef GE_FRONTEND_PREPARE_BENCH
    benchmark_prepared();
#endif
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
            for(size_t frame=0;frame<64;++frame)for(size_t v=0;v<built.vertex_count;++v) {
                const GeGbiVertex *source=&vertices[v].source;
                const uint8_t packed[3]={source->red,source->green,source->blue};
                const float position[3]={(float)source->x,(float)source->y,(float)source->z};
                check_prepared(packed,source->alpha,(float)frame/30.0f,
                    (float)frame*5.0f,880.0f,ambient,diffuse,direction,position);
            }
            printf("authored Rareware pass %u: %zu exact scalar/prepared vertex comparisons\n",
                (unsigned)pass,64U*built.vertex_count);
            free(vertices);
        }
        free(segment);
    }
    puts("Original frontend fixed-function visual semantics verified");
    return 0;
}
