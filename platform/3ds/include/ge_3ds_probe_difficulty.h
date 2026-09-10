#ifndef GE_3DS_PROBE_DIFFICULTY_H
#define GE_3DS_PROBE_DIFFICULTY_H

typedef enum Ge3dsProbeDifficultyResult {
    GE_3DS_PROBE_DIFFICULTY_MISSING = 0,
    GE_3DS_PROBE_DIFFICULTY_OK,
    GE_3DS_PROBE_DIFFICULTY_INVALID
} Ge3dsProbeDifficultyResult;

/* Read one digit 0..3 surrounded only by optional ASCII whitespace. Signs,
 * extra digits, comments and other trailing bytes are invalid. ENOENT means
 * MISSING; all other I/O failures and null arguments mean INVALID. Assigns
 * *difficulty only on OK. This parser does not apply frontend/game state. */
Ge3dsProbeDifficultyResult ge_3ds_probe_difficulty_load(
    const char *path, int *difficulty);

#endif
