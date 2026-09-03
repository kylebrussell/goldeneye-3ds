#include "ge_3ds_scene_texture.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Atlas API seam covers both orientations. Production still gets all four
 * corners from Tex3DS, not from these test definitions. */
static int rotated;
void Tex3DS_SubTextureTopLeft(const Tex3DS_SubTexture *s, float *u, float *v)
{ *u = s->left; *v = rotated ? s->bottom : s->top; }
void Tex3DS_SubTextureTopRight(const Tex3DS_SubTexture *s, float *u, float *v)
{ *u = rotated ? s->left : s->right; *v = s->top; }
void Tex3DS_SubTextureBottomLeft(const Tex3DS_SubTexture *s, float *u, float *v)
{ *u = rotated ? s->right : s->left; *v = s->bottom; }
void Tex3DS_SubTextureBottomRight(const Tex3DS_SubTexture *s, float *u, float *v)
{ *u = s->right; *v = rotated ? s->top : s->bottom; }

static uint32_t random_word(uint32_t *seed)
{ *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223); return *seed; }

static GeTextureUv reference_normalize(int16_t s, int16_t t,
    const GePicaMaterial *material, uint32_t width, uint32_t height)
{
    /* Original pre-context arithmetic; comparison is byte-exact, not epsilon. */
    GeTextureUv result;
    result.u = (float)s * (1.0f / 32.0f)
        * (float)material->texture_scale_s * (1.0f / 65536.0f) / (float)width;
    result.v = (float)t * (1.0f / 32.0f)
        * (float)material->texture_scale_t * (1.0f / 65536.0f) / (float)height;
    return result;
}

static void compare(const Ge3dsSceneTextureSlot *slot,
    const GePicaMaterial *material, const Ge3dsSceneTextureUvContext *context,
    int16_t s, int16_t t)
{
    GeTextureUv old, prepared;
    const GeTextureUv reference = reference_normalize(s,t,material,slot->width,slot->height);
    assert(ge_texture_uv_normalize(s,t,material,slot->width,slot->height,&old) == GE_TEXTURE_UV_OK);
    assert(memcmp(&reference,&old,sizeof(old)) == 0);
    assert(ge_texture_uv_normalize_prepared(s,t,&context->normalization,&prepared) == GE_TEXTURE_UV_OK);
    assert(memcmp(&reference,&prepared,sizeof(old)) == 0);
    assert(ge_3ds_scene_texture_map_uv(slot,s,t,material,&old) == GE_TEXTURE_UV_OK);
    assert(ge_3ds_scene_texture_map_uv_prepared(context,s,t,&prepared) == GE_TEXTURE_UV_OK);
    assert(memcmp(&old,&prepared,sizeof(old)) == 0);
}

static void test_exact_mapping(void)
{
    const uint32_t dimensions[] = {1U,3U,16U,31U,32U,64U,96U,255U,1024U,UINT32_MAX};
    uint32_t seed = 937U;
    for (size_t sample=0U;sample<64U;++sample) {
        GePicaMaterial material = {0};
        Ge3dsSceneTextureSlot slot = {.loaded=1U};
        Ge3dsSceneTextureUvContext context;
        slot.width = dimensions[sample % 10U];
        slot.height = dimensions[(sample + 3U) % 10U];
        material.texture_scale_s = sample==0U?0U:sample==1U?UINT16_MAX:(uint16_t)random_word(&seed);
        material.texture_scale_t = (uint16_t)random_word(&seed);
        material.texture_shift_s = (uint8_t)(sample % 16U);
        material.texture_shift_t = (uint8_t)((sample + 8U) % 16U);
        slot.subtexture = (Tex3DS_SubTexture){0.125f,0.8125f,0.625f,0.0625f};
        if (sample % 3U == 0U) {
            slot.subtexture.left = nextafterf(0.125f,INFINITY);
            slot.subtexture.bottom = nextafterf(0.0625f,-INFINITY);
        }
        rotated = (int)(sample % 2U);
        assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context) == GE_TEXTURE_UV_OK);
        /* Every representable ST value, paired with a different full-domain
         * permutation so both axes cover endpoints, negatives and wrap. */
        for (int32_t value=INT16_MIN;value<=INT16_MAX;++value) {
            const uint32_t paired = ((uint32_t)(value - INT16_MIN)
                * UINT32_C(40503) + (uint32_t)sample * UINT32_C(937)) & UINT32_C(65535);
            const int16_t t = (int16_t)((int32_t)paired + INT16_MIN);
            compare(&slot,&material,&context,(int16_t)value,t);
        }
    }
    puts("prepared UV: 4194304 byte-exact normal/atlas comparisons, rotated/padded atlases, signed ST, scales/shifts and non-power-of-two dimensions passed");
}

static void test_snapshot_and_failure(void)
{
    GePicaMaterial material = {.texture_scale_s=UINT16_MAX,.texture_scale_t=32768U};
    Ge3dsSceneTextureSlot slot = {.loaded=1U,.width=32U,.height=17U,
        .subtexture={0.125f,0.75f,0.875f,0.0625f}};
    Ge3dsSceneTextureUvContext context;
    GeTextureUv old, current;
    rotated=1;
    assert(ge_3ds_scene_texture_map_uv(&slot,-301,456,&material,&old)==GE_TEXTURE_UV_OK);
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_OK);
    memset(&slot,0,sizeof(slot)); memset(&material,0,sizeof(material)); rotated=0;
    assert(ge_3ds_scene_texture_map_uv_prepared(&context,-301,456,&current)==GE_TEXTURE_UV_OK);
    assert(memcmp(&old,&current,sizeof(old))==0);
    assert(ge_3ds_scene_texture_uv_prepare(NULL,&material,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_3ds_scene_texture_map_uv_prepared(&context,0,0,&current)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(memcmp(&old,&current,sizeof(old))==0);
    slot.loaded=1; slot.width=32; slot.height=16;
    material.texture_shift_s=16;
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    material.texture_shift_s=0; material.texture_shift_t=16;
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    material.texture_shift_t=0; slot.width=0;
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    slot.width=32; slot.height=0;
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_3ds_scene_texture_uv_prepare(&slot,NULL,&context)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,NULL)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_3ds_scene_texture_map_uv_prepared(NULL,0,0,&current)==GE_TEXTURE_UV_INVALID_ARGUMENT);
    assert(ge_3ds_scene_texture_map_uv_prepared(&context,0,0,NULL)==GE_TEXTURE_UV_INVALID_ARGUMENT);
}

#ifdef GE_TEXTURE_UV_BENCH
static void benchmark(void)
{
    GePicaMaterial material = {.texture_scale_s=UINT16_MAX,.texture_scale_t=32768U};
    Ge3dsSceneTextureSlot slot = {.loaded=1,.width=64,.height=37,
        .subtexture={0.125f,0.75f,0.875f,0.0625f}};
    GeTextureUv output[192];
    volatile float sums[2]={0};
    for(size_t batch_size=3;batch_size<=192;batch_size*=4) {
        double elapsed[2];
        for(unsigned mode=0;mode<2;++mode) {
            clock_t start=clock();
            for(size_t repeat=0;repeat<50000;++repeat) {
                Ge3dsSceneTextureUvContext context;
                material.texture_scale_s=(uint16_t)(repeat+1U);
                rotated=(int)(repeat%2U);
                if(mode) assert(ge_3ds_scene_texture_uv_prepare(&slot,&material,&context)==GE_TEXTURE_UV_OK);
                for(size_t i=0;i<batch_size;++i) {
                    if(mode) assert(ge_3ds_scene_texture_map_uv_prepared(&context,(int16_t)i,(int16_t)repeat,&output[i])==GE_TEXTURE_UV_OK);
                    else assert(ge_3ds_scene_texture_map_uv(&slot,(int16_t)i,(int16_t)repeat,&material,&output[i])==GE_TEXTURE_UV_OK);
                }
                sums[mode]+=output[repeat%batch_size].u;
            }
            elapsed[mode]=1000.0*(clock()-start)/CLOCKS_PER_SEC;
        }
        assert(sums[0]==sums[1]);
        printf("UV %zu vertices/batch: old %.3f ms prepared %.3f ms (includes preparation)\n",batch_size,elapsed[0],elapsed[1]);
    }
}
#endif
int main(void)
{
    test_exact_mapping(); test_snapshot_and_failure();
#ifdef GE_TEXTURE_UV_BENCH
    benchmark();
#endif
    return 0;
}
