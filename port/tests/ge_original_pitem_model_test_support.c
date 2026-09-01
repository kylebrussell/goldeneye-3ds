#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>

/* The production table links these exact common skeletons from pobjdata and
 * the relocated chrbug/greatguard model headers from their owning runtimes.
 * This focused host link supplies the common canonical skeleton definitions;
 * the two unused external headers are link sentinels only and are never
 * selected by the Facility/Runway prop dependency test. */
#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \
    ModelSkeleton SKELETON(NAME) = {NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0};
#include "assets/embedded/skeletons/standard_object.inc.c"
#include "assets/embedded/skeletons/prop_weapon.inc.c"
#include "assets/embedded/skeletons/guard.inc.c"

ModelFileHeader chrbug_header;
ModelFileHeader greatguard2_header;
