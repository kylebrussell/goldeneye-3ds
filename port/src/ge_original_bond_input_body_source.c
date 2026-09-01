/* Compile the canonical decompiled input body as its own dead-strippable
 * translation unit.  The slice guards in bondview2.c exclude the unrelated
 * camera/stage code already linked by the 3DS port. */
typedef struct OSViMode OSViMode;
typedef int PLAYERFLAG;
#include "../../src/game/bondview2.c"
