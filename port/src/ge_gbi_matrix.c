#include "ge_gbi_matrix.h"

#include <string.h>

static uint16_t ge_gbi_matrix_read_u16(const uint8_t *bytes,
                                       GeGbiByteOrder byte_order)
{
    if (byte_order == GE_GBI_BYTE_ORDER_BIG_ENDIAN) {
        return (uint16_t)(((uint16_t)bytes[0] << 8) | (uint16_t)bytes[1]);
    }
    return (uint16_t)(((uint16_t)bytes[1] << 8) | (uint16_t)bytes[0]);
}

GeGbiMatrixStatus ge_gbi_matrix_decode_fixed(const uint8_t *bytes,
                                              size_t byte_count,
                                              GeGbiByteOrder byte_order,
                                              GeGbiMatrix *matrix)
{
    size_t index;

    if (bytes == NULL || matrix == NULL
            || (byte_order != GE_GBI_BYTE_ORDER_BIG_ENDIAN
                && byte_order != GE_GBI_BYTE_ORDER_LITTLE_ENDIAN)) {
        return GE_GBI_MATRIX_INVALID_ARGUMENT;
    }
    if (byte_count < GE_GBI_MATRIX_BYTE_COUNT) {
        return GE_GBI_MATRIX_TRUNCATED;
    }

    for (index = 0U; index < 16U; ++index) {
        const int16_t integer_part = (int16_t)ge_gbi_matrix_read_u16(
            bytes + index * 2U, byte_order);
        const uint16_t fraction_part = ge_gbi_matrix_read_u16(
            bytes + 32U + index * 2U, byte_order);
        const int64_t fixed = (int64_t)integer_part * INT64_C(65536)
            + (int64_t)fraction_part;

        matrix->elements[index / 4U][index % 4U]
            = (float)fixed / 65536.0f;
    }
    return GE_GBI_MATRIX_OK;
}

void ge_gbi_matrix_identity(GeGbiMatrix *matrix)
{
    size_t row;
    size_t column;

    if (matrix == NULL) {
        return;
    }
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            matrix->elements[row][column] = row == column ? 1.0f : 0.0f;
        }
    }
}

void ge_gbi_matrix_multiply(GeGbiMatrix *result,
                            const GeGbiMatrix *left,
                            const GeGbiMatrix *right)
{
    GeGbiMatrix product;
    size_t row;
    size_t column;

    if (result == NULL || left == NULL || right == NULL) {
        return;
    }
    memset(&product, 0, sizeof(product));
    for (row = 0U; row < 4U; ++row) {
        for (column = 0U; column < 4U; ++column) {
            size_t term;

            for (term = 0U; term < 4U; ++term) {
                product.elements[row][column] += left->elements[row][term]
                    * right->elements[term][column];
            }
        }
    }
    *result = product;
}

void ge_gbi_matrix_stack_init(GeGbiMatrixStack *stack)
{
    if (stack != NULL) {
        memset(stack, 0, sizeof(*stack));
        stack->count = 1U;
        ge_gbi_matrix_identity(&stack->entries[0]);
    }
}

const GeGbiMatrix *ge_gbi_matrix_stack_top(const GeGbiMatrixStack *stack)
{
    if (stack == NULL || stack->count == 0U
            || stack->count > GE_GBI_MATRIX_STACK_CAPACITY) {
        return NULL;
    }
    return &stack->entries[stack->count - 1U];
}
