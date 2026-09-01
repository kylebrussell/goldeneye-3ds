#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

/* The generated PitemZ table needs its two common skeletons and the chrbug
 * owner sentinel.  The real greatguard2 owner is linked separately because
 * it is an authored MENU_DISPLAY_CAST body, not an inert table dependency. */
#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \
    ModelSkeleton SKELETON(NAME) = {NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0};
#include "assets/embedded/skeletons/standard_object.inc.c"
#include "assets/embedded/skeletons/prop_weapon.inc.c"

ModelFileHeader chrbug_header;
