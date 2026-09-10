#include "ge_mp_state_guns.h"
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"

_Static_assert(sizeof(float) == 4, "gun ABI requires binary32");
_Static_assert(sizeof(ITEM_IDS) <= 4 && (ITEM_IDS)-1 < 0, "signed ITEM_IDS ABI");
_Static_assert(sizeof(((union ModelRwData *)0)->LOD.visible) == 4, "native bool ABI");
#if UINTPTR_MAX == UINT32_MAX
_Static_assert(offsetof(Model, anim) == 0x20, "embedded model prefix ABI");
_Static_assert(offsetof(struct hand, modeldatas) - offsetof(struct hand, field_B68) == 0x20,
    "embedded model/RW boundary");
_Static_assert(offsetof(struct hand, field_C04) - offsetof(struct hand, modeldatas) == 31 * 4,
    "embedded RW capacity");
#endif

static const GeMpFieldSchema gun_fields[] = {
#define HAND_SCHEMA(name, id, type, count, path) {id, GE_MP_STATE_##type, 0, count, count},
    GE_MP_GUN_HAND_FIELDS(HAND_SCHEMA)
#undef HAND_SCHEMA
#define META_SCHEMA(name, id, type, count) {id, GE_MP_STATE_##type, \
    GE_MP_STATE_##type == GE_MP_STATE_ACTOR_REF ? GE_MP_FIELD_NULLABLE : 0, \
    id == GE_MP_GUN_FIELD_matrices ? 0 : count, count},
    GE_MP_GUN_META_FIELDS(META_SCHEMA)
#undef META_SCHEMA
#define RW_SCHEMA(name, id, type, count) {id, GE_MP_STATE_##type, \
    GE_MP_STATE_##type == GE_MP_STATE_ACTOR_REF ? GE_MP_FIELD_NULLABLE : 0, 0, GE_MP_GUN_MAX_RW * count},
    GE_MP_GUN_RW_FIELDS(RW_SCHEMA)
#undef RW_SCHEMA
};
static const GeMpRecordSchema gun_schema = {
    GE_MP_GUN_KIND, GE_MP_GUN_SYSTEMS, gun_fields, GE_MP_GUN_FIELD_COUNT
};
const GeMpRecordSchema *ge_mp_state_guns_schema(void) { return &gun_schema; }

static void omit(GeMpGunsState *state, GeMpGunValues *v, uint32_t bit)
{
    v->omissions[0] |= bit;
    ++state->coverage.unclassified_fields;
}
static uint64_t reference(GeMpGunsState *state, GeMpGunValues *v,
    const GeMpGunResolvers *r, GeMpGunRefKind kind, const void *pointer, int actor)
{
    GeMpStateStatus status;
    uint64_t id = 0;
    if (!pointer) return 0;
    status = actor ? (r->actor ? r->actor(r->context, kind, pointer, &id) : GE_MP_STATE_INCOMPLETE)
                   : (r->content ? r->content(r->context, kind, pointer, &id) : GE_MP_STATE_INCOMPLETE);
    if (status == GE_MP_STATE_OK && id && actor)
        status = ge_mp_registry_resolve(r->registry, id, NULL);
    if (status != GE_MP_STATE_OK || !id) {
        v->omissions[0] |= GE_MP_GUN_OMIT_REFERENCE;
        if (status == GE_MP_STATE_STALE) v->omissions[0] |= GE_MP_GUN_OMIT_STALE_REFERENCE;
        ++state->coverage.unresolved_references;
        return 0;
    }
    return id;
}

static void capture_hand(GeMpGunValues *v, const struct hand *hand)
{
    size_t i;
    v->weaponnum[0] = hand->weaponnum;
    v->weaponnum_watchmenu[0] = hand->weaponnum_watchmenu;
    v->previous_weapon[0] = hand->previous_weapon;
    v->weapon_firing_status[0] = hand->weapon_firing_status;
    v->field_87D[0] = hand->field_87D;
    v->field_87E[0] = hand->field_87E;
    v->field_87F[0] = hand->field_87F;
    v->weapon_hold_time[0] = hand->weapon_hold_time;
    v->field_884[0] = hand->field_884;
    v->field_888[0] = hand->field_888;
    v->field_88C[0] = hand->field_88C;
    v->field_890[0] = hand->field_890;
    v->weapon_action_state[0] = hand->weapon_action_state;
    v->weapon_current_animation[0] = hand->weapon_current_animation;
    v->weapon_ammo_in_magazine[0] = hand->weapon_ammo_in_magazine;
    v->field_8A0[0] = hand->field_8A0;
    v->numvisibleshells[0] = hand->numvisibleshells;
    v->field_8A8[0] = hand->field_8A8;
    v->weapon_next_weapon[0] = hand->weapon_next_weapon;
    v->field_8B0[0] = hand->field_8B0;
    v->weapon_animation_trigger[0] = hand->weapon_animation_trigger;
    v->field_8B8[0] = hand->field_8B8;
    v->field_8BC[0] = hand->field_8BC;
    v->field_8C0[0] = hand->field_8C0;
    v->field_8C4[0] = hand->field_8C4;
    v->field_8C8[0] = hand->field_8C8;
    v->field_8CC[0] = hand->field_8CC;
    v->field_8D0[0] = hand->field_8D0;
    v->field_8D4[0] = hand->field_8D4;
    v->field_8D8[0] = hand->field_8D8;
    v->field_8DC[0] = hand->field_8DC;
    v->field_8E0[0] = hand->field_8E0;
    v->field_8E4[0] = hand->field_8E4;
    v->field_8E8[0] = hand->field_8E8;
    for (i = 0; i < 16; ++i) v->field_8EC[i] = hand->field_8EC.m[i / 4][i % 4];
    v->field_92C[0] = hand->field_92C;
    v->sway_pos_x[0] = hand->sway_pos_x;
    v->sway_pos_y[0] = hand->sway_pos_y;
    v->sway_pos_z[0] = hand->sway_pos_z;
    v->sway_look_x[0] = hand->sway_look_x;
    v->sway_look_y[0] = hand->sway_look_y;
    v->sway_look_z[0] = hand->sway_look_z;
    v->sway_up_x[0] = hand->sway_up_x;
    v->sway_up_y[0] = hand->sway_up_y;
    v->sway_up_z[0] = hand->sway_up_z;
    v->spring_pos_x[0] = hand->spring_pos_x;
    v->spring_pos_y[0] = hand->spring_pos_y;
    v->spring_pos_z[0] = hand->spring_pos_z;
    v->spring_look_x[0] = hand->spring_look_x;
    v->spring_look_y[0] = hand->spring_look_y;
    v->spring_look_z[0] = hand->spring_look_z;
    v->spring_up_x[0] = hand->spring_up_x;
    v->spring_up_y[0] = hand->spring_up_y;
    v->spring_up_z[0] = hand->spring_up_z;
    for (i = 0; i < 12; ++i) v->blendpos[i] = hand->blendpos[i / 3].f[i % 3];
    for (i = 0; i < 12; ++i) v->blendlook[i] = hand->blendlook[i / 3].f[i % 3];
    for (i = 0; i < 12; ++i) v->blendup[i] = hand->blendup[i / 3].f[i % 3];
    v->curblendpos[0] = hand->curblendpos;
    v->dampt[0] = hand->dampt;
    v->blendscale[0] = hand->blendscale;
    v->blendscale1[0] = hand->blendscale1;
    v->sideflag[0] = hand->sideflag;
    v->weapon_theta_displacement[0] = hand->weapon_theta_displacement;
    v->weapon_verta_displacement[0] = hand->weapon_verta_displacement;
    v->field_A24[0] = hand->field_A24;
    v->gunofs2_x[0] = hand->gunofs2_x;
    v->gunofs2_y[0] = hand->gunofs2_y;
    v->gunofs2_z[0] = hand->gunofs2_z;
    v->field_A34[0] = hand->field_A34;
    v->field_A38[0] = hand->field_A38;
    v->field_A3C[0] = hand->field_A3C;
    v->field_A40[0] = hand->field_A40;
    v->field_A48[0] = hand->field_A48;
    v->field_A4C[0] = hand->field_A4C;
    v->field_A50[0] = hand->field_A50;
    v->beam_unk00[0] = hand->weapon_beam.unk00;
    v->beam_item_id[0] = hand->weapon_beam.item_id;
    v->beam_unk02[0] = hand->weapon_beam.unk02;
    v->beam_unk03[0] = hand->weapon_beam.unk03;
    for (i = 0; i < 3; ++i) v->beam_pos[i] = hand->weapon_beam.pos.f[i];
    for (i = 0; i < 3; ++i) v->beam_delta[i] = hand->weapon_beam.delta.f[i];
    v->beam_unk1c[0] = hand->weapon_beam.unk1c;
    v->beam_unk20[0] = hand->weapon_beam.unk20;
    v->beam_unk24[0] = hand->weapon_beam.unk24;
    v->beam_unk28[0] = hand->weapon_beam.unk28;
    v->noise[0] = hand->noise;
    v->field_A84[0] = hand->field_A84;
    v->field_A88[0] = hand->field_A88;
    v->field_A8C[0] = hand->field_A8C;
    v->firedrocket[0] = hand->firedrocket;
    for (i = 0; i < 16; ++i) v->gunmtx_camspace[i] = hand->gunmtx_camspace.m[i / 4][i % 4];
    for (i = 0; i < 16; ++i) v->throw_item_pos_related[i] = hand->throw_item_pos_related.m[i / 4][i % 4];
    for (i = 0; i < 16; ++i) v->throw_item_pos_related_prev[i] = hand->throw_item_pos_related_prev.m[i / 4][i % 4];
    for (i = 0; i < 3; ++i) v->field_B58[i] = hand->field_B58.f[i];
    v->field_B64[0] = hand->field_B64;
    v->volley[0] = hand->volley;
    for (i = 0; i < 3; ++i) v->item_related[i] = hand->item_related.f[i];
}

/* Spans use original 32-bit RW word coordinates, while readable_bytes uses
 * the actual compilation ABI. Pointer-bearing host unions are larger. */
static size_t rw_size(uint16_t tag, unsigned *words)
{
    *words = 1;
    switch (tag) {
    case GE_MP_GUN_RW_HEADER: *words = 24; return sizeof(((union ModelRwData *)0)->Header);
    case GE_MP_GUN_RW_DISPLAY_LIST: *words = 1; return sizeof(((union ModelRwData *)0)->DisplayList);
    case GE_MP_GUN_RW_OP07: *words = 1; return sizeof(((union ModelRwData *)0)->Op07);
    case GE_MP_GUN_RW_LOD: *words = 1; return sizeof(((union ModelRwData *)0)->LOD);
    case GE_MP_GUN_RW_BSP: *words = 1; return sizeof(((union ModelRwData *)0)->BSP);
    case GE_MP_GUN_RW_OP11: *words = 1; return sizeof(((union ModelRwData *)0)->Op11);
    case GE_MP_GUN_RW_GUNFIRE: *words = 1; return sizeof(((union ModelRwData *)0)->Gunfire);
    case GE_MP_GUN_RW_SWITCH: *words = 1; return sizeof(((union ModelRwData *)0)->Switch);
    case GE_MP_GUN_RW_HEAD: *words = 2; return sizeof(((union ModelRwData *)0)->HeadPlaceholder);
    case GE_MP_GUN_RW_DLCOLLISION: *words = 2; return sizeof(((union ModelRwData *)0)->DisplayListCollisions);
    case GE_MP_GUN_RW_ROOT: *words = 24; return sizeof(struct modeldata_root);
    default: return 0;
    }
}

static void capture_rw(GeMpGunsState *state, GeMpGunValues *v,
    const GeMpGunModelView *view, const GeMpGunResolvers *r)
{
    uint32_t used = 0;
    size_t i, j, k;
    v->classified_rw_words[0] = view->classified_rw_words;
    if (view->rw_count > GE_MP_GUN_MAX_RW || (view->rw_count && !view->rw)) {
        omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT); return;
    }
    v->rw_count[0] = (uint32_t)view->rw_count;
    for (i = 0; i < view->rw_count; ++i) {
        const GeMpGunRwView *w = &view->rw[i];
        unsigned words;
        size_t bytes = rw_size(w->tag, &words);
        uint32_t mask;
        v->rw_tag[i] = w->tag;
        v->rw_word_index[i] = w->word_index;
        v->rw_node[i] = w->node_content_id;
        if (!w->node_content_id) omit(state, v, GE_MP_GUN_OMIT_CONTENT_ID);
        for (k = 0; k < i; ++k) if (w->node_content_id && w->node_content_id == view->rw[k].node_content_id)
            omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT);
        if (!bytes) { omit(state, v, GE_MP_GUN_OMIT_RW_TYPE); continue; }
        if (w->word_index >= 32 || words > 32U - w->word_index) {
            omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT); continue;
        }
        mask = ((UINT32_C(1) << words) - 1) << w->word_index;
        if (used & mask) { omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT); continue; }
        used |= mask;
        if (w->readable_bytes < bytes ||
            (w->tag == GE_MP_GUN_RW_ROOT ? !w->root : !w->data)) {
            omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT); continue;
        }
        switch (w->tag) {
        case GE_MP_GUN_RW_HEADER:
            v->header_unk00[i] = w->data->Header.unk00;
            v->header_unk01[i] = w->data->Header.unk01;
            v->header_unk02[i] = w->data->Header.unk02;
            v->header_ground[i] = w->data->Header.ground;
            for (j = 0; j < 3; ++j) v->header_pos[i * 3 + j] = w->data->Header.pos.f[j];
            v->header_unk14[i] = w->data->Header.unk14;
            v->header_unk18[i] = w->data->Header.unk18;
            v->header_unk1c[i] = w->data->Header.unk1c;
            v->header_unk20[i] = w->data->Header.unk20;
            for (j = 0; j < 3; ++j) v->header_unk24[i * 3 + j] = w->data->Header.unk24.f[j];
            v->header_unk30[i] = w->data->Header.unk30;
            for (j = 0; j < 3; ++j) v->header_unk34[i * 3 + j] = w->data->Header.unk34.f[j];
            for (j = 0; j < 3; ++j) v->header_unk40[i * 3 + j] = w->data->Header.unk40.f[j];
            for (j = 0; j < 3; ++j) v->header_unk4c[i * 3 + j] = w->data->Header.unk4c.f[j];
            v->header_unk58[i] = w->data->Header.unk58;
            v->header_unk5c[i] = w->data->Header.unk5c;
            break;
        case GE_MP_GUN_RW_DISPLAY_LIST:
            v->displaylist_unk00[i] = w->data->DisplayList.unk00;
            break;
        case GE_MP_GUN_RW_OP07:
            v->op07_index[i] = w->data->Op07.index;
            break;
        case GE_MP_GUN_RW_LOD:
            v->lod_visible[i] = w->data->LOD.visible;
            break;
        case GE_MP_GUN_RW_BSP:
            v->bsp_visible[i] = w->data->BSP.visible;
            break;
        case GE_MP_GUN_RW_OP11:
            v->op11_unk00[i] = w->data->Op11.unk00;
            v->op11_unk02[i] = w->data->Op11.unk02;
            break;
        case GE_MP_GUN_RW_GUNFIRE:
            v->gunfire_visible[i] = w->data->Gunfire.visible;
            v->gunfire_unk02[i] = w->data->Gunfire.unk02;
            break;
        case GE_MP_GUN_RW_SWITCH:
            v->switch_visible[i] = w->data->Switch.visible;
            break;
        case GE_MP_GUN_RW_HEAD:
            if (w->data->HeadPlaceholder.ModelFileHeader) v->rw_reference_presence[i] |= 1;
            v->head_header[i] = reference(state, v, r, GE_MP_GUN_REF_RW_HEADER_ASSET, w->data->HeadPlaceholder.ModelFileHeader, 0);
            if (w->data->HeadPlaceholder.RwDatas) v->rw_reference_presence[i] |= 2;
            v->head_rw[i] = reference(state, v, r, GE_MP_GUN_REF_RW_ARENA, w->data->HeadPlaceholder.RwDatas, 1);
            break;
        case GE_MP_GUN_RW_DLCOLLISION:
            if (w->data->DisplayListCollisions.Vertices) v->rw_reference_presence[i] |= 4;
            v->collision_vertices[i] = reference(state, v, r, GE_MP_GUN_REF_VERTICES, w->data->DisplayListCollisions.Vertices, 0);
            if (w->data->DisplayListCollisions.gdl) v->rw_reference_presence[i] |= 8;
            v->collision_gdl[i] = reference(state, v, r, GE_MP_GUN_REF_DISPLAY_LIST, w->data->DisplayListCollisions.gdl, 0);
            break;
        case GE_MP_GUN_RW_ROOT:
            v->root_unk00[i] = w->root->unk00;
            v->root_unk02[i] = w->root->unk02;
            v->root_ground[i] = w->root->ground;
            for (j = 0; j < 3; ++j) v->root_pos[i * 3 + j] = w->root->pos.f[j];
            v->root_subroty[i] = w->root->subroty;
            v->root_unk18[i] = w->root->unk18;
            v->root_unk1c[i] = w->root->unk1c;
            v->root_unk20[i] = w->root->unk20;
            for (j = 0; j < 3; ++j) v->root_unk24[i * 3 + j] = w->root->unk24.f[j];
            v->root_unk30[i] = w->root->unk30;
            for (j = 0; j < 3; ++j) v->root_unk34[i * 3 + j] = w->root->unk34.f[j];
            for (j = 0; j < 3; ++j) v->root_unk40[i * 3 + j] = w->root->unk40.f[j];
            for (j = 0; j < 3; ++j) v->root_unk4c[i * 3 + j] = w->root->unk4c.f[j];
            v->root_unk58[i] = w->root->unk58;
            v->root_unk5c[i] = w->root->unk5c;
            break;
        default: break;
        }
    }
    if ((used | view->classified_rw_words) != UINT32_MAX || (used & view->classified_rw_words))
        omit(state, v, GE_MP_GUN_OMIT_RW_LAYOUT);
}

static void capture_model(GeMpGunsState *state, GeMpGunValues *v,
    const struct player *player, uint32_t slot, uint32_t handnum, const GeMpGunResolvers *r)
{
    const struct hand *hand = &player->hands[handnum];
    GeMpGunModelView view = {0};
    const Model *model;
    size_t i;
    if (hand->audioHandle) v->reference_presence[0] |= 1;
    v->audio[0] = reference(state, v, r, GE_MP_GUN_REF_AUDIO, hand->audioHandle, 1);
    if (hand->rocket) v->reference_presence[0] |= 2;
    v->rocket[0] = reference(state, v, r, GE_MP_GUN_REF_ROCKET, hand->rocket, 1);
    if (hand->mtxlist) v->reference_presence[0] |= 4;
    v->hand_matrices[0] = reference(state, v, r, GE_MP_GUN_REF_MATRIX_ARENA, hand->mtxlist, 1);
    if (!r->model_view || r->model_view(r->context, player, slot, handnum, &view) != GE_MP_STATE_OK ||
        !view.model || view.model_prefix_bytes < offsetof(Model, anim)) {
        omit(state, v, GE_MP_GUN_OMIT_MODEL_VIEW); return;
    }
    model = view.model;
    v->model_present[0] = 1;
    v->model_unk00[0] = model->unk00;
    v->model_rwdatalen[0] = model->rwdatalen;
    v->model_scale[0] = model->scale;
    if (model->chr) v->reference_presence[0] |= 8;
    v->model_character[0] = reference(state, v, r, GE_MP_GUN_REF_MODEL_CHARACTER, model->chr, 1);
    if (model->obj) v->reference_presence[0] |= 16;
    v->model_header[0] = reference(state, v, r, GE_MP_GUN_REF_MODEL_HEADER, model->obj, 0);
    if (model->render_pos) v->reference_presence[0] |= 32;
    v->model_matrices[0] = reference(state, v, r, GE_MP_GUN_REF_MATRIX_ARENA, model->render_pos, 1);
    if (model->datas) v->reference_presence[0] |= 64;
    v->model_rw_arena[0] = reference(state, v, r, GE_MP_GUN_REF_MODEL_RW_ARENA, model->datas, 1);
    if (model->attachedto) v->reference_presence[0] |= 128;
    v->attached_model[0] = reference(state, v, r, GE_MP_GUN_REF_ATTACHED_MODEL, model->attachedto, 1);
    if (model->attachedto_objinst) v->reference_presence[0] |= 256;
    v->attached_node[0] = reference(state, v, r, GE_MP_GUN_REF_MODEL_NODE, model->attachedto_objinst, 0);
    if (view.matrix_count > GE_MP_GUN_MAX_MATRICES ||
        view.matrix_readable_bytes < view.matrix_count * 16U * sizeof(float) ||
        (view.matrix_count && (!view.matrices || !hand->mtxlist || !model->render_pos)) ||
        (view.matrices != (const float *)hand->mtxlist) ||
        (view.matrices != (const float *)model->render_pos)) {
        omit(state, v, GE_MP_GUN_OMIT_MATRIX_VIEW);
    } else {
        v->matrix_count[0] = (uint32_t)view.matrix_count;
        for (i = 0; i < view.matrix_count * 16; ++i)
            memcpy(&v->matrices[i], (const unsigned char *)view.matrices + i * sizeof(float), sizeof(float));
    }
    capture_rw(state, v, &view, r);
}

GeMpStateStatus ge_mp_state_guns_capture(GeMpGunsState *state,
    const struct player *player, uint32_t slot,
    const uint64_t ids[2], const GeMpGunResolvers *r)
{
    size_t h;
    if (!state || !player || slot >= 4 || !ids || !r || !r->registry) return GE_MP_STATE_INVALID;
    for (h = 0; h < 2; ++h) {
        GeMpStateStatus status;
        if ((uint32_t)(ids[h] >> 32) != ge_mp_registry_key(GE_MP_GUN_KIND, (uint8_t)slot, (uint16_t)h) ||
            !(uint32_t)ids[h]) return GE_MP_STATE_INVALID;
        status = ge_mp_registry_resolve(r->registry, ids[h], NULL);
        if (status != GE_MP_STATE_OK) return status;
    }
    memset(state, 0, sizeof(*state));
    state->coverage.enumerated_systems = GE_MP_GUN_SYSTEMS;
    for (h = 0; h < 2; ++h) {
        GeMpGunValues *v = &state->values[h];
        size_t f = 0;
        capture_hand(v, &player->hands[h]);
        omit(state, v, GE_MP_GUN_OMIT_OTHER_DOMAINS);
        capture_model(state, v, player, slot, (uint32_t)h, r);
#define HAND_BIND(name, id, type, n, path) \
        state->fields[h][f++] = (GeMpStateField){id, GE_MP_STATE_##type, n, v->name};
        GE_MP_GUN_HAND_FIELDS(HAND_BIND)
#undef HAND_BIND
#define META_BIND(name, id, type, n) \
        state->fields[h][f++] = (GeMpStateField){id, GE_MP_STATE_##type, \
            id == GE_MP_GUN_FIELD_matrices ? v->matrix_count[0] * 16U : n, v->name};
        GE_MP_GUN_META_FIELDS(META_BIND)
#undef META_BIND
#define RW_BIND(name, id, type, n) \
        state->fields[h][f++] = (GeMpStateField){id, GE_MP_STATE_##type, v->rw_count[0] * n, v->name};
        GE_MP_GUN_RW_FIELDS(RW_BIND)
#undef RW_BIND
        state->records[h] = (GeMpStateRecord){ids[h], GE_MP_GUN_KIND, state->fields[h], f};
    }
    state->record_count = 2;
    return GE_MP_STATE_INCOMPLETE;
}
