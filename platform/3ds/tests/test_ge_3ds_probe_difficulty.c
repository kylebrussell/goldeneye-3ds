#include "ge_3ds_probe_difficulty.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check_bytes(const char *path, const void *data, size_t size,
    Ge3dsProbeDifficultyResult expected, int value)
{
    FILE *file = fopen(path, "wb");
    int difficulty = 77;
    assert(file != NULL);
    assert(fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
    assert(ge_3ds_probe_difficulty_load(path, &difficulty) == expected);
    assert(difficulty == (expected == GE_3DS_PROBE_DIFFICULTY_OK ? value : 77));
}

int main(int argc, char **argv)
{
    static const char *invalid[] = {
        "", " \t\r\n\v\f", "4", "9", "-1", "+1", "-0", "+0",
        "00", "01", "10", "2147483648", "9999999999999999999999999999",
        "1 2", "1\n2", "2 word", "word 2", "2#comment", "2 #comment\n",
        "//comment\n2", "2 /*comment*/", "2.0", "0x2", "2e0", "true"
    };
    const unsigned char embedded_nul[] = {'2', 0, ' '};
    const unsigned char non_ascii[] = {'2', 0xff};
    const unsigned char bom[] = {0xef, 0xbb, 0xbf, '2'};
    const unsigned char nbsp[] = {0xc2, 0xa0, '2'};
    char buffer[4096];
    int difficulty = 77;
    assert(argc == 3);
    assert(ge_3ds_probe_difficulty_load(NULL, &difficulty)
        == GE_3DS_PROBE_DIFFICULTY_INVALID);
    assert(difficulty == 77);
    assert(ge_3ds_probe_difficulty_load(argv[1], NULL)
        == GE_3DS_PROBE_DIFFICULTY_INVALID);
    assert(ge_3ds_probe_difficulty_load(argv[1], &difficulty)
        == GE_3DS_PROBE_DIFFICULTY_MISSING);
    assert(difficulty == 77);
    /* A directory/read error must not be confused with an absent file. */
    assert(ge_3ds_probe_difficulty_load(argv[2], &difficulty)
        == GE_3DS_PROBE_DIFFICULTY_INVALID);
    assert(difficulty == 77);
    for (int value = 0; value <= 3; ++value) {
        char digit = (char)('0' + value);
        check_bytes(argv[1], &digit, 1, GE_3DS_PROBE_DIFFICULTY_OK, value);
        int length = snprintf(buffer, sizeof(buffer), " \t\r\n\v\f%c\f\v\n\r\t ", digit);
        assert(length > 0 && (size_t)length < sizeof(buffer));
        check_bytes(argv[1], buffer, (size_t)length,
            GE_3DS_PROBE_DIFFICULTY_OK, value);
    }
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); ++i)
        check_bytes(argv[1], invalid[i], strlen(invalid[i]),
            GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    check_bytes(argv[1], embedded_nul, sizeof(embedded_nul), GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    check_bytes(argv[1], non_ascii, sizeof(non_ascii), GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    check_bytes(argv[1], bom, sizeof(bom), GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    check_bytes(argv[1], nbsp, sizeof(nbsp), GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    memset(buffer, ' ', sizeof(buffer));
    buffer[sizeof(buffer) / 2] = '3';
    check_bytes(argv[1], buffer, sizeof(buffer), GE_3DS_PROBE_DIFFICULTY_OK, 3);
    buffer[sizeof(buffer) - 1] = '0';
    check_bytes(argv[1], buffer, sizeof(buffer), GE_3DS_PROBE_DIFFICULTY_INVALID, 0);
    assert(remove(argv[1]) == 0);
    assert(ge_3ds_probe_difficulty_load(argv[1], &difficulty)
        == GE_3DS_PROBE_DIFFICULTY_MISSING);
    assert(difficulty == 77);
    puts("Probe difficulty: strict grammar, missing/invalid distinction and unchanged output passed");
    return 0;
}
