#include "ge_original_pp7_fire.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_prop_state.h"
#include "ge_original_pitem_models.h"
#include "ge_asset_pack.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/bondview.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/explosion.h"
#include "game/image_bank.h"
#ifdef AIPARSE
#define IMAGE(NAME, SZ, HS, HT, F3, F4, F5, F6) IMAGE_##NAME,
enum {
#include "assets/images.def"
};
#undef IMAGE
#endif

extern void objHit(ShotData *shotdata, BulletHit *hit);
extern s32 objGetDestroyedLevel(ObjectRecord *obj);
extern f32 gunItemGetDestructionAmount(ITEM_IDS item);
extern struct player_data *g_playerPerm;
extern bool propobjFindHit(Model *model, ModelNode *startNode,
    coord3d *ray_pos, coord3d *ray_dir, HitThing *hitthing,
    s32 *matrix_index, ModelNode **hit_node);

static int pitem_hit_ready(void *context, const void *model_instance)
{
    return ge_original_pitem_model_hit_ready(context, model_instance);
}

static void test_native_alarm_ray(const char *pack_path)
{
    GeAssetPack pack;
    GeOriginalPitemModelProvider *provider;
    GeOriginalPitemModelStatus status;
    GeOriginalPropState prop_state;
    ObjectRecord alarm;
    PropRecord *alarm_prop;
    ModelFileHeader *header = NULL;
    Model *model = NULL;
    float scale = 0.0f;
    float identity[4][4] = {{0}};
    int hit = 0;
    int xi;
    int yi;
    assert(ge_asset_pack_open(&pack, pack_path) == GE_ASSET_PACK_OK);
    provider = ge_original_pitem_model_provider_create(&pack, 1U, 1U,
                                                       &status);
    assert(provider != NULL && status == GE_ORIGINAL_PITEM_MODEL_OK);
    assert(ge_original_pitem_model_resolve_instance(provider, PROP_ALARM1,
        (void **)&header, (void **)&model, &scale));
    assert(header != NULL && model != NULL && scale > 0.0f);
    assert(ge_original_pitem_model_hit_ready(provider, model));
    memset(&alarm, 0, sizeof(alarm));
    assert(ge_original_prop_state_reset(&prop_state, 137U));
    alarm_prop = ge_original_prop_state_allocate(&prop_state, &alarm);
    assert(alarm_prop != NULL);
    ((PropDefHeaderRecord *)&alarm)->type = PROPDEF_ALARM;
    alarm.model = model;
    alarm.prop = alarm_prop;
    alarm.mtx.m[0][0] = alarm.mtx.m[1][1] = 1.0f;
    alarm.mtx.m[2][2] = alarm.mtx.m[3][3] = 1.0f;
    alarm.runtime_pos.x = 125.0f;
    alarm.runtime_pos.y = -50.0f;
    alarm.runtime_pos.z = -900.0f;
    alarm_prop->type = PROP_TYPE_OBJ;
    alarm_prop->obj = &alarm;
    alarm_prop->pos = alarm.runtime_pos;
    assert(ge_original_prop_state_set_primary_room(alarm_prop, 1));
    ge_original_prop_state_activate(&prop_state, alarm_prop);
    ge_original_prop_state_enable(&prop_state, alarm_prop);
    identity[0][0] = identity[1][1] = 1.0f;
    identity[2][2] = identity[3][3] = 1.0f;
    assert(ge_original_prop_state_publish_scene_visibility(
        alarm_prop, 1, identity));
    assert((alarm_prop->flags & PROPFLAG_ONSCREEN) != 0U);
    assert(fabsf(model->render_pos[0].pos.m[3][0] - 125.0f) < 0.001f);
    assert(fabsf(model->render_pos[0].pos.m[3][1] + 50.0f) < 0.001f);
    assert(fabsf(model->render_pos[0].pos.m[3][2] + 900.0f) < 0.001f);
    ge_original_pp7_fire_bind_object_hit_ready(provider, pitem_hit_ready);
    for (yi = -100; yi <= 50 && !hit; yi += 50) {
        for (xi = -100; xi <= 100 && !hit; xi += 50) {
            coord3d ray_pos = {125.0f + (float)xi,
                               -50.0f + (float)yi, 0.0f};
            coord3d ray_dir = {0.0f, 0.0f, -1.0f};
            HitThing thing;
            s32 matrix_index = -1;
            ModelNode *hit_node = NULL;
            memset(&thing, 0, sizeof(thing));
            hit = propobjFindHit(model, header->RootNode, &ray_pos, &ray_dir,
                                 &thing, &matrix_index, &hit_node);
            if (hit) assert(matrix_index == 0 && hit_node != NULL);
        }
    }
    assert(hit);
    ge_original_pp7_fire_bind_object_hit_ready(NULL, NULL);
    assert(ge_original_pitem_model_release_instance(provider, model));
    ge_original_pitem_model_provider_destroy(provider);
    ge_asset_pack_close(&pack);
}

static unsigned char *read_file(const char *path, size_t *size)
{
    FILE *file = fopen(path, "rb");
    long length;
    unsigned char *bytes;
    assert(file != NULL);
    assert(fseek(file, 0L, SEEK_END) == 0);
    length = ftell(file);
    assert(length > 0L);
    assert(fseek(file, 0L, SEEK_SET) == 0);
    *size = (size_t)length;
    bytes = malloc(*size);
    assert(bytes != NULL);
    assert(fread(bytes, 1U, *size, file) == *size);
    assert(fclose(file) == 0);
    return bytes;
}

static void prepare_player(struct player *player,
                           struct player_data *permissions,
                           GeOriginalPropState *prop_state,
                           const GeStanNativeMap *native,
                           Mtxf *view)
{
    PropRecord *player_prop;
    float x = 0.0f;
    float z = 0.0f;
    uint16_t point;

    for (point = 0U; point < ge_stan_native_point_count(native->spawn_tile);
            point++)
    {
        x += native->spawn_tile->points[point].x;
        z += native->spawn_tile->points[point].z;
    }
    x /= ge_stan_native_point_count(native->spawn_tile) * native->level_scale;
    z /= ge_stan_native_point_count(native->spawn_tile) * native->level_scale;

    memset(player, 0, sizeof(*player));
    memset(permissions, 0, sizeof(*permissions));
    memset(view, 0, sizeof(*view));
    view->m[0][0] = view->m[1][1] = view->m[2][2] = view->m[3][3] = 1.0f;
    assert(ge_original_prop_state_reset(prop_state, 137U));
    player_prop = ge_original_prop_state_allocate_player(prop_state);
    assert(player_prop != NULL);
    player_prop->type = PROP_TYPE_PLAYER;
    player_prop->stan = (StandTile *)native->spawn_tile;
    player_prop->pos.x = x;
    player_prop->pos.y = ge_original_stan_get_position_y(
        native, native->spawn_tile, x, z) + 175.0f;
    player_prop->pos.z = z;
    view->m[3][0] = player_prop->pos.x;
    view->m[3][1] = player_prop->pos.y;
    view->m[3][2] = player_prop->pos.z;

    player->prop = player_prop;
    player->viewtoworldmtxf = view;
    player->c_screenwidth = 320.0f;
    player->c_screenheight = 240.0f;
    player->c_halfwidth = 160.0f;
    player->c_halfheight = 120.0f;
    player->c_scalex = 1.0f;
    player->c_scaley = 1.0f;
    player->c_perspaspect = 4.0f / 3.0f;
    player->crosshair_angle.x = 160.0f;
    player->crosshair_angle.y = 120.0f;
    player->hands[GUNRIGHT].weaponnum = ITEM_WPPK;
    player->hands[GUNRIGHT].weaponnum_watchmenu = -1;
    player->hands[GUNRIGHT].weapon_firing_status = 1;
    player->hands[GUNRIGHT].weapon_ammo_in_magazine = 6;
    player->hands[GUNRIGHT].volley = 1;
    player->hands[GUNLEFT].weaponnum = ITEM_UNARMED;
    player->hands[GUNLEFT].weaponnum_watchmenu = -1;
    ge_original_bond_input_bind_player(player, permissions);
    ge_original_bond_input_provider_reset_normal_dam();
}

static void test_canonical_impact_images(void)
{
    struct BulletImpact *saved = g_BulletImpactBuffer;
    const int saved_count = g_NumImpactEntries;
    g_BulletImpactBuffer = calloc(BULLET_IMPACT_BUFFER_LEN, sizeof(*g_BulletImpactBuffer));
    assert(g_BulletImpactBuffer != NULL);
    g_NumImpactEntries = 0;
    PropRecord impact_prop = {0};
    ObjectRecord impact_object = {0};
    Model impact_model = {0};
    RenderPosView impact_matrix = {0};
    coord3d impact_position = {{0.0f, 0.0f, 0.0f}};
    coord3d impact_normal = {{0.0f, 1.0f, 0.0f}};
    int type;
    impact_matrix.pos.m[0][0] = impact_matrix.pos.m[1][1] = 1.0f;
    impact_matrix.pos.m[2][2] = impact_matrix.pos.m[3][3] = 1.0f;
    impact_prop.obj = &impact_object;
    impact_object.model = &impact_model;
    impact_model.render_pos = &impact_matrix;
    assert(impactimages != NULL && explosion_smokeimages != NULL
           && scattered_explosions != NULL && flareimage2 != NULL);
    assert(impactimages[7].index == IMAGE_IMPACT4
           && impactimages[7].width == 32U && impactimages[7].height == 32U);
    assert(explosion_smokeimages[5].index == IMAGE_SMOKE6);
    assert(scattered_explosions[4].index == IMAGE_SMOKEBALLS5);
    assert(flareimage2[0].index == IMAGE_WHITEBOX);
    for (type = 0; type < 20; ++type) {
        const int impact_index = g_NumImpactEntries;
        explosionCreateBulletImpact(&impact_position, &impact_normal,
            (s16)type, 1, &impact_prop, 0, 0);
        assert(g_BulletImpactBuffer[impact_index].vertex_list[0].v.tc[1]
               == (s16)(impactimages[type].height << 5U));
        assert(g_BulletImpactBuffer[impact_index].vertex_list[2].v.tc[0]
               == (s16)(impactimages[type].width << 5U));
    }
    free(g_BulletImpactBuffer);
    g_BulletImpactBuffer = saved;
    g_NumImpactEntries = saved_count;
    puts("Bullet impacts: all 20 canonical image records drive original UV construction");
}

int main(int argc, char **argv)
{
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeOriginalPropState prop_state;
    GeOriginalPp7FireStats stats;
    GeOriginalPp7FireStatus status;
    struct player player;
    struct player_data permissions;
    Mtxf view;
    unsigned char *collision_bytes;
    unsigned char *native_bytes;
    size_t collision_size;
    size_t native_size;

    assert(argc == 3);
    test_native_alarm_ray(argv[2]);
    collision_bytes = read_file(argv[1], &collision_size);
    assert(ge_stan_collision_open(collision_bytes, collision_size, &surface)
           == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface, &native_size)
           == GE_STAN_COLLISION_OK);
    native_bytes = malloc(native_size);
    assert(native_bytes != NULL);
    assert(ge_stan_native_materialize(&surface, 0.23363999f,
               native_bytes, native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&native) == GE_STAN_COLLISION_OK);
    prepare_player(&player, &permissions, &prop_state, &native, &view);
    test_canonical_impact_images();

    ge_original_pp7_fire_reset();
    status = ge_original_pp7_fire_tick();
    assert(status == GE_ORIGINAL_PP7_FIRE_STAN_HIT
           || status == GE_ORIGINAL_PP7_FIRE_BACKGROUND_PROP_FRONTIER);
    ge_original_pp7_fire_snapshot(&stats);
    assert(stats.both_hands_ticks == 1U);
    assert(stats.hand_dispatches == 2U);
    assert(stats.pp7_shots == 1U);
    assert(stats.last_weapon == ITEM_WPPK);
    assert(stats.last_ammo_after_hand_tick == 6);
    assert(stats.last_shot_sound == 0x6bU);
    assert(isfinite(stats.last_direction[0]));
    assert(isfinite(stats.last_direction[1]));
    assert(isfinite(stats.last_direction[2]));
    assert(fabsf(stats.last_direction[0]) + fabsf(stats.last_direction[1])
               + fabsf(stats.last_direction[2]) > 0.9f);
    assert(isfinite(stats.last_endpoint[0]));
    assert(isfinite(stats.last_endpoint[1]));
    assert(isfinite(stats.last_endpoint[2]));

    ge_original_pp7_fire_reset();
    player.hands[GUNRIGHT].weaponnum = ITEM_WPPKSIL;
    player.hands[GUNRIGHT].weapon_ammo_in_magazine = 5;
    status = ge_original_pp7_fire_tick();
    assert(status == GE_ORIGINAL_PP7_FIRE_STAN_HIT
           || status == GE_ORIGINAL_PP7_FIRE_BACKGROUND_PROP_FRONTIER);
    ge_original_pp7_fire_snapshot(&stats);
    assert(stats.last_weapon == ITEM_WPPKSIL);
    assert(stats.last_ammo_after_hand_tick == 5);
    assert(stats.last_shot_sound == 0x2eU);

    /* Feed a Dam alarm-shaped ordinary object through the unchanged objHit
     * damage tail and prove both healthy and accumulated nonlethal states.
     * Lethal objExplode is covered by the live explosion arena rather than
     * fabricating its PropRecord allocation in this focused fixture. */
    {
        ObjectRecord alarm;
        PropRecord alarm_prop;
        Model alarm_model;
        ModelFileHeader alarm_header;
        ShotData alarm_shot;
        BulletHit alarm_hit;
        memset(&alarm, 0, sizeof(alarm));
        memset(&alarm_prop, 0, sizeof(alarm_prop));
        memset(&alarm_model, 0, sizeof(alarm_model));
        memset(&alarm_header, 0, sizeof(alarm_header));
        memset(&alarm_shot, 0, sizeof(alarm_shot));
        memset(&alarm_hit, 0, sizeof(alarm_hit));
        g_playerPerm = &permissions;
        ((PropDefHeaderRecord *)&alarm)->type = PROPDEF_ALARM;
        alarm.flags2 = PROPFLAG2_00002000;
        alarm.damage = gunItemGetDestructionAmount(ITEM_WATCHLASER) * 500.0f;
        assert(alarm.damage > 0.0f);
        alarm.obj = 0;
        alarm.prop = &alarm_prop;
        alarm.model = &alarm_model;
        alarm_model.obj = &alarm_header;
        alarm_prop.type = PROP_TYPE_OBJ;
        alarm_prop.obj = &alarm;
        alarm_prop.stan = player.prop->stan;
        alarm_prop.rooms[0] = player.prop->stan->room;
        alarm_prop.rooms[1] = -1;
        alarm_shot.viewDir.z = -1.0f;
        alarm_shot.dir.z = -1.0f;
        /* Watch laser follows the same object damage tail while intentionally
         * suppressing objHit's renderer-only impact particle branch. */
        alarm_shot.weapon = ITEM_WATCHLASER;
        alarm_hit.prop = &alarm_prop;
        alarm_hit.countsAsPenetration = 1;
        alarm_hit.hit.texturenum = -1;
        alarm_hit.room = alarm_prop.stan->room;
        assert(objGetDestroyedLevel(&alarm) == 0);
        objHit(&alarm_shot, &alarm_hit);
        assert(objGetDestroyedLevel(&alarm) == 0
               && alarm.maxdamage > 0.0f
               && alarm.maxdamage < alarm.damage);
        objHit(&alarm_shot, &alarm_hit);
        assert(objGetDestroyedLevel(&alarm) == 0
               && alarm.maxdamage == alarm.damage);
    }
    puts("original PP7 hand-fire to authored Dam STAN hitscan prefix: ok");

    free(native_bytes);
    free(collision_bytes);
    return 0;
}
