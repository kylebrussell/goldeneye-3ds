#include <ultra64.h>
#include <bondgame.h>
#include <bondtypes.h>
#include "game/chrobjdata.h"

/* Exact embedded decomp assets used by sets_a_bunch_of_BONDdata_values_to_default. */
#undef MODELSKELETON
#define MODELSKELETON(NAME, NUMJOINTS, SKELSIZE) \
    ModelSkeleton SKELETON(NAME) = { \
        NUMJOINTS, 0, JOINTLIST(NAME), SKELSIZE, 0 \
    };
#include <assets/embedded/skeletons/player_gait_object.inc.c>
#include <assets/embedded/player_gait_object/modelnode.inc.c>
#include <assets/embedded/player_gait_object/header.inc.c>
