#ifndef REFERENCE_FRONTEND_VISUALS_H
#define REFERENCE_FRONTEND_VISUALS_H
#include "ge_original_frontend_visuals.h"
int reference_frontend_generate_lit_vertex(
    const uint8_t packed_normal[3], uint8_t alpha, float rotation_y_radians,
    const uint8_t ambient_rgb[3], const uint8_t diffuse_rgb[3],
    const int8_t light_direction[3], GeOriginalFrontendGeneratedVertex *output);
void reference_frontend_rareware_project(const float authored[3],
    float rotation_y_degrees, float camera_eye_z, float projected[3]);
#endif
