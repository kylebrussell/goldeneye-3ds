#ifndef TEST_CITRO3D_H
#define TEST_CITRO3D_H

#include <stdint.h>

typedef struct C3D_Tex {
    uint32_t test_handle;
} C3D_Tex;

void C3D_TexDelete(C3D_Tex *texture);

#endif
