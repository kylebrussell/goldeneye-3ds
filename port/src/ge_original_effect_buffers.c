#include "ge_original_effect_buffers.h"

#include <stddef.h>

#include "game/explosion.h"

/*
 * The N64 stage allocator supplies these arrays to the exact globals in
 * explosion.c. The 3DS frontend does not run the monolithic lvlReset path, so
 * this module is the equivalent stage-allocation boundary. Capacities and
 * initialization order below are the single-player branch of
 * alloc_explosion_smoke_casing_scorch_impact_buffers.
 */
static _Alignas(16) struct Explosion explosion_stage_storage[
    EXPLOSION_BUFFER_LEN];
static _Alignas(16) struct Smoke smoke_stage_storage[SMOKE_BUFFER_LEN];
static _Alignas(16) struct Scorch scorch_stage_storage[SCORCH_BUFFER_LEN];
static _Alignas(16) struct BulletImpact bullet_impact_stage_storage[
    BULLET_IMPACT_BUFFER_LEN];
static _Alignas(16) struct FlyingParticles particle_stage_storage[
    MAX_FLYING_PARTICLES];

/* These canonical explosion.c globals are not yet carried by the reduced
 * explosion source slice. Weak ownership lets the source slice supersede this
 * platform storage automatically when its full data block is linked later. */
__attribute__((weak)) f32 g_SpExplosionDamageMult = 1.0f;
__attribute__((weak)) struct Scorch *g_ScorchBuffer;
__attribute__((weak)) s32 g_NumScorchEntries;
__attribute__((weak)) s32 g_NumImpactEntries;

void ge_original_effect_buffers_reset_single_player(void)
{
    s32 i;
    s32 j;

    g_NumExplosionEntries = 0;
    g_NumSmokeEntries = 0;
    g_NumParticleEntries = 0;
    g_NumScorchEntries = 0;
    g_NumImpactEntries = 0;
    g_SpExplosionDamageMult = 1.0f;

    g_ExplosionBuffer = explosion_stage_storage;

    for (i = 0; i < EXPLOSION_BUFFER_LEN; i++) {
        g_ExplosionBuffer[i].prop = NULL;

        for (j = 0; j < EXPLOSION_PARTS_LEN; j++) {
            g_ExplosionBuffer[i].parts[j].frame = 0;
        }
    }

    g_SmokeBuffer = smoke_stage_storage;

    for (i = 0; i < SMOKE_BUFFER_LEN; i++) {
        g_SmokeBuffer[i].prop = NULL;

        for (j = 0; j < SMOKE_PARTS_LEN; j++) {
            g_SmokeBuffer[i].parts[j].size = 0.0f;
        }
    }

    g_ScorchBuffer = scorch_stage_storage;

    for (i = 0; i < SCORCH_BUFFER_LEN; i++) {
        g_ScorchBuffer[i].roomid = -1;
    }

    g_BulletImpactBuffer = bullet_impact_stage_storage;

    for (i = 0; i < BULLET_IMPACT_BUFFER_LEN; i++) {
        g_BulletImpactBuffer[i].room = -1;
    }

    max_particles = MAX_FLYING_PARTICLES;
    g_FlyingParticlesBuffer = particle_stage_storage;

    for (i = 0; i < max_particles; i++) {
        g_FlyingParticlesBuffer[i].unk00 = 0;
    }
}

size_t ge_original_effect_explosion_capacity(void)
{
    return EXPLOSION_BUFFER_LEN;
}

size_t ge_original_effect_smoke_capacity(void)
{
    return SMOKE_BUFFER_LEN;
}

size_t ge_original_effect_particle_capacity(void)
{
    return MAX_FLYING_PARTICLES;
}
