#include "ge_original_guard_bullet_hit.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <bondaicommands.h>
#include <music.h>
#include <snd.h>
#include "game/bondview.h"
#include "game/chr.h"
#include "game/chraction.h"
#include "game/gun.h"
#include "game/model.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/image.h"
#include "game/tex.h"
#include "game/textrelated.h"

static struct player players[4];
static PropRecord props[4];
static ChrRecord characters[4];
static Model models[4];
static ModelFileHeader headers[4];
static ModelNode roots[4], boxes[4];
static union ModelRoData rootdata[4], boxdata[4];
static RenderPosView matrices[4];
static ModelHitEntry hitentries[4];
static Mtxf view;
static size_t roster_count;
static unsigned effects, drops;
static s32 scenario;
extern WeaponStats wppk_stats;
extern struct player_data g_playerPlayerData[4];
extern struct player_data *g_playerPerm;
extern ModelHitEntry *sub_GAME_7F06B120(ModelHitEntry *, Model *);

s32 g_PlayerIsInTank, g_stopPlayFlag, g_gameOverFlag, g_isBondKIA, g_ExplodeTankOnDeathFlag;
bool g_PlayerInvincible;
PropRecord *g_PlayerTankProp;
f32 g_AiHealthModifier = 1;
ALBank *g_musicSfxBufferPtr;
struct image_sound *g_HitTypeSounds[1];
struct image_entry g_Textures[1];
GunModelFileRecord gitem_structs[64];
ModelFileHeader cartridge_header;
s16 metal_ricochet_SFX[3];
char aSD[] = "%s%d", g_GunDeathCountFormat[] = "%s%d%s";

static size_t player_count(void *context) { assert(context == players); return roster_count; }
static void *player_record(void *context, size_t slot)
{ assert(context == players && slot < roster_count); return &players[slot]; }
static size_t authored_count(void *context) { assert(context == characters); return 1; }
static int authored_actor(void *context, size_t index, void **prop, void **chr)
{
    assert(context == characters && index == 0);
    *prop = &props[1]; *chr = &characters[1]; return 1;
}
size_t ge_original_dam_guards_count(void) { return 0; }
void *ge_original_dam_guard_prop(size_t slot) { (void)slot; abort(); }
void *ge_original_dam_guard_chr(size_t slot) { (void)slot; abort(); }
WeaponStats *get_ptr_item_statistics(ITEM_IDS item)
{ assert(item == ITEM_WPPK); return &wppk_stats; }
/* Only presentation, inventory-drop publication and unreachable nonfixture
 * model branches are sinks. Ray intersection, damage and scores are original. */
void gunSetTracerTarget(coord3d *pos) { assert(isfinite(pos->z)); ++effects; }
void recall_joy2_hits_edit_detail_edit_flag(ITEM_IDS item, PropRecord *prop, s32 arg)
{ (void)item; (void)prop; (void)arg; ++effects; }
void chrCreateHitPuffs(PropRecord *prop, s32 hitpart, coord3d *hit, coord3d *near)
{ (void)prop; (void)hitpart; assert(isfinite(hit->z) && isfinite(near->z)); ++effects; }
void chrCreateBloodStain(Model *model, s32 part, ModelNode *node, coord3d *pos)
{ (void)model; (void)part; (void)node; assert(isfinite(pos->z)); ++effects; }
void play_sound_for_shot_actor(ChrRecord *chr) { assert(chr->prop->type == PROP_TYPE_VIEWER); ++effects; }
ALSoundState *sndPlaySfx(struct ALBankAlt_s *bank, s16 sound, ALSoundState *old)
{ (void)bank; (void)sound; (void)old; ++effects; return NULL; }
void chrobjSndCreatePostEventDefault(ALSoundState *snd, coord3d *pos)
{ (void)snd; (void)pos; abort(); }
void joyRumblePakStart(s8 slot, f32 strength) { assert(slot >= 0 && slot < (s8)roster_count); assert(strength == 0.25f); }
s32 cur_player_get_control_type(void) { return 0; }
s32 get_scenario(void) { return scenario; }
bool bondinvHasGoldenGun(void) { return FALSE; }
void drop_inventory(void) { ++drops; }
s32 currentPlayerEquipWeaponWrapper(GUNHAND hand, s32 item)
{ assert(hand == GUNLEFT || hand == GUNRIGHT); assert(item == 0); return 0; }
s32 getMissiontimer(void) { return 100; }
void trigger_solo_watch_menu(s32 value) { (void)value; abort(); }
void hudmsgBottomShow(char *text) { assert(text); ++effects; }
u8 *langGet(s32 id) { (void)id; return (u8 *)"stat"; }
void increment_num_suicides_display_MP(void) { abort(); }
f32 currentPlayerGetArmor(void) { return g_CurrentPlayer->bondarmour; }
f32 currentPlayerGetHealth(void) { return g_CurrentPlayer->bondhealth; }
void hudMakeDamageSegments(f32 *values, s32 a, s32 b, f32 c)
{ (void)values; (void)a; (void)b; (void)c; abort(); }
void inc_curplayer_hitcount_with_weapon(ITEM_IDS item, SHOT_REGISTER hit)
{ assert(item == ITEM_WPPK && hit == SHOT_REGISTER_BODY); }
void chrSetHiddenToRandom(ChrRecord *chr) { (void)chr; }
f32 modelGetAnimFrame(Model *model) { (void)model; abort(); }
void triggered_on_shot_hit(ChrRecord *chr, coord3d *pos, f32 angle, s32 part, ITEM_IDS item)
{ (void)chr; (void)pos; (void)angle; (void)part; (void)item; abort(); }
void modelSetAnimEndFrame(Model *model, f32 end) { (void)model; (void)end; abort(); }
f32 getjointsize(Model *model, ModelNode *node) { (void)model; (void)node; return 1; }
union ModelRwData *modelGetNodeRwData(Model *model, ModelNode *node) { (void)model; (void)node; abort(); }
void modelApplyDistanceRelations(Model *model, ModelNode *node) { (void)model; (void)node; abort(); }
void modelApplyToggleRelations(Model *model, ModelNode *node) { (void)model; (void)node; abort(); }
void modelApplyHeadRelations(Model *model, ModelNode *node) { (void)model; (void)node; abort(); }
s32 sub_GAME_7F074CAC(Model *model, ModelNode *node, coord3d *p, coord3d *d)
{ (void)model; (void)node; (void)p; (void)d; abort(); }
bool propobjFindHit(Model *model, ModelNode *node, coord3d *p, coord3d *d, struct HitThing *hit, s32 *index, ModelNode **dl)
{ (void)model; (void)node; (void)p; (void)d; (void)hit; (void)index; (void)dl; abort(); }
s32 get_hat_model(PropRecord *prop) { (void)prop; abort(); }
void propobjSetDropped(PropRecord *prop, DROPTYPE value) { (void)prop; (void)value; abort(); }
void objApplyDamage(ObjectRecord *obj, f32 amount, coord3d *pos, s32 item, s32 slot)
{ (void)obj; (void)amount; (void)pos; (void)item; (void)slot; abort(); }
void explosionCreateBulletImpact(coord3d *p, coord3d *n, s32 type, s32 unknown, PropRecord *prop, s32 room, s32 unknown2)
{ (void)p; (void)n; (void)type; (void)unknown; (void)prop; (void)room; (void)unknown2; abort(); }
u32 randomGetNext(void) { return 1; }

static void initialize(size_t count)
{
    size_t i;
    memset(players, 0, sizeof(players)); memset(props, 0, sizeof(props));
    memset(characters, 0, sizeof(characters)); memset(models, 0, sizeof(models));
    memset(headers, 0, sizeof(headers)); memset(roots, 0, sizeof(roots));
    memset(boxes, 0, sizeof(boxes)); memset(rootdata, 0, sizeof(rootdata));
    memset(boxdata, 0, sizeof(boxdata)); memset(matrices, 0, sizeof(matrices));
    memset(hitentries, 0, sizeof(hitentries)); memset(g_playerPlayerData, 0, sizeof(g_playerPlayerData));
    memset(g_playerPointers, 0, sizeof(struct player *) * 4);
    roster_count = count; effects = drops = 0; scenario = 0;
    memset(&view, 0, sizeof(view));
    for (i = 0; i < 4; ++i) view.m[i][i] = 1;
    for (i = 0; i < count; ++i) {
        size_t axis;
        g_playerPointers[i] = &players[i];
        players[i].prop = &props[i]; players[i].bodyModel = &models[i];
        players[i].bondhealth = 1; players[i].actual_health = players[i].actual_armor = 1;
        players[i].damageshowtime = -1; players[i].viewtoworldmtxf = &view;
        g_playerPlayerData[i].handicap = 1; g_playerPlayerData[i].shortest_inning = 10000;
        props[i].type = PROP_TYPE_VIEWER; props[i].chr = &characters[i];
        props[i].flags = PROPFLAG_ONSCREEN; props[i].pos.z = -100 - (float)i * 100;
        props[i].zDepth = -props[i].pos.z; props[i].rooms[0] = 1;
        characters[i].prop = &props[i]; characters[i].model = &models[i];
        characters[i].actiontype = ACT_PREARGH;
        models[i].obj = &headers[i]; models[i].scale = 1; models[i].render_pos = &matrices[i];
        headers[i].RootNode = &roots[i]; headers[i].BoundingVolumeRadius = 20;
        roots[i].Opcode = MODELNODE_OPCODE_GROUP; roots[i].Data = &rootdata[i]; roots[i].Child = &boxes[i];
        boxes[i].Opcode = MODELNODE_OPCODE_BBOX; boxes[i].Data = &boxdata[i]; boxes[i].Parent = &roots[i];
        boxdata[i].BoundingBox.ModelNumber = HIT_CHEST;
        boxdata[i].BoundingBox.Bounds = (struct bbox){-10, 10, -10, 10, -10, 10};
        for (axis = 0; axis < 4; ++axis) matrices[i].pos.m[axis][axis] = 1;
        matrices[i].pos.m[3][2] = props[i].pos.z;
        hitentries[i].model = &models[i]; hitentries[i].rootnode = &roots[i];
        characters[i].field_20 = &hitentries[i];
    }
    gitem_structs[ITEM_WPPK].item_weapon_stats = &wppk_stats;
    set_cur_player(0);
    ge_original_guard_bullet_hit_bind_players(players, player_count, player_record);
}
static ShotData ray(void)
{
    ShotData shot;
    memset(&shot, 0, sizeof(shot));
    shot.viewDir.z = shot.dir.z = -1; shot.weapon = ITEM_WPPK; shot.maxdist = 1000;
    return shot;
}
static ShotData aimed_shot(size_t target)
{
    ShotData shot = ray();
    (void)ge_original_guard_bullet_hit_populate_shot(&shot);
    assert(shot.hits[0].prop == &props[target]);
    assert(shot.hits[0].hitpart == HIT_CHEST);
    assert(shot.hits[1].prop == NULL); /* PP7 canonical one-object penetration */
    return shot;
}
static void run(size_t count, size_t attacker, size_t target)
{
    ShotData shot;
    float damage;
    size_t i;
    initialize(count);
    set_cur_player((s32)attacker);
    /* Primitive fixture geometry is in the selected shooter's camera space.
     * Its own nearest box must never mask the intended opponent. */
    for (i = 0; i < count; ++i) {
        props[i].pos.x = i == attacker || i == target ? 0 : 100 + (float)i * 100;
        props[i].pos.z = i == attacker ? -100 : -200;
        props[i].zDepth = -props[i].pos.z;
        matrices[i].pos.m[3][0] = props[i].pos.x;
        matrices[i].pos.m[3][2] = props[i].pos.z;
    }
    shot = aimed_shot(target);
    damage = wppk_stats.DestructionAmount * 2 * 0.125f;
    assert(damage > 0 && damage < 1);
    players[target].bondarmour = damage * 2;
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot, 0) == GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
    assert(players[target].bondhealth == 1 && players[target].bondarmour == damage);
    assert(g_CurrentPlayer == &players[attacker] && get_cur_playernum() == (s32)attacker && g_playerPerm == &g_playerPlayerData[attacker]);
    assert(characters[target].damage == 0 && players[attacker].bondhealth == 1);
    for (i = 0; i < count; ++i) if (i != target)
        assert(players[i].bondhealth == 1 && !players[i].bonddead);
    shot = aimed_shot(target);
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot, 0) == GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
    assert(players[target].bondarmour == 0);
    players[target].bondhealth = damage;
    shot = aimed_shot(target);
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot, 0) == GE_ORIGINAL_GUARD_BULLET_DAMAGE_APPLIED);
    assert(players[target].bonddead && players[target].deathcount == 1 && drops == 1);
    assert(g_playerPlayerData[attacker].kill_count == 1 && g_playerPlayerData[attacker].kill_counts[target] == 1);
    assert(players[attacker].kills_this_life == 1 && get_cur_playernum() == (s32)attacker);
    assert(effects > 0);
    /* Player models whose camera hit list has not been published are not
     * eligible. Do not let a render frontier become a NULL list crash. */
    characters[target].field_20 = NULL;
    shot = ray(); (void)ge_original_guard_bullet_hit_populate_shot(&shot);
    assert(!shot.hits[0].prop);
    characters[target].field_20 = &hitentries[target];
    characters[target].actiontype = ACT_DEAD;
    shot = ray(); (void)ge_original_guard_bullet_hit_populate_shot(&shot);
    assert(!shot.hits[0].prop);
    characters[target].actiontype = ACT_PREARGH;
    /* Miss, visibility/depth, and shooter-ownership gates remain canonical. */
    shot = ray(); shot.viewDir.x = 1; shot.viewDir.z = 0;
    (void)ge_original_guard_bullet_hit_populate_shot(&shot); assert(!shot.hits[0].prop);
    shot = ray(); shot.maxdist = 150;
    (void)ge_original_guard_bullet_hit_populate_shot(&shot); assert(!shot.hits[0].prop);
    for (i = 0; i < count; ++i) if (i != attacker) props[i].flags = 0;
    shot = ray(); (void)ge_original_guard_bullet_hit_populate_shot(&shot); assert(!shot.hits[0].prop);
    shot.hits[0].prop = &props[attacker];
    assert(ge_original_guard_bullet_hit_apply_shot_hit(&shot, 0) == GE_ORIGINAL_GUARD_BULLET_DAMAGE_FRONTIER);
    ge_original_guard_bullet_hit_bind_players(NULL, NULL, NULL);
}
int main(void)
{
    size_t count, attacker, target;
    for (count = 2; count <= 4; count += 2)
        for (attacker = 0; attacker < count; ++attacker)
            for (target = 0; target < count; ++target)
                if (attacker != target) run(count, attacker, target);
    /* Existing authored actors retain their separate binding; malformed
     * roster ownership cannot reinterpret an object/guard as a player. */
    initialize(2);
    props[1].type = PROP_TYPE_CHR;
    ge_original_guard_bullet_hit_bind_stage_guards(characters, authored_count, authored_actor);
    (void)aimed_shot(1);
    ge_original_guard_bullet_hit_bind_stage_guards(NULL, NULL, NULL);
    {
        ShotData shot = ray();
        (void)ge_original_guard_bullet_hit_populate_shot(&shot);
        assert(!shot.hits[0].prop);
    }
    ge_original_guard_bullet_hit_bind_players(NULL, NULL, NULL);
    puts("canonical multiplayer ray/BBOX -> damage/armor/death/scoring passed (2/4 players; presentation sinks)");
    return 0;
}
