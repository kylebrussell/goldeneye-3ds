#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "ge_original_bond_camera.h"
#include "ge_original_first_person_pose.h"

static struct player player;
static WeaponStats stats;
static ModelFileHeader item_header;
struct player *g_CurrentPlayer;
static uint32_t frustum_publications;

void ge_original_bondview_update_frustum_planes_exact(void)
{
    ++frustum_publications;
}

struct player *ge_original_spawn_player_get(void) { return &player; }
ITEM_IDS get_item_in_hand_or_watch_menu(GUNHAND hand)
{
    return player.hands[hand].weaponnum_watchmenu >= 0
        ? player.hands[hand].weaponnum_watchmenu : player.hands[hand].weaponnum;
}
WeaponStats *get_ptr_item_statistics(ITEM_IDS item) { (void)item; return &stats; }
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask)
{ (void)item; return (stats.BitFlags & mask) != 0; }
ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS item)
{ (void)item; return &item_header; }
f32 gunSetHorizontalOffset(GUNHAND hand)
{ return hand == GUNRIGHT ? stats.PosX : -stats.PosX; }
f32 sub_GAME_7F05DCB8(GUNHAND hand) { return player.hands[hand].field_A34; }
s32 Gun_hand_without_item(GUNHAND hand)
{
    return player.hand_invisible[hand] > 0
        || (player.hand_item[hand] == 0 && player.field_2A44[hand] < 0);
}
u32 randomGetNext(void) { return UINT32_C(0x40000000); }

void matrix_4x4_set_identity(Mtxf *m)
{
    memset(m, 0, sizeof(*m));
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1.0f;
}
void matrix_4x4_copy(Mtxf *src, Mtxf *dst) { *dst = *src; }
void matrix_4x4_set_position(coord3d *p, Mtxf *m)
{ m->m[3][0] = p->x; m->m[3][1] = p->y; m->m[3][2] = p->z; }
void matrix_4x4_multiply_homogeneous(Mtxf *a, Mtxf *b, Mtxf *out)
{
    Mtxf result;
    unsigned i, j, k;
    for (i = 0; i < 4; i++) for (j = 0; j < 4; j++) {
        result.m[i][j] = 0.0f;
        for (k = 0; k < 4; k++) result.m[i][j] += a->m[i][k] * b->m[k][j];
    }
    *out = result;
}
void matrix_4x4_multiply_homogeneous_in_place(Mtxf *a, Mtxf *b)
{ Mtxf result; matrix_4x4_multiply_homogeneous(a, b, &result); *b = result; }
void matrix_4x4_multiply_in_place(Mtxf *a, Mtxf *b)
{ matrix_4x4_multiply_homogeneous_in_place(a, b); }
void matrix_4x4_set_basis_and_position_target(Mtxf *m, f32 px, f32 py, f32 pz,
    f32 tx, f32 ty, f32 tz, f32 ux, f32 uy, f32 uz)
{
    (void)px; (void)py; (void)pz; (void)tx; (void)ty; (void)tz;
    (void)ux; (void)uy; (void)uz; matrix_4x4_set_identity(m);
}
void matrix_4x4_align(Mtxf *m, f32 angle, f32 x, f32 y, f32 z)
{ (void)angle; (void)x; (void)y; (void)z; matrix_4x4_set_identity(m); }
void matrix_column_1_scalar_multiply(f32 s, f32 m[4])
{ for (unsigned i = 0; i < 4; i++) m[i] *= s; }
void matrix_column_3_scalar_multiply(f32 s, f32 m[4])
{ for (unsigned i = 0; i < 4; i++) m[i * 4 + 2] *= s; }
void matrix_scalar_multiply(f32 s, f32 m[4])
{ for (unsigned i = 0; i < 12; i++) m[i] *= s; }
void matrix_4x4_set_identity_and_position(coord3d *p, Mtxf *m)
{ matrix_4x4_set_identity(m); matrix_4x4_set_position(p, m); }
void mtx4TransformVecInPlace(Mtxf *m, coord3d *v)
{
    coord3d r = {
        v->x*m->m[0][0]+v->y*m->m[1][0]+v->z*m->m[2][0]+m->m[3][0],
        v->x*m->m[0][1]+v->y*m->m[1][1]+v->z*m->m[2][1]+m->m[3][1],
        v->x*m->m[0][2]+v->y*m->m[1][2]+v->z*m->m[2][2]+m->m[3][2]};
    *v = r;
}

int main(void)
{
    GeOriginalFirstPersonPoseState state;
    GeOriginalFirstPersonPosePublication publication;
    ModelNode root;
    ModelNode muzzle_node;
    union ModelRoData muzzle_data;
    ModelNode *switches[4] = {0};
    GeOriginalBondCameraConfig camera_config;
    GeOriginalBondCameraResult camera_result;

    memset(&player, 0, sizeof(player));
    memset(&stats, 0, sizeof(stats));
    memset(&item_header, 0, sizeof(item_header));
    memset(&root, 0, sizeof(root));
    memset(&muzzle_node, 0, sizeof(muzzle_node));
    memset(&muzzle_data, 0, sizeof(muzzle_data));
    memset(&camera_config, 0, sizeof(camera_config));
    memset(&camera_result, 0, sizeof(camera_result));
    stats.PosX = 7.0f; stats.PosY = -12.0f; stats.PosZ = -30.0f;
    stats.MuzzleFlashExtension = 2.0f;
    stats.BitFlags = WEAPONSTATBITFLAG_SHOW_FIRST_PERSON;
    item_header.RootNode = &root;
    item_header.Switches = switches;
    item_header.numSwitches = 4;
    switches[3] = &muzzle_node;
    muzzle_node.Data = &muzzle_data;
    ((coord3d *)&muzzle_data)->z = -5.0f;
    matrix_4x4_set_identity((Mtxf *)camera_result.view);
    matrix_4x4_set_identity((Mtxf *)camera_result.view_to_world);
    matrix_4x4_set_identity((Mtxf *)camera_result.projection);
    camera_result.view_to_world[3][0] = 100.0f;
    camera_result.view_to_world[3][1] = 200.0f;
    camera_result.view_to_world[3][2] = 300.0f;
    camera_config.viewport_scale[0] = 640;
    camera_config.viewport_scale[1] = 480;
    camera_config.viewport_translation[0] = 640;
    camera_config.viewport_translation[1] = 480;
    camera_config.vertical_fov_degrees = 60.0f;
    camera_config.perspective_aspect = 4.0f / 3.0f;
    camera_config.near_distance = 100.0f;
    assert(ge_original_bond_camera_publish_live_player(
        &camera_config, &camera_result));
    assert(g_CurrentPlayer == &player && frustum_publications == 1U);
    player.field_FFC.x = player.c_screenleft + player.c_halfwidth;
    player.field_FFC.y = player.c_screentop + player.c_halfheight;
    player.gunposamplitude = 1.0f;
    player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].weaponnum_watchmenu = ITEM_NOTHING;
    player.hands[GUNRIGHT].field_A40 = 1000.0f;
    for (unsigned i = 0; i < 4; i++) {
        player.hands[GUNRIGHT].blendlook[i].z = -1.0f;
        player.hands[GUNRIGHT].blendup[i].y = 1.0f;
    }
    ge_original_first_person_pose_bind(&state);

    /* A relocated model can be cached while the exact intro loadout switch is
     * still pending.  It must not become visible ahead of SWITCH_LOWER: doing
     * so produces a visible-at-spawn, hidden-at-SWITCH_HOLD dropout. */
    player.hands[GUNRIGHT].weaponnum = ITEM_UNARMED;
    player.hands[GUNRIGHT].weapon_next_weapon = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].weapon_current_animation =
        GUN_ANIM_STATE_SWITCH_LOWER;
    player.hand_invisible[GUNRIGHT] = 0;
    player.hand_item[GUNRIGHT] = ITEM_UNARMED;
    player.field_2A44[GUNRIGHT] = -1;
    assert(ge_original_first_person_pose_bind_hand_model(
        GUNRIGHT, ITEM_WPPKSIL, &item_header) == GE_ORIGINAL_FIRST_PERSON_POSE_OK);
    assert(player.copy_of_body_obj_header[GUNRIGHT].RootNode == &root);
    assert(player.hand_invisible[GUNRIGHT] == 0);
    assert(player.hand_item[GUNRIGHT] == ITEM_UNARMED);
    assert(ge_original_first_person_pose_tick(1, 1.0f)
           == GE_ORIGINAL_FIRST_PERSON_POSE_ITEM_MISMATCH);

    /* Model loading completes after the original swap commits weaponnum.  The
     * same binding now has the exact successful on-demand hand semantics and
     * remains published as camera pose changes over sustained movement. */
    player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].weapon_current_animation = 0;
    assert(ge_original_first_person_pose_bind_hand_model(
        GUNRIGHT, ITEM_WPPKSIL, &item_header) == GE_ORIGINAL_FIRST_PERSON_POSE_OK);
    for (unsigned frame = 0; frame < 24U; ++frame) {
        camera_result.view_to_world[3][0] = 100.0f + (float)frame * 3.0f;
        camera_result.view_to_world[3][2] = 300.0f + (float)frame * 7.0f;
        assert(ge_original_bond_camera_publish_live_player(
            &camera_config, &camera_result));
        assert(ge_original_first_person_pose_tick(1, 1.0f)
               == GE_ORIGINAL_FIRST_PERSON_POSE_OK);
        assert(ge_original_first_person_pose_snapshot(
            GUNRIGHT, &publication));
        assert(publication.visible);
        assert(publication.generation == frame + 1U);
        assert(isfinite(publication.throw_world[3][0]));
        assert(isfinite(publication.throw_world[3][2]));
    }
    assert(ge_original_first_person_pose_ready(GUNRIGHT));
    assert(ge_original_first_person_pose_snapshot(GUNRIGHT, &publication));
    assert(publication.generation == 24U && publication.visible);
    assert(isfinite(publication.muzzle_world[0]));
    assert(publication.muzzle_world[0] > 90.0f);
    assert(publication.throw_world[3][0] != publication.gun_camera[3][0]);
    assert(frustum_publications == 25U);
    player.viewtoworldmtxf = NULL;
    assert(ge_original_first_person_pose_tick(1, 1.0f)
           == GE_ORIGINAL_FIRST_PERSON_POSE_NO_VIEW_TO_WORLD);
    puts("original first-person pose publication passed");
    return 0;
}
