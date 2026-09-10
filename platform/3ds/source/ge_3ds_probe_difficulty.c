#include "ge_3ds_probe_difficulty.h"

#include <errno.h>
#include <stdio.h>

static int probe_whitespace(int byte)
{
    return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r'
        || byte == '\v' || byte == '\f';
}

Ge3dsProbeDifficultyResult ge_3ds_probe_difficulty_load(
    const char *path, int *difficulty)
{
    FILE *file;
    int byte;
    int parsed = -1;
    int valid = 1;
    if (path == NULL || difficulty == NULL)
        return GE_3DS_PROBE_DIFFICULTY_INVALID;
    file = fopen(path, "rb");
    if (file == NULL)
        return errno == ENOENT ? GE_3DS_PROBE_DIFFICULTY_MISSING
                              : GE_3DS_PROBE_DIFFICULTY_INVALID;
    while ((byte = fgetc(file)) != EOF) {
        if (probe_whitespace(byte)) continue;
        if (parsed < 0 && byte >= '0' && byte <= '3') {
            parsed = byte - '0';
        } else {
            valid = 0;
            break;
        }
    }
    if (ferror(file)) valid = 0;
    if (fclose(file) != 0) valid = 0;
    if (!valid || parsed < 0) return GE_3DS_PROBE_DIFFICULTY_INVALID;
    *difficulty = parsed;
    return GE_3DS_PROBE_DIFFICULTY_OK;
}
