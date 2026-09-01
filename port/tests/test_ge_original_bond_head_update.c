#include "ge_original_bond_head_update.h"
#include "ge_original_player_spawn_internal.h"

typedef int PLAYERFLAG;
#include "game/bondhead.h"
#include "game/bondview.h"
#include "game/model.h"

#include <assert.h>
#include <math.h>
#include <string.h>

s32 g_ClockTimer;
f32 g_GlobalTimerDelta;

static struct player test_player;
static f32 test_anim_speed;

void bheadUpdateRot(coord3d *lookvel, coord3d *upvel);

struct player *ge_original_spawn_player_get(void)
{
    return &test_player;
}

u32 randomGetNext(void)
{
    return 0;
}

f32 modelGetAbsAnimSpeed(Model *model)
{
    (void)model;
    return test_anim_speed;
}

u32 modelIsAnimMergingEnabled(void)
{
    return 0;
}

void modelSetAnimMergingEnabled(s32 enabled)
{
    (void)enabled;
}

void modelTickAnim(Model *model, s32 ticks, s32 arg2)
{
    (void)model;
    (void)ticks;
    (void)arg2;
}

void subcalcpos(Model *model)
{
    (void)model;
}

void subcalcmatrices(ModelRenderData *render_data, Model *model)
{
    (void)render_data;
    (void)model;
}

void getsuboffset(Model *model, coord3d *offset)
{
    (void)model;
    offset->x = 0.0f;
    offset->y = 0.0f;
    offset->z = 0.0f;
}

void setsuboffset(Model *model, struct coord3d *offset)
{
    (void)model;
    (void)offset;
}

static void assert_near(f32 actual, f32 expected)
{
    assert(fabsf(actual - expected) < 0.00001f);
}

static void test_rotation_damping(void)
{
    coord3d look = {{0.25f, -0.125f, 1.0f}};
    coord3d up = {{0.125f, 1.0f, -0.25f}};

    memset(&test_player, 0, sizeof(test_player));
    test_player.headdamp = 0.5f;
    test_player.resetheadrot = TRUE;
    g_ClockTimer = 1;

    bheadUpdateRot(&look, &up);

    assert(test_player.resetheadrot == FALSE);
    assert_near(test_player.headlook.f[0], look.f[0]);
    assert_near(test_player.headlook.f[1], look.f[1]);
    assert_near(test_player.headlook.f[2], look.f[2]);
    assert_near(test_player.headup.f[0], up.f[0]);
    assert_near(test_player.headup.f[1], up.f[1]);
    assert_near(test_player.headup.f[2], up.f[2]);
}

static void test_breathing(void)
{
    memset(&test_player, 0, sizeof(test_player));
    test_player.headanim = 0;
    test_player.bondbreathing = 0.4f;

    test_anim_speed = 0.0f;
    assert_near(ge_original_bond_head_breathing_value(), 0.009166667f);

    test_anim_speed = 0.5f;
    assert_near(ge_original_bond_head_breathing_value(), 0.5f / 17.5f);

    test_player.headanim = -1;
    assert_near(ge_original_bond_head_breathing_value(), 0.0f);
}

static void test_idle_update(void)
{
    memset(&test_player, 0, sizeof(test_player));
    test_player.headanim = 0;
    test_player.standheight = 100.0f;
    test_player.headdamp = 0.99f;
    test_player.resetheadpos = TRUE;
    test_player.resetheadrot = TRUE;
    test_player.standlook[0].f[2] = 1.0f;
    test_player.standlook[1].f[2] = 1.0f;
    test_player.standup[0].f[1] = 1.0f;
    test_player.standup[1].f[1] = 1.0f;
    g_ClockTimer = 1;
    g_GlobalTimerDelta = 1.0f;
    test_anim_speed = 0.0f;

    assert(ge_original_bond_head_update_tick(0.0f, 0.0f));
    assert_near(test_player.headpos.f[0], 0.0f);
    assert_near(test_player.headpos.f[1], 100.0f);
    assert_near(test_player.headpos.f[2], 0.0f);
    assert_near(test_player.headlook.f[2], 1.0f);
    assert_near(test_player.headup.f[1], 1.0f);
}

int main(void)
{
    test_rotation_damping();
    test_breathing();
    test_idle_update();
    return 0;
}
