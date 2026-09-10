#ifndef GE_ORIGINAL_ANIMATION_TABLE_REFS_H
#define GE_ORIGINAL_ANIMATION_TABLE_REFS_H

#include <stdint.h>
#include "game/initanitable.h"

/* Original s32 animation references are pointer-sized on N64/ARM. Keep those
 * declarations unchanged there; the full native host graph requires a wide
 * owner shared by the generated AI/scheduler consumers and stage startup. */
#if defined(GE_PORT_FULL_GAMEPLAY_RUNTIME) && UINTPTR_MAX > UINT32_MAX
#define GE_PORT_WIDE_ANIMATION_REFS 1
extern uintptr_t ge_original_animation_table_refs[];
#define animation_table_ptrs1 ge_original_animation_table_refs
int ge_original_animation_table_refs_materialize(void);
#endif

#endif
