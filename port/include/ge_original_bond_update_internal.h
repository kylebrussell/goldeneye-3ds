#ifndef GE_ORIGINAL_BOND_UPDATE_INTERNAL_H
#define GE_ORIGINAL_BOND_UPDATE_INTERNAL_H

#include <limits.h>

#include "ge_original_bond_animation_internal.h"
#include "random.h"

f32 ge_original_bond_head_breathing_provider(void);

#define bondviewGetBondBreathing ge_original_bond_head_breathing_provider

#endif
