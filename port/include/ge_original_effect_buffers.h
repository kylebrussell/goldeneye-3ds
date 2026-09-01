#ifndef GE_ORIGINAL_EFFECT_BUFFERS_H
#define GE_ORIGINAL_EFFECT_BUFFERS_H

#include <stddef.h>

/*
 * Binds stage-lifetime storage to the original explosion globals and performs
 * the single-player portion of alloc_explosion_smoke_casing_scorch_impact_buffers.
 * The buffers remain owned by this platform allocation boundary; all effect
 * construction and mutation continues through the unchanged decompiled code.
 */
void ge_original_effect_buffers_reset_single_player(void);

/* Introspection used by focused host coverage and the 3DS startup audit. */
size_t ge_original_effect_explosion_capacity(void);
size_t ge_original_effect_smoke_capacity(void);
size_t ge_original_effect_particle_capacity(void);

#endif
