#include "ge_original_bond_camera.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BG_BASE UINT32_C(0x0f000000)
#define DAM_LEVEL_SCALE 0.23363999f
#define DAM_VISIBILITY_SCALE 0.2f
#define DAM_SPAWN_ROOM 135U

static uint32_t be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static float be_float(const uint8_t *data)
{
    const uint32_t bits = be32(data);
    float value;

    memcpy(&value, &bits, sizeof(value));
    return value;
}

int main(int argc, char **argv)
{
    static const float authored_position[3] = {4719.0f, -18.0f, 3949.0f};
    static const float look[3] = {-1.0f, 0.0f, -0.000643f};
    static const float up[3] = {0.0f, 1.0f, 0.0f};
    const char *background_path =
        "build/u/assets/obseg/bg/bg_dam_all_p.bin";
    FILE *background;
    FILE *output;
    uint8_t header[20];
    uint8_t room_record[24];
    uint32_t room_table;
    GeOriginalBondCameraConfig config = {0};
    GeOriginalBondCameraResult result;
    size_t axis;

    assert(argc == 2);
    background = fopen(background_path, "rb");
    assert(background != NULL);
    assert(fread(header, 1U, sizeof(header), background) == sizeof(header));
    room_table = be32(header + 4U) - BG_BASE;
    assert(fseek(background,
        (long)(room_table + DAM_SPAWN_ROOM * 24U), SEEK_SET) == 0);
    assert(fread(room_record, 1U, sizeof(room_record), background)
        == sizeof(room_record));
    assert(fclose(background) == 0);
    for (axis = 0U; axis < 3U; ++axis) {
        config.camera_position[axis] = authored_position[axis]
            / DAM_LEVEL_SCALE;
        config.camera_look_direction[axis] = look[axis];
        config.camera_up[axis] = up[axis];
        config.room_origin[axis] = be_float(room_record + 12U + axis * 4U)
            / DAM_LEVEL_SCALE;
    }
    config.room_position_scale = DAM_LEVEL_SCALE;
    config.camera_local_scale = DAM_VISIBILITY_SCALE;
    config.visibility_scale = DAM_VISIBILITY_SCALE;
    config.viewport_scale[0] = 640;
    config.viewport_scale[1] = 480;
    config.viewport_scale[2] = 511;
    config.viewport_translation[0] = 800;
    config.viewport_translation[1] = 480;
    config.viewport_translation[2] = 511;
    config.room = DAM_SPAWN_ROOM;
    assert(ge_original_bond_camera_set_perspective(
        &config, 60.0f, 4.0f / 3.0f, 100.0f, 10000.0f)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    assert(ge_original_bond_camera_run(&config, &result)
        == GE_ORIGINAL_BOND_CAMERA_OK);
    output = fopen(argv[1], "wb");
    assert(output != NULL);
    assert(fwrite(result.view, 1U, sizeof(result.view), output)
        == sizeof(result.view));
    assert(fclose(output) == 0);
    puts("original Dam spawn camera matrix captured");
    return 0;
}
