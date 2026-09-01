#include "ge_gbi_matrix.h"

#include "ge_gbi_matrix_fixture.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int nearly_equal(float left, float right)
{
    return fabsf(left - right) < 0.00001f;
}

static void test_fixed_matrix_decode(void)
{
    GeGbiMatrix matrix;

    assert(ge_gbi_matrix_decode_fixed(ge_test_matrix_fixed_be,
                                      sizeof(ge_test_matrix_fixed_be),
                                      GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                      &matrix) == GE_GBI_MATRIX_OK);
    assert(nearly_equal(matrix.elements[0][0], 1.5f));
    assert(nearly_equal(matrix.elements[0][1], -2.25f));
    assert(nearly_equal(matrix.elements[1][1], 1.0f));
    assert(nearly_equal(matrix.elements[2][2], 1.0f));
    assert(nearly_equal(matrix.elements[3][0], 3.125f));
    assert(nearly_equal(matrix.elements[3][1], 4.5f));
    assert(nearly_equal(matrix.elements[3][2], -0.5f));
    assert(nearly_equal(matrix.elements[3][3], 1.0f));
    assert(ge_gbi_matrix_decode_fixed(ge_test_matrix_fixed_be, 63U,
                                      GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                      &matrix) == GE_GBI_MATRIX_TRUNCATED);
    assert(ge_gbi_matrix_decode_fixed(NULL, 64U,
                                      GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                      &matrix) == GE_GBI_MATRIX_INVALID_ARGUMENT);
}

static void test_identity_and_alias_safe_multiply(void)
{
    GeGbiMatrix identity;
    GeGbiMatrix matrix;
    GeGbiMatrix product;

    ge_gbi_matrix_identity(&identity);
    assert(ge_gbi_matrix_decode_fixed(ge_test_matrix_fixed_be,
                                      sizeof(ge_test_matrix_fixed_be),
                                      GE_GBI_BYTE_ORDER_BIG_ENDIAN,
                                      &matrix) == GE_GBI_MATRIX_OK);
    ge_gbi_matrix_multiply(&product, &matrix, &identity);
    assert(memcmp(&product, &matrix, sizeof(product)) == 0);
    ge_gbi_matrix_multiply(&matrix, &identity, &matrix);
    assert(memcmp(&product, &matrix, sizeof(product)) == 0);
}

static void test_matrix_stack_initialization(void)
{
    GeGbiMatrixStack stack;
    const GeGbiMatrix *top;

    ge_gbi_matrix_stack_init(&stack);
    top = ge_gbi_matrix_stack_top(&stack);
    assert(stack.count == 1U);
    assert(top != NULL);
    assert(nearly_equal(top->elements[0][0], 1.0f));
    assert(nearly_equal(top->elements[3][3], 1.0f));
    assert(ge_gbi_matrix_stack_top(NULL) == NULL);
}

int main(void)
{
    test_fixed_matrix_decode();
    test_identity_and_alias_safe_multiply();
    test_matrix_stack_initialization();
    puts("GoldenEye N64 fixed-point matrix tests passed");
    return 0;
}
