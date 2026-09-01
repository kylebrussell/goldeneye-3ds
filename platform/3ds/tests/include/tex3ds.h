#ifndef TEST_TEX3DS_H
#define TEST_TEX3DS_H

#include <stdbool.h>
#include <stddef.h>

#include <citro3d.h>

typedef struct Tex3DS_SubTexture {
    float left;
    float top;
    float right;
    float bottom;
} Tex3DS_SubTexture;

typedef struct TestTex3DS_Texture *Tex3DS_Texture;

Tex3DS_Texture Tex3DS_TextureImport(
    const void *data, size_t data_size, C3D_Tex *texture,
    void *cube_map, bool vram);
size_t Tex3DS_GetNumSubTextures(Tex3DS_Texture texture);
const Tex3DS_SubTexture *Tex3DS_GetSubTexture(
    Tex3DS_Texture texture, size_t index);
void Tex3DS_TextureFree(Tex3DS_Texture texture);
void Tex3DS_SubTextureTopLeft(
    const Tex3DS_SubTexture *subtexture, float *u, float *v);
void Tex3DS_SubTextureTopRight(
    const Tex3DS_SubTexture *subtexture, float *u, float *v);
void Tex3DS_SubTextureBottomLeft(
    const Tex3DS_SubTexture *subtexture, float *u, float *v);
void Tex3DS_SubTextureBottomRight(
    const Tex3DS_SubTexture *subtexture, float *u, float *v);

#endif
