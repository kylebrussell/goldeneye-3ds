#include "ge_original_stage_monitor.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

static void multiply_reference(
    const float lhs[4][4], const float rhs[4][4], float output[4][4])
{
    size_t axis;
    size_t row;
    for (axis = 0U; axis < 3U; ++axis)
        for (row = 0U; row < 4U; ++row) {
            output[row][axis] = lhs[0][axis] * rhs[row][0]
                + lhs[1][axis] * rhs[row][1]
                + lhs[2][axis] * rhs[row][2];
            if (row == 3U) output[row][axis] += lhs[3][axis];
        }
    output[0][3] = output[1][3] = output[2][3] = 0.0f;
    output[3][3] = 1.0f;
}

int main(void)
{
    const float owner[4][4] = {
        {0.0f, 1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 1.0f, 0.0f},
        {120.0f, -45.0f, 900.0f, 1.0f},
    };
    const float embedment[4][4] = {
        {0.5f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.466164f, 0.179184f, 0.0f},
        {0.0f, -0.179184f, 0.466164f, 0.0f},
        {0.0f, 0.0f, 0.0f, 1.0f},
    };
    float expected[4][4];
    float actual[4][4];
    size_t row;
    size_t column;

    multiply_reference(owner, embedment, expected);
    assert(ge_original_stage_monitor_compose_attachment_exact(
        owner, embedment, actual));
    for (row = 0U; row < 4U; ++row)
        for (column = 0U; column < 4U; ++column)
            assert(fabsf(actual[row][column] - expected[row][column])
                < 0.00001f);
    assert(!ge_original_stage_monitor_compose_attachment_exact(
        NULL, embedment, actual));
    assert(!ge_original_stage_monitor_compose_attachment_exact(
        owner, NULL, actual));
    assert(!ge_original_stage_monitor_compose_attachment_exact(
        owner, embedment, NULL));
    puts("canonical stage monitor attachment composition passed");
    return 0;
}
