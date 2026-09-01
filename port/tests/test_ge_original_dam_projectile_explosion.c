#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "bondaicommands.h"
#include "game/bondview.h"
#include "game/chraction.h"
#include "game/chrai.h"
#include "game/explosion.h"
#include "game/matrixmath.h"
#include "game/model.h"
#include "game/objecthandler.h"
#include "game/player.h"
#include "game/propobj.h"
#include "game/stan.h"
#include "game/stanintersection.h"

bool projectileFindCollidingProp(PropRecord *ignore_prop,
        coord3d *start, coord3d *end, u32 cdtypes, coord3d *hit_pos,
        coord3d *hit_normal, s32 *rooms);
void propExplode(PropRecord *prop, s32 explosion_type);
u32 modelFindNextProjectileHitCandidate(Model *model, coord3d *ray_pos,
        coord3d *ray_dir, ModelNode **nodeptr);

PropRecord g_Props[4];
s16 *ptr_list_object_lookup_indices;
struct player *g_playerPointers[4];
PropRecord *D_80030B0C;
s32 bodypartshot;
Model *g_CurrentProjectileModel;
ModelNode *dword_CODE_bss_80075B74;
coord3d flt_CODE_bss_80075B78;
coord3d flt_CODE_bss_80075B88;

static Mtxf identity;
static Mtxf hit_matrix;
static Model collision_model;
static ModelNode collision_node;
static s32 room_calls;
static s32 explosion_calls;
static s32 walk_calls;
static bool walk_result;
static coord3d explosion_pos;
static StandTile *explosion_stan;
static s16 explosion_type_seen;
static s32 explosion_arg4;
static s32 explosion_player;
static u8 *explosion_rooms;
static s32 explosion_arg7;
static bool bbox_hit;
static bool propobj_hit;
static bool chr_hit;

static void set_identity(Mtxf *mtx)
{
    memset(mtx, 0, sizeof(*mtx));
    mtx->m[0][0] = 1.0f;
    mtx->m[1][1] = 1.0f;
    mtx->m[2][2] = 1.0f;
    mtx->m[3][3] = 1.0f;
}

Mtxf *camGetWorldToScreenMtxf(void) { return &identity; }
Mtxf *currentPlayerGetViewToWorldMtxf(void) { return &identity; }
Mtxf *getsubmatrix(Model *model) { (void)model; return &hit_matrix; }

void mtx4TransformVecInPlace(Mtxf *mtx, coord3d *vec)
{
    coord3d old = *vec;
    vec->x = old.x * mtx->m[0][0] + old.y * mtx->m[1][0]
        + old.z * mtx->m[2][0] + mtx->m[3][0];
    vec->y = old.x * mtx->m[0][1] + old.y * mtx->m[1][1]
        + old.z * mtx->m[2][1] + mtx->m[3][1];
    vec->z = old.x * mtx->m[0][2] + old.y * mtx->m[1][2]
        + old.z * mtx->m[2][2] + mtx->m[3][2];
}

void mtx4RotateVecInPlace(Mtxf *mtx, coord3d *vec)
{
    coord3d old = *vec;
    vec->x = old.x * mtx->m[0][0] + old.y * mtx->m[1][0]
        + old.z * mtx->m[2][0];
    vec->y = old.x * mtx->m[0][1] + old.y * mtx->m[1][1]
        + old.z * mtx->m[2][1];
    vec->z = old.x * mtx->m[0][2] + old.y * mtx->m[1][2]
        + old.z * mtx->m[2][2];
}

void guNormalize(f32 *x, f32 *y, f32 *z)
{
    f32 len = sqrtf(*x * *x + *y * *y + *z * *z);
    if (len != 0.0f) {
        *x /= len;
        *y /= len;
        *z /= len;
    }
}

void roomGetProps(s32 *rooms) { (void)rooms; room_calls++; }
PropRecord *get_ptr_for_players_tank(void) { return NULL; }
s32 getPlayerPointerIndex(PropRecord *prop) { (void)prop; return 0; }
s32 propDoorGetCdTypes(PropRecord *prop) { (void)prop; return CDTYPE_DOORS; }
f32 getinstsize(Model *model) { (void)model; return 2.0f; }
Mtxf *modelFindNodeMtx(Model *model, ModelNode *node, s32 index)
{
    (void)model; (void)node; (void)index;
    return &hit_matrix;
}

s32 sub_GAME_7F06C010(ModelHitEntry **entry, coord3d *start, coord3d *dir,
        Model **model, ModelNode **node)
{
    (void)entry; (void)start; (void)dir;
    if (!chr_hit) return 0;
    *model = &collision_model;
    *node = &collision_node;
    return HIT_HEAD;
}

bool modelTestRayIntersectsTransformedBBox(ModelRoData_BoundingBoxRecord *bbox,
        Mtxf *mtx, coord3d *pos, coord3d *dir)
{
    (void)bbox; (void)mtx; (void)pos; (void)dir;
    return bbox_hit;
}

bool modelTestRayIntersectsNodeBBox(Model *model, ModelNode *node,
        coord3d *pos, coord3d *dir)
{
    (void)model; (void)node; (void)pos; (void)dir;
    return bbox_hit;
}

s32 sub_GAME_7F074CAC(Model *model, ModelNode *node,
        coord3d *pos, coord3d *dir)
{
    (void)model; (void)node; (void)pos; (void)dir;
    return 0;
}

void modelApplyDistanceRelations(Model *model, ModelNode *node)
{ (void)model; (void)node; }
void modelApplyToggleRelations(Model *model, ModelNode *node)
{ (void)model; (void)node; }
void modelApplyHeadRelations(Model *model, ModelNode *node)
{ (void)model; (void)node; }

bool propobjFindHit(Model *model, ModelNode *start, coord3d *pos,
        coord3d *dir, HitThing *hit, s32 *mtxindex, ModelNode **dstnode)
{
    (void)model; (void)start; (void)pos; (void)dir;
    if (!propobj_hit) return FALSE;
    hit->hitpos.x = 3.0f;
    hit->hitpos.y = 0.0f;
    hit->hitpos.z = 0.0f;
    hit->normal.x = -1.0f;
    hit->normal.y = 0.0f;
    hit->normal.z = 0.0f;
    *mtxindex = 0;
    *dstnode = &collision_node;
    return TRUE;
}

bool projectileTestPropBoundingSphere(coord3d *origin, coord3d *dir,
        coord3d *center, f32 scale)
{ (void)origin; (void)dir; (void)center; (void)scale; return FALSE; }

void chraiGetCollisionBounds(PropRecord *prop, struct rect4f **polygon,
        s32 *edges, f32 *top, f32 *bottom)
{
    (void)prop;
    *polygon = NULL; *edges = 0; *top = 0.0f; *bottom = 0.0f;
}
bool doSegmentsIntersect(f32 a, f32 b, f32 c, f32 d,
        f32 e, f32 f, f32 g, f32 h)
{ (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; return FALSE; }
f32 calculateSegmentIntersectionFraction(coord2d *a, coord2d *b,
        coord2d *c, coord2d *d)
{ (void)a;(void)b;(void)c;(void)d; return 0.0f; }
void chrlvLineLineIntersection(coord3d *a, coord3d *b, coord3d *c,
        coord3d *d, coord3d *result)
{ (void)a;(void)b;(void)c;(void)d; memset(result, 0, sizeof(*result)); }

s32 walkTilesBetweenPoints_NoCallback(StandTile **stan, f32 sx, f32 sz,
        f32 dx, f32 dz)
{
    (void)stan; (void)sx; (void)sz; (void)dx; (void)dz;
    walk_calls++;
    return walk_result;
}

void explosionCreate(PropRecord *source, coord3d *pos, StandTile *stan,
        s16 type, s32 arg4, s32 player, u8 *rooms, s32 arg7)
{
    (void)source;
    explosion_calls++;
    explosion_pos = *pos;
    explosion_stan = stan;
    explosion_type_seen = type;
    explosion_arg4 = arg4;
    explosion_player = player;
    explosion_rooms = rooms;
    explosion_arg7 = arg7;
}

static void reset_state(void)
{
    memset(g_Props, 0, sizeof(g_Props));
    memset(g_playerPointers, 0, sizeof(g_playerPointers));
    set_identity(&identity);
    set_identity(&hit_matrix);
    memset(&collision_model, 0, sizeof(collision_model));
    memset(&collision_node, 0, sizeof(collision_node));
    room_calls = explosion_calls = walk_calls = 0;
    walk_result = FALSE;
    bbox_hit = propobj_hit = chr_hit = FALSE;
    D_80030B0C = NULL;
    bodypartshot = HIT_NULL_PART;
    g_CurrentProjectileModel = NULL;
    dword_CODE_bss_80075B74 = NULL;
}

static void test_zero_length_and_character_hit(void)
{
    s16 lookup[] = {0, -1};
    s32 rooms[] = {1, -1};
    coord3d start = {0.0f, 0.0f, 0.0f};
    coord3d end = {0.0f, 0.0f, 0.0f};
    coord3d hit = {0};
    coord3d normal = {0};
    ChrRecord chr;

    reset_state();
    ptr_list_object_lookup_indices = lookup;
    assert(!projectileFindCollidingProp(NULL, &start, &end, CDTYPE_CHRS,
            &hit, &normal, rooms));
    assert(room_calls == 0);

    memset(&chr, 0, sizeof(chr));
    g_Props[0].type = PROP_TYPE_CHR;
    g_Props[0].flags = PROPFLAG_ONSCREEN;
    g_Props[0].pos.x = 5.0f;
    g_Props[0].chr = &chr;
    chr.prop = &g_Props[0];
    chr.model = &collision_model;
    hit_matrix.m[3][0] = 4.0f;
    end.x = 10.0f;
    chr_hit = TRUE;

    assert(projectileFindCollidingProp(NULL, &start, &end, CDTYPE_CHRS,
            &hit, &normal, rooms));
    assert(room_calls == 1);
    assert(fabsf(hit.x - 4.0f) < 0.001f);
    assert(fabsf(normal.x + 1.0f) < 0.001f);
    assert(D_80030B0C == &g_Props[0]);
    assert(bodypartshot == HIT_HEAD);
    assert(g_CurrentProjectileModel == &collision_model);
    assert(dword_CODE_bss_80075B74 == &collision_node);

    assert(!projectileFindCollidingProp(&g_Props[0], &start, &end,
            CDTYPE_CHRS, &hit, &normal, rooms));
}

static void test_object_hit_and_model_candidate(void)
{
    s16 lookup[] = {0, -1};
    s32 rooms[] = {1, -1};
    coord3d start = {0.0f, 0.0f, 0.0f};
    coord3d end = {10.0f, 0.0f, 0.0f};
    coord3d hit = {0};
    coord3d normal = {0};
    DoorRecord door;
    ModelFileHeader model_file;
    ModelRoData_BoundingBoxRecord bbox;
    RenderPosView render_pos[1];
    u32 candidate = 0x1234;
    ModelNode root;
    ModelNode child;
    ModelNode *cursor = NULL;

    reset_state();
    memset(&door, 0, sizeof(door));
    memset(&model_file, 0, sizeof(model_file));
    memset(&bbox, 0, sizeof(bbox));
    memset(render_pos, 0, sizeof(render_pos));
    memset(&root, 0, sizeof(root));
    memset(&child, 0, sizeof(child));
    set_identity(&render_pos[0].pos);
    ptr_list_object_lookup_indices = lookup;

    root.Child = &child;
    child.Parent = &root;
    child.Opcode = MODELNODE_OPCODE_BBOX;
    child.Data = &candidate;
    model_file.RootNode = &root;
    collision_model.obj = &model_file;
    collision_model.render_pos = render_pos;
    collision_node = child;
    door.bbox = bbox;
    door.type = PROPDEF_DOOR;
    door.model = &collision_model;
    door.prop = &g_Props[0];
    door.runtime_pos.x = 4.0f;
    g_Props[0].type = PROP_TYPE_OBJ;
    g_Props[0].flags = PROPFLAG_ONSCREEN;
    g_Props[0].obj = (ObjectRecord *)&door;
    bbox_hit = TRUE;
    propobj_hit = TRUE;

    assert(modelFindNextProjectileHitCandidate(&collision_model, &start,
            &end, &cursor) == candidate);
    assert(cursor == &child);
    assert(projectileFindCollidingProp(NULL, &start, &end, CDTYPE_OBJS,
            &hit, &normal, rooms));
    assert(fabsf(hit.x - 3.0f) < 0.001f);
    assert(D_80030B0C == &g_Props[0]);
    assert(bodypartshot == -1);
}

static void test_prop_explode_paths(void)
{
    ObjectRecord obj;
    PropRecord prop;
    PropRecord parent;
    StandTile *stan = (StandTile *)(uintptr_t)0x1234;

    reset_state();
    memset(&obj, 0, sizeof(obj));
    memset(&prop, 0, sizeof(prop));
    memset(&parent, 0, sizeof(parent));
    prop.obj = &obj;
    obj.prop = &prop;
    obj.runtime_pos.x = 1.0f;
    obj.runtime_pos.y = 2.0f;
    obj.runtime_pos.z = 3.0f;
    obj.runtime_bitflags = 2 << RUNTIMEBITSHIFT_OWNER;
    prop.stan = stan;
    propExplode(&prop, EXPLOSION_DEF_STANDARD);
    assert(explosion_calls == 1);
    assert(explosion_pos.x == 1.0f && explosion_pos.y == 2.0f
            && explosion_pos.z == 3.0f);
    assert(explosion_stan == stan);
    assert(explosion_type_seen == EXPLOSION_DEF_STANDARD);
    assert(explosion_player == 2);
    assert(explosion_rooms == prop.rooms);
    assert(explosion_arg4 == 1 && explosion_arg7 == 0);

    explosion_calls = walk_calls = 0;
    prop.parent = &parent;
    parent.pos.x = 7.0f;
    parent.pos.y = 8.0f;
    parent.pos.z = 9.0f;
    parent.stan = stan;
    walk_result = TRUE;
    propExplode(&prop, EXPLOSION_DEF_FACILITY_REMOTE);
    assert(explosion_calls == 1 && walk_calls == 1);
    assert(explosion_pos.x == 7.0f && explosion_pos.y == 8.0f
            && explosion_pos.z == 9.0f);
    assert(explosion_rooms == parent.rooms);
    assert(explosion_arg4 == 1 && explosion_arg7 == 0);

    explosion_calls = walk_calls = 0;
    parent.flags = PROPFLAG_00000008;
    propExplode(&prop, EXPLOSION_DEF_FACILITY_REMOTE);
    assert(explosion_calls == 1 && walk_calls == 0);
    assert(explosion_arg4 == 0 && explosion_arg7 == 1);
}

int main(void)
{
    test_zero_length_and_character_hit();
    test_object_hit_and_model_candidate();
    test_prop_explode_paths();
    puts("Dam projectile/explosion: exact modem collision and explosion paths pass");
    return 0;
}
