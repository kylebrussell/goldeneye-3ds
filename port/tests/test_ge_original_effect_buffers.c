#include "ge_original_effect_buffers.h"

#include <assert.h>
#include <stdio.h>
#include <bondconstants.h>

#include "game/explosion.h"

/* Exact explosion.c globals normally supplied by the generated runtime slice. */
struct Smoke *g_SmokeBuffer;
struct Explosion *g_ExplosionBuffer;
s32 max_particles;
struct FlyingParticles *g_FlyingParticlesBuffer;
struct BulletImpact *g_BulletImpactBuffer;
s32 g_NumExplosionEntries;
s32 g_NumSmokeEntries;
s32 g_NumParticleEntries;

int main(void)
{
    ge_original_effect_buffers_reset_single_player();

    assert(g_ExplosionBuffer != NULL);
    assert(g_SmokeBuffer != NULL);
    assert(g_ScorchBuffer != NULL);
    assert(g_BulletImpactBuffer != NULL);
    assert(g_FlyingParticlesBuffer != NULL);
    assert(max_particles == MAX_FLYING_PARTICLES);
    assert(ge_original_effect_explosion_capacity() == EXPLOSION_BUFFER_LEN);
    assert(ge_original_effect_smoke_capacity() == SMOKE_BUFFER_LEN);
    assert(ge_original_effect_particle_capacity() == MAX_FLYING_PARTICLES);
    assert(g_NumExplosionEntries == 0 && g_NumSmokeEntries == 0);
    assert(g_NumScorchEntries == 0 && g_NumImpactEntries == 0);
    assert(g_NumParticleEntries == 0);
    assert(g_SpExplosionDamageMult == 1.0f);
    assert(g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1].prop == NULL);
    assert(g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1]
               .parts[EXPLOSION_PARTS_LEN - 1].frame == 0);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].prop == NULL);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1]
               .parts[SMOKE_PARTS_LEN - 1].size == 0.0f);
    assert(g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid == -1);
    assert(g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room == -1);
    assert(g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 == 0);

    g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1].prop = (PropRecord *)1;
    g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1]
        .parts[EXPLOSION_PARTS_LEN - 1].frame = 7;
    g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].prop = (PropRecord *)1;
    g_SmokeBuffer[SMOKE_BUFFER_LEN - 1]
        .parts[SMOKE_PARTS_LEN - 1].size = 3.0f;
    g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid = 4;
    g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room = 4;
    g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 = 4;
    g_NumExplosionEntries = 4;
    g_NumSmokeEntries = 4;
    g_NumScorchEntries = 4;
    g_NumImpactEntries = 4;
    g_NumParticleEntries = 4;
    g_SpExplosionDamageMult = 4.0f;

    ge_original_effect_buffers_reset_single_player();

    assert(g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1].prop == NULL);
    assert(g_ExplosionBuffer[EXPLOSION_BUFFER_LEN - 1]
               .parts[EXPLOSION_PARTS_LEN - 1].frame == 0);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1].prop == NULL);
    assert(g_SmokeBuffer[SMOKE_BUFFER_LEN - 1]
               .parts[SMOKE_PARTS_LEN - 1].size == 0.0f);
    assert(g_ScorchBuffer[SCORCH_BUFFER_LEN - 1].roomid == -1);
    assert(g_BulletImpactBuffer[BULLET_IMPACT_BUFFER_LEN - 1].room == -1);
    assert(g_FlyingParticlesBuffer[MAX_FLYING_PARTICLES - 1].unk00 == 0);
    assert(g_NumExplosionEntries == 0 && g_NumSmokeEntries == 0);
    assert(g_NumScorchEntries == 0 && g_NumImpactEntries == 0);
    assert(g_NumParticleEntries == 0);
    assert(g_SpExplosionDamageMult == 1.0f);

    for (unsigned count = 2; count <= 4; ++count) {
        assert(ge_original_effect_buffers_reset_players(count, LEVELID_DAM));
        assert(!g_ScorchBuffer && !g_NumScorchEntries);
        assert(max_particles == MAX_FLYING_PARTICLES/(s32)count);
        assert(ge_original_effect_particle_capacity() == (size_t)max_particles);
        assert(!g_FlyingParticlesBuffer[max_particles-1].unk00);
        assert(ge_original_effect_buffers_reset_players(count, LEVELID_STREETS));
        assert(max_particles == (MAX_FLYING_PARTICLES/(s32)count)/2);
        assert(ge_original_effect_buffers_reset_players(count, LEVELID_DEPOT));
        assert(max_particles == (MAX_FLYING_PARTICLES/(s32)count)/2);
        assert(!ge_original_effect_buffers_reset_players(0, LEVELID_DAM));
        assert(!ge_original_effect_buffers_reset_players(5, LEVELID_DAM));
        assert(max_particles == (MAX_FLYING_PARTICLES/(s32)count)/2);
    }
    ge_original_effect_buffers_reset_single_player();
    assert(g_ScorchBuffer && max_particles == MAX_FLYING_PARTICLES);
    puts("canonical effect buffer reset passed: solo, 2/3/4 players, Streets/Depot caps");
    return 0;
}
