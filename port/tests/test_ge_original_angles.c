#include <assert.h>
#include <stdio.h>

#include "math_atan2f.h"

static int near(float actual, float expected)
{
    float delta = actual - expected;

    if (delta < 0.0f) delta = -delta;
    return delta < 0.0002f;
}

int main(void)
{
    const float half_pi = 3.1415927f * 0.5f;

    assert(near(atan2f(0.0f, 1.0f), 0.0f));
    assert(near(atan2f(0.0f, -1.0f), 3.1415927f));
    assert(near(atan2f(1.0f, 0.0f), half_pi));
    /* GoldenEye returns the wrapped positive angle, unlike libc atan2f. */
    assert(near(atan2f(-1.0f, 0.0f), 3.0f * half_pi));
    assert(near(atan2f(1.0f, 1.0f), 0.25f * 3.1415927f));
    assert(near(atan2f(-1.0f, 1.0f), 1.75f * 3.1415927f));

    puts("original GoldenEye table-driven angle functions passed");
    return 0;
}
