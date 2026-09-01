#include "ge_original_first_person_pose.h"

#include <math.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/gun.h"
#include "game/matrixmath.h"
#include "ge_original_player_spawn_internal.h"

#define GE_GUN_SPRING_DAMP 0.95f
#define GE_GUN_SPRING_SCALE 0.050000012f
#define GE_GUN_U32_RANGE_F 4294967296.0f
#define GE_GUN_MODEL_SCALE 0.10000001f

/* Canonical gun.c initial pose vectors used by gunUpdateAndFire. */
static const coord3d ge_gun_zero = {0.0f, 0.0f, 0.0f};
static const coord3d ge_gun_look = {0.0f, 0.0f, -1.0f};
static const coord3d ge_gun_up = {0.0f, 1.0f, 0.0f};

static GeOriginalFirstPersonPoseState *pose_state;

ITEM_IDS get_item_in_hand_or_watch_menu(GUNHAND hand);
WeaponStats *get_ptr_item_statistics(ITEM_IDS item);
u32 bondwalkItemCheckBitflags(ITEM_IDS item, u32 mask);
ModelFileHeader *get_ptr_weapon_model_header_line(ITEM_IDS item);
f32 gunSetHorizontalOffset(GUNHAND hand);
f32 sub_GAME_7F05DCB8(GUNHAND hand);
s32 Gun_hand_without_item(GUNHAND hand);
u32 randomGetNext(void);
void mtx4TransformVecInPlace(Mtxf *matrix, coord3d *vector);

/* Exact coord3dCatmullRomInterp body from matrixmath_misc.c. */
static void pose_catmull_rom(coord3d *p0, coord3d *p1, coord3d *p2,
                             coord3d *p3, f32 fraction, coord3d *result)
{
    f32 mult0;
    f32 mult1;
    f32 mult2;
    f32 mult3;
    f32 squared = fraction * fraction;
    f32 cubed = fraction * fraction * fraction;
    mult0 = squared - 0.5f * (fraction + cubed);
    mult1 = 1.5f * cubed - 2.5f * squared + 1.0f;
    mult2 = -1.5f * cubed + 2.0f * squared + 0.5f * fraction;
    mult3 = 0.5f * (cubed - squared);
    result->x = mult0 * p0->x + mult1 * p1->x
        + mult2 * p2->x + mult3 * p3->x;
    result->y = mult0 * p0->y + mult1 * p1->y
        + mult2 * p2->y + mult3 * p3->y;
    result->z = mult0 * p0->z + mult1 * p1->z
        + mult2 * p2->z + mult3 * p3->z;
}

static int finite_nonzero_matrix(const Mtxf *matrix)
{
    float magnitude = 0.0f;
    unsigned row;
    unsigned column;
    if (matrix == NULL) return 0;
    for (row = 0; row < 4U; row++) {
        for (column = 0; column < 4U; column++) {
            if (!isfinite(matrix->m[row][column])) return 0;
            magnitude += fabsf(matrix->m[row][column]);
        }
    }
    return magnitude > 0.0f;
}

static GeOriginalFirstPersonPoseStatus pose_one_hand(
    struct player *player, unsigned handnum, s32 clock_timer,
    f32 global_timer_delta)
{
    struct hand *hand = &player->hands[handnum];
    ModelFileHeader *header = pose_state->model_header[handnum];
    ITEM_IDS item = get_item_in_hand_or_watch_menu((GUNHAND)handnum);
    WeaponStats *itemstats;
    coord3d gunofs = ge_gun_zero;
    coord3d blendedpos = ge_gun_zero;
    coord3d blendedlook = ge_gun_look;
    coord3d blendedup = ge_gun_up;
    Mtxf rotmtx;
    Mtxf tmpmtx;
    Mtxf gunmtx;
    s32 index;
    s32 iteration;

    if (header == NULL) return GE_ORIGINAL_FIRST_PERSON_POSE_NO_MODEL;
    if ((s32)item != pose_state->model_item[handnum])
        return GE_ORIGINAL_FIRST_PERSON_POSE_ITEM_MISMATCH;
    if (!finite_nonzero_matrix(player->viewtoworldmtxf))
        return GE_ORIGINAL_FIRST_PERSON_POSE_NO_VIEW_TO_WORLD;
    if (!(player->c_screenwidth > 0.0f)
            || !(player->c_screenheight > 0.0f))
        return GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_VIEWPORT;
    itemstats = get_ptr_item_statistics(item);
    if (itemstats == NULL) return GE_ORIGINAL_FIRST_PERSON_POSE_NO_MODEL;

    /* Canonical gunUpdateAndFire prefix: dual interpolation and hand sway. */
    if (handnum == GUNRIGHT) {
        if (bondwalkItemCheckBitflags(
                get_item_in_hand_or_watch_menu(GUNLEFT),
                WEAPONSTATBITFLAG_SHOW_FIRST_PERSON)) {
            hand->field_A34 += 2.0f * global_timer_delta / 240.0f;
            if (hand->field_A34 > 2.0f) hand->field_A34 = 2.0f;
        } else {
            hand->field_A34 -= 2.0f * global_timer_delta / 240.0f;
            if (hand->field_A34 < 0.0f) hand->field_A34 = 0.0f;
        }
    } else if (bondwalkItemCheckBitflags(
            get_item_in_hand_or_watch_menu(GUNRIGHT),
            WEAPONSTATBITFLAG_SHOW_FIRST_PERSON)) {
        hand->field_A34 -= 2.0f * global_timer_delta / 240.0f;
        if (hand->field_A34 < -2.0f) hand->field_A34 = -2.0f;
    } else {
        hand->field_A34 += 2.0f * global_timer_delta / 240.0f;
        if (hand->field_A34 > 0.0f) hand->field_A34 = 0.0f;
    }

    index = hand->curblendpos;
    if (index < 0 || index > 3) return GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_ARGUMENT;
    pose_catmull_rom(&hand->blendpos[(index + 3) % 4],
        &hand->blendpos[index], &hand->blendpos[(index + 1) % 4],
        &hand->blendpos[(index + 2) % 4], hand->dampt, &blendedpos);
    pose_catmull_rom(&hand->blendlook[(index + 3) % 4],
        &hand->blendlook[index], &hand->blendlook[(index + 1) % 4],
        &hand->blendlook[(index + 2) % 4], hand->dampt, &blendedlook);
    pose_catmull_rom(&hand->blendup[(index + 3) % 4],
        &hand->blendup[index], &hand->blendup[(index + 1) % 4],
        &hand->blendup[(index + 2) % 4], hand->dampt, &blendedup);
    blendedpos.x = blendedpos.x * player->gunposamplitude
        + hand->weapon_theta_displacement + sub_GAME_7F05DCB8((GUNHAND)handnum);
    blendedpos.y = blendedpos.y * player->gunposamplitude
        + hand->weapon_verta_displacement;
    blendedpos.z *= player->gunposamplitude;
    for (iteration = 0; iteration < clock_timer; iteration++) {
        hand->spring_pos_x = GE_GUN_SPRING_DAMP * hand->spring_pos_x + blendedpos.x;
        hand->spring_pos_y = GE_GUN_SPRING_DAMP * hand->spring_pos_y + blendedpos.y;
        hand->spring_pos_z = GE_GUN_SPRING_DAMP * hand->spring_pos_z + blendedpos.z;
        hand->spring_look_x = GE_GUN_SPRING_DAMP * hand->spring_look_x + blendedlook.x;
        hand->spring_look_y = GE_GUN_SPRING_DAMP * hand->spring_look_y + blendedlook.y;
        hand->spring_look_z = GE_GUN_SPRING_DAMP * hand->spring_look_z + blendedlook.z;
        hand->spring_up_x = GE_GUN_SPRING_DAMP * hand->spring_up_x + blendedup.x;
        hand->spring_up_y = GE_GUN_SPRING_DAMP * hand->spring_up_y + blendedup.y;
        hand->spring_up_z = GE_GUN_SPRING_DAMP * hand->spring_up_z + blendedup.z;
    }
    hand->sway_pos_x = hand->spring_pos_x * GE_GUN_SPRING_SCALE;
    hand->sway_pos_y = hand->spring_pos_y * GE_GUN_SPRING_SCALE;
    hand->sway_pos_z = hand->spring_pos_z * GE_GUN_SPRING_SCALE;
    hand->sway_look_x = hand->spring_look_x * GE_GUN_SPRING_SCALE;
    hand->sway_look_y = hand->spring_look_y * GE_GUN_SPRING_SCALE;
    hand->sway_look_z = hand->spring_look_z * GE_GUN_SPRING_SCALE;
    hand->sway_up_x = hand->spring_up_x * GE_GUN_SPRING_SCALE;
    hand->sway_up_y = hand->spring_up_y * GE_GUN_SPRING_SCALE;
    hand->sway_up_z = hand->spring_up_z * GE_GUN_SPRING_SCALE;

    gunofs.x = gunSetHorizontalOffset((GUNHAND)handnum) + hand->sway_pos_x
        + (handnum == GUNRIGHT ? hand->gunofs2_x : -hand->gunofs2_x);
    gunofs.y = hand->gunofs2_y + itemstats->PosY + hand->sway_pos_y
        + 5.0f * player->ducking_height_offset / -100.0f;
    gunofs.z = hand->gunofs2_z + itemstats->PosZ + hand->sway_pos_z
        + 15.0f * player->ducking_height_offset / -100.0f;
    if (hand->weapon_firing_status
            && bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_00000020)) {
        if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_00000040))
            gunofs.x += 0.3f - (f32)randomGetNext()
                * (1.0f / GE_GUN_U32_RANGE_F) * 0.6f;
        gunofs.y += 0.3f - (f32)randomGetNext()
            * (1.0f / GE_GUN_U32_RANGE_F) * 0.6f;
        gunofs.z += 0.3f - (f32)randomGetNext()
            * (1.0f / GE_GUN_U32_RANGE_F) * 0.6f;
    }
    gunofs.x += ((player->field_FFC.x - player->c_screenleft
        - player->c_screenwidth * 0.5f) * itemstats->PlayZ)
        / (player->c_screenwidth * 0.5f);
    if (player->field_FFC.y - player->c_screentop
            > player->c_screenheight * 0.5f)
        gunofs.y -= ((player->field_FFC.y - player->c_screentop
            - player->c_screenheight * 0.5f) * itemstats->PlayY)
            / (player->c_screenheight * 0.5f);
    else
        gunofs.y -= ((player->field_FFC.y - player->c_screentop
            - player->c_screenheight * 0.5f) * itemstats->PlayX)
            / (player->c_screenheight * 0.5f);

    matrix_4x4_set_identity(&rotmtx);
    if (hand->field_92C) {
        gunofs.x += hand->field_8EC.m[3][0];
        gunofs.y += hand->field_8EC.m[3][1];
        gunofs.z += hand->field_8EC.m[3][2];
        matrix_4x4_multiply_homogeneous_in_place(&hand->field_8EC, &rotmtx);
        rotmtx.m[3][0] = rotmtx.m[3][1] = rotmtx.m[3][2] = 0.0f;
    } else {
        hand->field_8E8 = hand->field_8DC = hand->field_8E0 = hand->field_8E4 = 0.0f;
    }
    matrix_4x4_set_basis_and_position_target(&tmpmtx, 0.0f, 0.0f, 0.0f,
        hand->sway_look_x, hand->sway_look_y, hand->sway_look_z,
        hand->sway_up_x, hand->sway_up_y, hand->sway_up_z);
    matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    matrix_4x4_align(&tmpmtx, 0.0f, gunofs.x - hand->field_A38,
        gunofs.y - hand->field_A3C, gunofs.z - hand->field_A40);
    matrix_4x4_multiply_homogeneous_in_place(&tmpmtx, &rotmtx);
    matrix_4x4_copy(&rotmtx, &gunmtx);
    matrix_4x4_set_position(&gunofs, &gunmtx);
    matrix_4x4_copy(&gunmtx, &hand->gunmtx_camspace);
    matrix_4x4_copy(&hand->throw_item_pos_related,
                    &hand->throw_item_pos_related_prev);
    matrix_4x4_multiply_homogeneous(player->viewtoworldmtxf,
        &hand->gunmtx_camspace, &hand->throw_item_pos_related);
    hand->field_87F = get_ptr_weapon_model_header_line(item) != NULL
        && bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_SHOW_FIRST_PERSON)
        && !bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_HIDE_FIRST_PERSON_HAND)
        && hand->weapon_action_state != GUN_ANIM_STATE_SWITCH_SWAP
        && hand->weapon_action_state != GUN_ANIM_STATE_SWITCH_HOLD
        && Gun_hand_without_item((GUNHAND)handnum)
        && player->hand_item[handnum] != ITEM_UNARMED;
    if (hand->weapon_ammo_in_magazine <= 0
            && bondwalkItemCheckBitflags(item,
                WEAPONSTATBITFLAG_SINGLE_USE_RELOAD)) hand->field_87F = 0;
    if (!hand->field_87F) return GE_ORIGINAL_FIRST_PERSON_POSE_HIDDEN;

    if (bondwalkItemCheckBitflags(item, WEAPONSTATBITFLAG_MIRROR_DUAL)
            && handnum == GUNLEFT)
        matrix_column_1_scalar_multiply(-1.0f, gunmtx.m[0]);
    matrix_scalar_multiply(GE_GUN_MODEL_SCALE, gunmtx.m[0]);
    if (header->Switches != NULL && header->numSwitches > 3
            && header->Switches[3] != NULL) {
        coord3d *flashdata = (coord3d *)header->Switches[3]->Data;
        Mtxf flashmtx;
        f32 flashscale = (f32)randomGetNext()
            * (1.0f / GE_GUN_U32_RANGE_F) * 0.25f + 1.0f;
        matrix_4x4_set_identity_and_position(flashdata, &flashmtx);
        matrix_scalar_multiply(flashscale, flashmtx.m[0]);
        matrix_column_3_scalar_multiply(itemstats->MuzzleFlashExtension,
                                         flashmtx.m[0]);
        matrix_4x4_multiply_in_place(&gunmtx, &flashmtx);
        hand->field_B58.x = flashmtx.m[3][0];
        hand->field_B58.y = flashmtx.m[3][1];
        hand->field_B58.z = flashmtx.m[3][2];
        mtx4TransformVecInPlace(player->viewtoworldmtxf, &hand->field_B58);
        hand->field_B64 = -flashmtx.m[3][2];
    } else {
        hand->field_B58.x = hand->throw_item_pos_related.m[3][0];
        hand->field_B58.y = hand->throw_item_pos_related.m[3][1];
        hand->field_B58.z = hand->throw_item_pos_related.m[3][2];
        hand->field_B64 = -hand->gunmtx_camspace.m[3][2];
    }
    return GE_ORIGINAL_FIRST_PERSON_POSE_OK;
}

void ge_original_first_person_pose_bind(GeOriginalFirstPersonPoseState *state)
{
    pose_state = state;
    if (state != NULL) {
        memset(state, 0, sizeof(*state));
        state->model_item[0] = state->model_item[1] = ITEM_NOTHING;
        state->hand_status[0] = state->hand_status[1]
            = GE_ORIGINAL_FIRST_PERSON_POSE_NO_MODEL;
        state->initialized = 1;
    }
}

GeOriginalFirstPersonPoseStatus ge_original_first_person_pose_bind_hand_model(
    unsigned hand, int32_t item, const void *native_model_header)
{
    struct player *player = ge_original_spawn_player_get();
    struct hand *native_hand;
    const ModelFileHeader *header = native_model_header;
    if (pose_state == NULL || !pose_state->initialized || hand >= 2U
            || header == NULL || header->RootNode == NULL)
        return GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_ARGUMENT;
    if (player == NULL) return GE_ORIGINAL_FIRST_PERSON_POSE_NO_PLAYER;
    native_hand = &player->hands[hand];
    pose_state->model_header[hand] = (void *)header;
    pose_state->model_item[hand] = item;
    player->copy_of_body_obj_header[hand] = *header;

    /* Loadout setup queues the unchanged SWITCH_LOWER action before the 3DS
     * model cache is populated.  A cache preload is not the canonical
     * used_to_load_1st_person_model_on_demand handoff: committing hand_item
     * here makes the weapon visible before that queued action, after which
     * the exact SWITCH_SWAP/HOLD frame appears to make it disappear.  Keep
     * the original pending state intact.  gunTickGameplay will perform the
     * exact load/visibility commit after currentPlayerUnEquipWeaponWrapper
     * installs weapon_next_weapon. */
    if (native_hand->weaponnum != item
            && native_hand->weapon_next_weapon == item
            && native_hand->weapon_current_animation
                == GUN_ANIM_STATE_SWITCH_LOWER)
        return GE_ORIGINAL_FIRST_PERSON_POSE_OK;
    player->hand_invisible[hand] = 1;
    player->hand_item[hand] = item;
    player->field_2A44[hand] = -1;
    return GE_ORIGINAL_FIRST_PERSON_POSE_OK;
}

GeOriginalFirstPersonPoseStatus ge_original_first_person_pose_tick(
    int32_t clock_timer, float global_timer_delta)
{
    struct player *player = ge_original_spawn_player_get();
    GeOriginalFirstPersonPoseStatus overall = GE_ORIGINAL_FIRST_PERSON_POSE_OK;
    unsigned successful = 0U;
    unsigned hand;
    if (pose_state == NULL || !pose_state->initialized || clock_timer < 0
            || !isfinite(global_timer_delta))
        return GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_ARGUMENT;
    if (player == NULL) return GE_ORIGINAL_FIRST_PERSON_POSE_NO_PLAYER;
    pose_state->ticks++;
    for (hand = 0; hand < 2U; hand++) {
        struct hand *native_hand = &player->hands[hand];
        GeOriginalFirstPersonPosePublication *out =
            &pose_state->publication[hand];
        GeOriginalFirstPersonPoseStatus status = pose_one_hand(
            player, hand, clock_timer, global_timer_delta);
        pose_state->hand_status[hand] = status;
        if (status == GE_ORIGINAL_FIRST_PERSON_POSE_OK) {
            memcpy(out->gun_camera, native_hand->gunmtx_camspace.m,
                   sizeof(out->gun_camera));
            memcpy(out->throw_world, native_hand->throw_item_pos_related.m,
                   sizeof(out->throw_world));
            memcpy(out->throw_world_previous,
                   native_hand->throw_item_pos_related_prev.m,
                   sizeof(out->throw_world_previous));
            memcpy(out->muzzle_world, native_hand->field_B58.f,
                   sizeof(out->muzzle_world));
            out->camera_depth = native_hand->field_B64;
            out->item = pose_state->model_item[hand];
            out->visible = 1;
            out->generation++;
            pose_state->published_hands++;
            successful++;
        } else {
            out->visible = 0;
            if (overall == GE_ORIGINAL_FIRST_PERSON_POSE_OK) overall = status;
        }
    }
    return successful != 0U ? GE_ORIGINAL_FIRST_PERSON_POSE_OK : overall;
}

int ge_original_first_person_pose_ready(unsigned hand)
{
    return pose_state != NULL && pose_state->initialized && hand < 2U
        && pose_state->model_header[hand] != NULL
        && pose_state->hand_status[hand] == GE_ORIGINAL_FIRST_PERSON_POSE_OK;
}

int32_t ge_original_first_person_pose_current_item(unsigned hand)
{
    struct player *player = ge_original_spawn_player_get();
    if (player == NULL || hand >= 2U) return ITEM_NOTHING;
    return (int32_t)get_item_in_hand_or_watch_menu((GUNHAND)hand);
}

int ge_original_first_person_pose_snapshot(
    unsigned hand, GeOriginalFirstPersonPosePublication *publication)
{
    if (!ge_original_first_person_pose_ready(hand) || publication == NULL)
        return 0;
    *publication = pose_state->publication[hand];
    return 1;
}

const char *ge_original_first_person_pose_status_name(
    GeOriginalFirstPersonPoseStatus status)
{
    switch (status) {
    case GE_ORIGINAL_FIRST_PERSON_POSE_OK: return "ok";
    case GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_FIRST_PERSON_POSE_NO_PLAYER: return "no player";
    case GE_ORIGINAL_FIRST_PERSON_POSE_NO_MODEL: return "no model";
    case GE_ORIGINAL_FIRST_PERSON_POSE_ITEM_MISMATCH: return "item mismatch";
    case GE_ORIGINAL_FIRST_PERSON_POSE_NO_VIEW_TO_WORLD: return "no view-to-world";
    case GE_ORIGINAL_FIRST_PERSON_POSE_INVALID_VIEWPORT: return "invalid viewport";
    case GE_ORIGINAL_FIRST_PERSON_POSE_HIDDEN: return "hidden";
    default: return "unknown";
    }
}
