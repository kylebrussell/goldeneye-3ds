#include "game/matrixmath.h"

#include <PR/gu.h>

#include <string.h>

/* Keep original GoldenEye types on this side of the test boundary. The
 * adapter itself intentionally accepts plain copied float matrices. */
void ge_test_original_camera_matrices(float view[4][4],
                                      float projection[4][4],
                                      u16 *perspective_normalize)
{
    Mtxf original_view;

    matrix_4x4_set_lookat(&original_view,
        10.0f, 20.0f, 30.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f);
    memcpy(view, original_view.m, sizeof(original_view.m));
    guPerspectiveF(projection, perspective_normalize,
        60.0f, 1.6f, 1.0f, 100.0f, 1.0f);
}
