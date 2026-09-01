#ifndef GE_GBI_MATRIX_H
#define GE_GBI_MATRIX_H

#include "ge_gbi_decoder.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GE_GBI_MATRIX_BYTE_COUNT 64U
#define GE_GBI_MATRIX_STACK_CAPACITY 32U

/* Classic Fast3D matrix parameter bits used by GoldenEye. */
#define GE_GBI_MTX_MODELVIEW  UINT8_C(0x00)
#define GE_GBI_MTX_PROJECTION UINT8_C(0x01)
#define GE_GBI_MTX_MULTIPLY   UINT8_C(0x00)
#define GE_GBI_MTX_LOAD       UINT8_C(0x02)
#define GE_GBI_MTX_NO_PUSH    UINT8_C(0x00)
#define GE_GBI_MTX_PUSH       UINT8_C(0x04)
#define GE_GBI_MTX_VALID_MASK UINT8_C(0x07)

typedef struct GeGbiMatrix {
    float elements[4][4];
} GeGbiMatrix;

typedef enum GeGbiMatrixStatus {
    GE_GBI_MATRIX_OK,
    GE_GBI_MATRIX_INVALID_ARGUMENT,
    GE_GBI_MATRIX_TRUNCATED
} GeGbiMatrixStatus;

typedef struct GeGbiMatrixStack {
    GeGbiMatrix entries[GE_GBI_MATRIX_STACK_CAPACITY];
    uint8_t count;
} GeGbiMatrixStack;

GeGbiMatrixStatus ge_gbi_matrix_decode_fixed(const uint8_t *bytes,
                                              size_t byte_count,
                                              GeGbiByteOrder byte_order,
                                              GeGbiMatrix *matrix);

void ge_gbi_matrix_identity(GeGbiMatrix *matrix);

/* Alias-safe row-major multiplication: result = left * right. */
void ge_gbi_matrix_multiply(GeGbiMatrix *result,
                            const GeGbiMatrix *left,
                            const GeGbiMatrix *right);

void ge_gbi_matrix_stack_init(GeGbiMatrixStack *stack);

const GeGbiMatrix *ge_gbi_matrix_stack_top(const GeGbiMatrixStack *stack);

#ifdef __cplusplus
}
#endif

#endif
