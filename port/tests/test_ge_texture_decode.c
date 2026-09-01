#include "pdtex.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

static void test_tardetail(const char *path)
{
    struct pd_tex *texture;
    const struct pd_image *image;
    uint8_t minimum = UINT8_MAX;
    uint8_t maximum = 0U;
    size_t index;

    texture = pdtex_allocate();
    assert(texture != NULL);
    assert(pdtex_read(texture, (char *)path) == 0);
    image = &texture->images[0];
    assert(image->exists);
    assert(image->format == PDFORMAT_I8);
    assert(image->width == 64);
    assert(image->height == 32);
    assert(image->pixels != NULL);
    for (index = 0U; index < (size_t)image->width * (size_t)image->height;
            ++index) {
        if (image->pixels[index] < minimum) minimum = image->pixels[index];
        if (image->pixels[index] > maximum) maximum = image->pixels[index];
    }
    assert(minimum == UINT8_C(10));
    assert(maximum == UINT8_C(170));
    pdtex_free(texture);
}

static void test_texture_291(const char *path)
{
    struct pd_tex *texture = pdtex_allocate();
    const struct pd_image *image;
    uint8_t seen[16] = {0U};
    uint8_t minimum = UINT8_MAX;
    uint8_t maximum = 0U;
    size_t unique = 0U;
    size_t index;

    assert(texture != NULL);
    assert(pdtex_read(texture, (char *)path) == 0);
    image = &texture->images[0];
    assert(image->exists);
    assert(image->format == PDFORMAT_I4);
    assert(image->width == 64);
    assert(image->height == 64);
    assert(image->pixels != NULL);
    for (index = 0U; index < (size_t)image->width * (size_t)image->height;
            ++index) {
        const uint8_t packed = image->pixels[index >> 1];
        const uint8_t intensity = index % 2U == 0U ? packed >> 4
                                                   : packed & UINT8_C(0x0f);

        if (intensity < minimum) minimum = intensity;
        if (intensity > maximum) maximum = intensity;
        if (seen[intensity] == 0U) {
            seen[intensity] = UINT8_C(1);
            unique++;
        }
    }
    printf("Texture 291 I4 range %u..%u, %zu unique levels\n",
           (unsigned)minimum, (unsigned)maximum, unique);
    assert(minimum == UINT8_C(2));
    assert(maximum == UINT8_C(9));
    assert(unique == 8U);
    pdtex_free(texture);
}

int main(int argc, char **argv)
{
    assert(argc == 3);
    test_tardetail(argv[1]);
    test_texture_291(argv[2]);
    puts("GoldenEye lookup-compressed I8/I4 texture decode passed");
    return 0;
}
