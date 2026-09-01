#include "ge_original_animation_root.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    unsigned char *bytes;
    long length;
    assert(file != NULL);
    assert(fseek(file, 0, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0);
    assert(fseek(file, 0, SEEK_SET) == 0);
    bytes = malloc((size_t)length);
    assert(bytes != NULL);
    assert(fread(bytes, 1, (size_t)length, file) == (size_t)length);
    fclose(file);
    *size = (size_t)length;
    return bytes;
}

int main(int argc, char **argv)
{
    unsigned char *data;
    size_t size;
    GeOriginalAnimationRoot *walk;
    GeOriginalAnimationRoot *sprint;
    GeOriginalAnimationRoot *idle;
    GeOriginalAnimationRootSample sample = {0};
    float pos[3];
    float angle;
    float velocity[3];

    assert(argc == 2);
    data = read_file(argv[1], &size);
    walk = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_EYE_WALK);
    sprint = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_SPRINTING);
    idle = ge_original_animation_root_create(
        data, size, GE_ORIGINAL_BOND_ANIMATION_IDLE);
    assert(walk != NULL && sprint != NULL && idle != NULL);
    assert(ge_original_animation_root_frame_count(walk) == 35);
    assert(ge_original_animation_root_frame_count(sprint) == 19);
    assert(ge_original_animation_root_frame_count(idle) == 163);

    assert(ge_original_animation_root_create(
        data, size - 1, GE_ORIGINAL_BOND_ANIMATION_SPRINTING) == NULL);
    assert(ge_original_animation_root_decode(walk, 9, 0, pos, &angle));
    assert(pos[0] == -3.0f && pos[1] == 1033.0f && pos[2] == 59.0f);
    assert(angle == 0.0f);
    assert(ge_original_animation_root_decode(walk, 27, 1, pos, &angle));
    assert(pos[0] == -3.0f && pos[1] == 1029.0f && pos[2] == 60.0f);
    assert(angle == 0.0f);
    assert(ge_original_animation_root_decode(sprint, 7, 0, pos, &angle));
    assert(pos[0] == -6.0f && pos[1] == 1011.0f && pos[2] == 157.0f);
    assert(ge_original_animation_root_decode(idle, 0, 0, pos, &angle));
    assert(pos[0] == 0.0f && pos[1] == 1083.0f && pos[2] == 0.0f);
    assert(angle == 0.0f);
    assert(!ge_original_animation_root_decode(sprint, 19, 0, pos, &angle));
    ge_original_animation_root_sample_set(&sample, sprint, 7, 0);
    assert(ge_original_animation_root_sample_velocity(
        &sample, 1.0f, 0.0f, 1, 1.0f, velocity));
    assert(fabsf(velocity[0] - -0.6f) < 0.0001f);
    assert(fabsf(velocity[1] - 101.1f) < 0.0001f);
    assert(fabsf(velocity[2] - 15.7f) < 0.0001f);
    assert(!ge_original_animation_root_sample_velocity(
        &sample, 1.0f, 0.0f, 1, 1.0f, velocity));

    ge_original_animation_root_destroy(walk);
    ge_original_animation_root_destroy(sprint);
    ge_original_animation_root_destroy(idle);
    free(data);
    return 0;
}
