#ifndef GE_MP_STATE_GUNS_H
#define GE_MP_STATE_GUNS_H

#include "ge_mp_state.h"

struct player;
struct hand;
struct Model;
union ModelRwData;
struct modeldata_root;

#define GE_MP_GUN_KIND 0xe4U
#define GE_MP_GUN_SCHEMA_VERSION 1U
#define GE_MP_GUN_MAX_RW 32U
#define GE_MP_GUN_MAX_MATRICES 64U
#define GE_MP_GUN_SYSTEMS (GE_MP_SYSTEM_GUNS | GE_MP_SYSTEM_ANIMATION | GE_MP_SYSTEM_PROJECTILES)

/* Every non-overlay hand member, split into named primitive fields. */
#define GE_MP_GUN_HAND_FIELDS(X) \
    X(weaponnum, 0x100, I32, 1, weaponnum) \
    X(weaponnum_watchmenu, 0x101, I32, 1, weaponnum_watchmenu) \
    X(previous_weapon, 0x102, I32, 1, previous_weapon) \
    X(weapon_firing_status, 0x103, I8, 1, weapon_firing_status) \
    X(field_87D, 0x104, I8, 1, field_87D) \
    X(field_87E, 0x105, I8, 1, field_87E) \
    X(field_87F, 0x106, I8, 1, field_87F) \
    X(weapon_hold_time, 0x107, I32, 1, weapon_hold_time) \
    X(field_884, 0x108, I32, 1, field_884) \
    X(field_888, 0x109, I32, 1, field_888) \
    X(field_88C, 0x10a, I32, 1, field_88C) \
    X(field_890, 0x10b, I32, 1, field_890) \
    X(weapon_action_state, 0x10c, I32, 1, weapon_action_state) \
    X(weapon_current_animation, 0x10d, I32, 1, weapon_current_animation) \
    X(weapon_ammo_in_magazine, 0x10e, I32, 1, weapon_ammo_in_magazine) \
    X(field_8A0, 0x10f, I32, 1, field_8A0) \
    X(numvisibleshells, 0x110, I32, 1, numvisibleshells) \
    X(field_8A8, 0x111, I32, 1, field_8A8) \
    X(weapon_next_weapon, 0x112, I32, 1, weapon_next_weapon) \
    X(field_8B0, 0x113, I32, 1, field_8B0) \
    X(weapon_animation_trigger, 0x114, I32, 1, weapon_animation_trigger) \
    X(field_8B8, 0x115, I32, 1, field_8B8) \
    X(field_8BC, 0x116, F32, 1, field_8BC) \
    X(field_8C0, 0x117, F32, 1, field_8C0) \
    X(field_8C4, 0x118, F32, 1, field_8C4) \
    X(field_8C8, 0x119, F32, 1, field_8C8) \
    X(field_8CC, 0x11a, F32, 1, field_8CC) \
    X(field_8D0, 0x11b, F32, 1, field_8D0) \
    X(field_8D4, 0x11c, F32, 1, field_8D4) \
    X(field_8D8, 0x11d, F32, 1, field_8D8) \
    X(field_8DC, 0x11e, F32, 1, field_8DC) \
    X(field_8E0, 0x11f, F32, 1, field_8E0) \
    X(field_8E4, 0x120, F32, 1, field_8E4) \
    X(field_8E8, 0x121, F32, 1, field_8E8) \
    X(field_8EC, 0x122, F32, 16, field_8EC.m) \
    X(field_92C, 0x123, I32, 1, field_92C) \
    X(sway_pos_x, 0x124, F32, 1, sway_pos_x) \
    X(sway_pos_y, 0x125, F32, 1, sway_pos_y) \
    X(sway_pos_z, 0x126, F32, 1, sway_pos_z) \
    X(sway_look_x, 0x127, F32, 1, sway_look_x) \
    X(sway_look_y, 0x128, F32, 1, sway_look_y) \
    X(sway_look_z, 0x129, F32, 1, sway_look_z) \
    X(sway_up_x, 0x12a, F32, 1, sway_up_x) \
    X(sway_up_y, 0x12b, F32, 1, sway_up_y) \
    X(sway_up_z, 0x12c, F32, 1, sway_up_z) \
    X(spring_pos_x, 0x12d, F32, 1, spring_pos_x) \
    X(spring_pos_y, 0x12e, F32, 1, spring_pos_y) \
    X(spring_pos_z, 0x12f, F32, 1, spring_pos_z) \
    X(spring_look_x, 0x130, F32, 1, spring_look_x) \
    X(spring_look_y, 0x131, F32, 1, spring_look_y) \
    X(spring_look_z, 0x132, F32, 1, spring_look_z) \
    X(spring_up_x, 0x133, F32, 1, spring_up_x) \
    X(spring_up_y, 0x134, F32, 1, spring_up_y) \
    X(spring_up_z, 0x135, F32, 1, spring_up_z) \
    X(blendpos, 0x136, F32, 12, blendpos) \
    X(blendlook, 0x137, F32, 12, blendlook) \
    X(blendup, 0x138, F32, 12, blendup) \
    X(curblendpos, 0x139, I32, 1, curblendpos) \
    X(dampt, 0x13a, F32, 1, dampt) \
    X(blendscale, 0x13b, F32, 1, blendscale) \
    X(blendscale1, 0x13c, F32, 1, blendscale1) \
    X(sideflag, 0x13d, I32, 1, sideflag) \
    X(weapon_theta_displacement, 0x13e, F32, 1, weapon_theta_displacement) \
    X(weapon_verta_displacement, 0x13f, F32, 1, weapon_verta_displacement) \
    X(field_A24, 0x140, I32, 1, field_A24) \
    X(gunofs2_x, 0x141, F32, 1, gunofs2_x) \
    X(gunofs2_y, 0x142, F32, 1, gunofs2_y) \
    X(gunofs2_z, 0x143, F32, 1, gunofs2_z) \
    X(field_A34, 0x144, F32, 1, field_A34) \
    X(field_A38, 0x145, F32, 1, field_A38) \
    X(field_A3C, 0x146, F32, 1, field_A3C) \
    X(field_A40, 0x147, F32, 1, field_A40) \
    X(field_A48, 0x148, I32, 1, field_A48) \
    X(field_A4C, 0x149, I32, 1, field_A4C) \
    X(field_A50, 0x14a, I32, 1, field_A50) \
    X(beam_unk00, 0x14b, I8, 1, weapon_beam.unk00) \
    X(beam_item_id, 0x14c, I8, 1, weapon_beam.item_id) \
    X(beam_unk02, 0x14d, U8, 1, weapon_beam.unk02) \
    X(beam_unk03, 0x14e, U8, 1, weapon_beam.unk03) \
    X(beam_pos, 0x14f, F32, 3, weapon_beam.pos.f) \
    X(beam_delta, 0x150, F32, 3, weapon_beam.delta.f) \
    X(beam_unk1c, 0x151, F32, 1, weapon_beam.unk1c) \
    X(beam_unk20, 0x152, F32, 1, weapon_beam.unk20) \
    X(beam_unk24, 0x153, F32, 1, weapon_beam.unk24) \
    X(beam_unk28, 0x154, F32, 1, weapon_beam.unk28) \
    X(noise, 0x155, F32, 1, noise) \
    X(field_A84, 0x156, F32, 1, field_A84) \
    X(field_A88, 0x157, F32, 1, field_A88) \
    X(field_A8C, 0x158, I32, 1, field_A8C) \
    X(firedrocket, 0x159, I32, 1, firedrocket) \
    X(gunmtx_camspace, 0x15a, F32, 16, gunmtx_camspace.m) \
    X(throw_item_pos_related, 0x15b, F32, 16, throw_item_pos_related.m) \
    X(throw_item_pos_related_prev, 0x15c, F32, 16, throw_item_pos_related_prev.m) \
    X(field_B58, 0x15d, F32, 3, field_B58.f) \
    X(field_B64, 0x15e, F32, 1, field_B64) \
    X(volley, 0x15f, I32, 1, volley) \
    X(item_related, 0x160, F32, 3, item_related.f)

enum {
#define GE_MP_GUN_ID(name, id, type, count, path) GE_MP_GUN_FIELD_##name = id,
    GE_MP_GUN_HAND_FIELDS(GE_MP_GUN_ID)
#undef GE_MP_GUN_ID
};

#define GE_MP_GUN_META_FIELDS(X) \
    X(omissions, 512, U32, 1) \
    X(reference_presence, 513, U16, 1) \
    X(model_present, 514, U8, 1) \
    X(model_unk00, 515, I16, 1) \
    X(model_rwdatalen, 516, I16, 1) \
    X(model_scale, 517, F32, 1) \
    X(audio, 518, ACTOR_REF, 1) \
    X(rocket, 519, ACTOR_REF, 1) \
    X(hand_matrices, 520, ACTOR_REF, 1) \
    X(model_character, 521, ACTOR_REF, 1) \
    X(model_header, 522, U64, 1) \
    X(model_matrices, 523, ACTOR_REF, 1) \
    X(model_rw_arena, 524, ACTOR_REF, 1) \
    X(attached_model, 525, ACTOR_REF, 1) \
    X(attached_node, 526, U64, 1) \
    X(matrix_count, 527, U32, 1) \
    X(matrices, 528, F32, 1024) \
    X(rw_count, 529, U32, 1) \
    X(classified_rw_words, 530, U32, 1)

#define GE_MP_GUN_RW_FIELDS(X) \
    X(rw_tag, 768, U16, 1) \
    X(rw_word_index, 769, U16, 1) \
    X(rw_node, 770, U64, 1) \
    X(rw_reference_presence, 771, U8, 1) \
    X(header_unk00, 772, I8, 1) \
    X(header_unk01, 773, I8, 1) \
    X(header_unk02, 774, I8, 1) \
    X(header_ground, 775, F32, 1) \
    X(header_pos, 776, F32, 3) \
    X(header_unk14, 777, F32, 1) \
    X(header_unk18, 778, F32, 1) \
    X(header_unk1c, 779, F32, 1) \
    X(header_unk20, 780, F32, 1) \
    X(header_unk24, 781, F32, 3) \
    X(header_unk30, 782, F32, 1) \
    X(header_unk34, 783, F32, 3) \
    X(header_unk40, 784, F32, 3) \
    X(header_unk4c, 785, F32, 3) \
    X(header_unk58, 786, F32, 1) \
    X(header_unk5c, 787, F32, 1) \
    X(root_unk00, 788, U16, 1) \
    X(root_unk02, 789, U8, 1) \
    X(root_ground, 790, F32, 1) \
    X(root_pos, 791, F32, 3) \
    X(root_subroty, 792, F32, 1) \
    X(root_unk18, 793, F32, 1) \
    X(root_unk1c, 794, U32, 1) \
    X(root_unk20, 795, F32, 1) \
    X(root_unk24, 796, F32, 3) \
    X(root_unk30, 797, F32, 1) \
    X(root_unk34, 798, F32, 3) \
    X(root_unk40, 799, F32, 3) \
    X(root_unk4c, 800, F32, 3) \
    X(root_unk58, 801, F32, 1) \
    X(root_unk5c, 802, F32, 1) \
    X(displaylist_unk00, 803, I32, 1) \
    X(op07_index, 804, I32, 1) \
    X(lod_visible, 805, I32, 1) \
    X(bsp_visible, 806, I32, 1) \
    X(op11_unk00, 807, I16, 1) \
    X(op11_unk02, 808, I16, 1) \
    X(gunfire_visible, 809, I16, 1) \
    X(gunfire_unk02, 810, U16, 1) \
    X(switch_visible, 811, I32, 1) \
    X(head_header, 812, U64, 1) \
    X(head_rw, 813, ACTOR_REF, 1) \
    X(collision_vertices, 814, U64, 1) \
    X(collision_gdl, 815, U64, 1)

typedef enum GeMpGunRefKind {
    GE_MP_GUN_REF_AUDIO = 1, GE_MP_GUN_REF_ROCKET,
    GE_MP_GUN_REF_MATRIX_ARENA, GE_MP_GUN_REF_MODEL_CHARACTER,
    GE_MP_GUN_REF_MODEL_RW_ARENA, GE_MP_GUN_REF_ATTACHED_MODEL,
    GE_MP_GUN_REF_MODEL_HEADER, GE_MP_GUN_REF_MODEL_NODE,
    GE_MP_GUN_REF_RW_HEADER_ASSET, GE_MP_GUN_REF_RW_ARENA,
    GE_MP_GUN_REF_VERTICES, GE_MP_GUN_REF_DISPLAY_LIST
} GeMpGunRefKind;

/* Native opcode is explicit; opcode 1 has two named C interpretations.
 * ROOT selects modeldata_root (u16/byte/U32 flag fields), HEADER selects
 * ModelRwData_HeaderRecord (s8/F32). Unknown tags are never raw-word encoded. */
typedef enum GeMpGunRwTag {
    GE_MP_GUN_RW_HEADER = 1, GE_MP_GUN_RW_DISPLAY_LIST = 4,
    GE_MP_GUN_RW_OP07 = 7, GE_MP_GUN_RW_LOD = 8, GE_MP_GUN_RW_BSP = 9,
    GE_MP_GUN_RW_OP11 = 11, GE_MP_GUN_RW_GUNFIRE = 12,
    GE_MP_GUN_RW_SWITCH = 18, GE_MP_GUN_RW_HEAD = 23,
    GE_MP_GUN_RW_DLCOLLISION = 24, GE_MP_GUN_RW_ROOT = 0x101
} GeMpGunRwTag;

typedef struct GeMpGunRwView {
    uint16_t tag, word_index; /* original 32-bit RW word coordinate */
    uint64_t node_content_id; /* owner-validated immutable node identity */
    const union ModelRwData *data;
    const struct modeldata_root *root; /* ROOT tag only */
    size_t readable_bytes; /* validated native allocation span for chosen type */
} GeMpGunRwView;

typedef struct GeMpGunModelView {
    /* Must be actual embedded ARM prefix or the actual host per-player owner.
     * Only Model fields before anim are read; anim aliases RW words on ARM. */
    const struct Model *model;
    size_t model_prefix_bytes;
    const float *matrices; /* contiguous actual native matrix values */
    size_t matrix_count;
    size_t matrix_readable_bytes; /* owner-validated actual allocation span */
    const GeMpGunRwView *rw;
    size_t rw_count;
    uint32_t classified_rw_words; /* native owner attestation for unused words */
} GeMpGunModelView;

typedef struct GeMpGunResolvers {
    void *context;
    const GeMpRegistry *registry; /* validates supplied hand and dynamic IDs */
    /* Every non-null pointer is validated by its owning registry/content map.
     * Dynamic references return live generation-bearing actor IDs; static
     * content references return manifest-qualified stable IDs. Never addresses. */
    GeMpStateStatus (*actor)(void *context, GeMpGunRefKind kind,
        const void *native_pointer, uint64_t *actor_id);
    GeMpStateStatus (*content)(void *context, GeMpGunRefKind kind,
        const void *native_pointer, uint64_t *content_id);
    GeMpStateStatus (*model_view)(void *context, const struct player *player,
        uint32_t player_slot, uint32_t hand, GeMpGunModelView *view);
} GeMpGunResolvers;

/* Diagnostic omissions are explicit. Capture always reports OTHER_DOMAINS;
 * even fully classified hand records cannot certify a native simulation. */
#define GE_MP_GUN_OMIT_OTHER_DOMAINS 1U
#define GE_MP_GUN_OMIT_MODEL_VIEW 2U
#define GE_MP_GUN_OMIT_RW_LAYOUT 4U
#define GE_MP_GUN_OMIT_REFERENCE 8U
#define GE_MP_GUN_OMIT_STALE_REFERENCE 16U
#define GE_MP_GUN_OMIT_MATRIX_VIEW 32U
#define GE_MP_GUN_OMIT_RW_TYPE 64U
#define GE_MP_GUN_OMIT_CONTENT_ID 128U

enum {
#define GE_MP_GUN_EXTRA_ID(name, id, type, count) GE_MP_GUN_FIELD_##name = id,
    GE_MP_GUN_META_FIELDS(GE_MP_GUN_EXTRA_ID)
    GE_MP_GUN_RW_FIELDS(GE_MP_GUN_EXTRA_ID)
#undef GE_MP_GUN_EXTRA_ID
    GE_MP_GUN_FIELD_COUNT = 0
#define GE_MP_GUN_COUNT_HAND(name, id, type, count, path) + 1
    GE_MP_GUN_HAND_FIELDS(GE_MP_GUN_COUNT_HAND)
#undef GE_MP_GUN_COUNT_HAND
#define GE_MP_GUN_COUNT_EXTRA(name, id, type, count) + 1
    GE_MP_GUN_META_FIELDS(GE_MP_GUN_COUNT_EXTRA)
    GE_MP_GUN_RW_FIELDS(GE_MP_GUN_COUNT_EXTRA)
#undef GE_MP_GUN_COUNT_EXTRA
};

typedef struct GeMpGunValues {
    int32_t weaponnum[1];
    int32_t weaponnum_watchmenu[1];
    int32_t previous_weapon[1];
    int8_t weapon_firing_status[1];
    int8_t field_87D[1];
    int8_t field_87E[1];
    int8_t field_87F[1];
    int32_t weapon_hold_time[1];
    int32_t field_884[1];
    int32_t field_888[1];
    int32_t field_88C[1];
    int32_t field_890[1];
    int32_t weapon_action_state[1];
    int32_t weapon_current_animation[1];
    int32_t weapon_ammo_in_magazine[1];
    int32_t field_8A0[1];
    int32_t numvisibleshells[1];
    int32_t field_8A8[1];
    int32_t weapon_next_weapon[1];
    int32_t field_8B0[1];
    int32_t weapon_animation_trigger[1];
    int32_t field_8B8[1];
    float field_8BC[1];
    float field_8C0[1];
    float field_8C4[1];
    float field_8C8[1];
    float field_8CC[1];
    float field_8D0[1];
    float field_8D4[1];
    float field_8D8[1];
    float field_8DC[1];
    float field_8E0[1];
    float field_8E4[1];
    float field_8E8[1];
    float field_8EC[16];
    int32_t field_92C[1];
    float sway_pos_x[1];
    float sway_pos_y[1];
    float sway_pos_z[1];
    float sway_look_x[1];
    float sway_look_y[1];
    float sway_look_z[1];
    float sway_up_x[1];
    float sway_up_y[1];
    float sway_up_z[1];
    float spring_pos_x[1];
    float spring_pos_y[1];
    float spring_pos_z[1];
    float spring_look_x[1];
    float spring_look_y[1];
    float spring_look_z[1];
    float spring_up_x[1];
    float spring_up_y[1];
    float spring_up_z[1];
    float blendpos[12];
    float blendlook[12];
    float blendup[12];
    int32_t curblendpos[1];
    float dampt[1];
    float blendscale[1];
    float blendscale1[1];
    int32_t sideflag[1];
    float weapon_theta_displacement[1];
    float weapon_verta_displacement[1];
    int32_t field_A24[1];
    float gunofs2_x[1];
    float gunofs2_y[1];
    float gunofs2_z[1];
    float field_A34[1];
    float field_A38[1];
    float field_A3C[1];
    float field_A40[1];
    int32_t field_A48[1];
    int32_t field_A4C[1];
    int32_t field_A50[1];
    int8_t beam_unk00[1];
    int8_t beam_item_id[1];
    uint8_t beam_unk02[1];
    uint8_t beam_unk03[1];
    float beam_pos[3];
    float beam_delta[3];
    float beam_unk1c[1];
    float beam_unk20[1];
    float beam_unk24[1];
    float beam_unk28[1];
    float noise[1];
    float field_A84[1];
    float field_A88[1];
    int32_t field_A8C[1];
    int32_t firedrocket[1];
    float gunmtx_camspace[16];
    float throw_item_pos_related[16];
    float throw_item_pos_related_prev[16];
    float field_B58[3];
    float field_B64[1];
    int32_t volley[1];
    float item_related[3];
    uint32_t omissions[1];
    uint16_t reference_presence[1];
    uint8_t model_present[1];
    int16_t model_unk00[1];
    int16_t model_rwdatalen[1];
    float model_scale[1];
    uint64_t audio[1];
    uint64_t rocket[1];
    uint64_t hand_matrices[1];
    uint64_t model_character[1];
    uint64_t model_header[1];
    uint64_t model_matrices[1];
    uint64_t model_rw_arena[1];
    uint64_t attached_model[1];
    uint64_t attached_node[1];
    uint32_t matrix_count[1];
    float matrices[1024];
    uint32_t rw_count[1];
    uint32_t classified_rw_words[1];
    uint16_t rw_tag[GE_MP_GUN_MAX_RW * 1];
    uint16_t rw_word_index[GE_MP_GUN_MAX_RW * 1];
    uint64_t rw_node[GE_MP_GUN_MAX_RW * 1];
    uint8_t rw_reference_presence[GE_MP_GUN_MAX_RW * 1];
    int8_t header_unk00[GE_MP_GUN_MAX_RW * 1];
    int8_t header_unk01[GE_MP_GUN_MAX_RW * 1];
    int8_t header_unk02[GE_MP_GUN_MAX_RW * 1];
    float header_ground[GE_MP_GUN_MAX_RW * 1];
    float header_pos[GE_MP_GUN_MAX_RW * 3];
    float header_unk14[GE_MP_GUN_MAX_RW * 1];
    float header_unk18[GE_MP_GUN_MAX_RW * 1];
    float header_unk1c[GE_MP_GUN_MAX_RW * 1];
    float header_unk20[GE_MP_GUN_MAX_RW * 1];
    float header_unk24[GE_MP_GUN_MAX_RW * 3];
    float header_unk30[GE_MP_GUN_MAX_RW * 1];
    float header_unk34[GE_MP_GUN_MAX_RW * 3];
    float header_unk40[GE_MP_GUN_MAX_RW * 3];
    float header_unk4c[GE_MP_GUN_MAX_RW * 3];
    float header_unk58[GE_MP_GUN_MAX_RW * 1];
    float header_unk5c[GE_MP_GUN_MAX_RW * 1];
    uint16_t root_unk00[GE_MP_GUN_MAX_RW * 1];
    uint8_t root_unk02[GE_MP_GUN_MAX_RW * 1];
    float root_ground[GE_MP_GUN_MAX_RW * 1];
    float root_pos[GE_MP_GUN_MAX_RW * 3];
    float root_subroty[GE_MP_GUN_MAX_RW * 1];
    float root_unk18[GE_MP_GUN_MAX_RW * 1];
    uint32_t root_unk1c[GE_MP_GUN_MAX_RW * 1];
    float root_unk20[GE_MP_GUN_MAX_RW * 1];
    float root_unk24[GE_MP_GUN_MAX_RW * 3];
    float root_unk30[GE_MP_GUN_MAX_RW * 1];
    float root_unk34[GE_MP_GUN_MAX_RW * 3];
    float root_unk40[GE_MP_GUN_MAX_RW * 3];
    float root_unk4c[GE_MP_GUN_MAX_RW * 3];
    float root_unk58[GE_MP_GUN_MAX_RW * 1];
    float root_unk5c[GE_MP_GUN_MAX_RW * 1];
    int32_t displaylist_unk00[GE_MP_GUN_MAX_RW * 1];
    int32_t op07_index[GE_MP_GUN_MAX_RW * 1];
    int32_t lod_visible[GE_MP_GUN_MAX_RW * 1];
    int32_t bsp_visible[GE_MP_GUN_MAX_RW * 1];
    int16_t op11_unk00[GE_MP_GUN_MAX_RW * 1];
    int16_t op11_unk02[GE_MP_GUN_MAX_RW * 1];
    int16_t gunfire_visible[GE_MP_GUN_MAX_RW * 1];
    uint16_t gunfire_unk02[GE_MP_GUN_MAX_RW * 1];
    int32_t switch_visible[GE_MP_GUN_MAX_RW * 1];
    uint64_t head_header[GE_MP_GUN_MAX_RW * 1];
    uint64_t head_rw[GE_MP_GUN_MAX_RW * 1];
    uint64_t collision_vertices[GE_MP_GUN_MAX_RW * 1];
    uint64_t collision_gdl[GE_MP_GUN_MAX_RW * 1];
} GeMpGunValues;

typedef struct GeMpGunsState {
    GeMpGunValues values[2];
    GeMpStateField fields[2][GE_MP_GUN_FIELD_COUNT];
    GeMpStateRecord records[2];
    size_t record_count;
    GeMpStateCoverage coverage;
} GeMpGunsState;

const GeMpRecordSchema *ge_mp_state_guns_schema(void);
/* Captures two hands for any physical player row 0..3, including inactive
 * allocated rows supplied by the storage owner. Outside-hand player fields
 * belong to other domains. Caller validates player-storage lifetime and freezes native owners/resolvers
 * throughout capture. No selection, reset or registration hooks occur here.
 * Resolvers must be read-only. A null/failed model view is an omission, never
 * an attestation that its overlay is irrelevant. Matrix count/span and RW
 * types/word positions are owner-validated, not inferred from pointer bits.
 * INVALID/STALE hand arguments leave output unchanged. Successful capture
 * always returns INCOMPLETE, retaining typed diagnostic records and coverage.
 * Keep output in place: record/field pointers refer into caller-owned state. */
GeMpStateStatus ge_mp_state_guns_capture(GeMpGunsState *state,
    const struct player *player, uint32_t player_slot,
    const uint64_t hand_actor_ids[2], const GeMpGunResolvers *resolvers);

#endif
