/* Host-only false-branch providers for the focused PP7/object sanitizer.
 * The ARM runtime resolves every symbol below to the retained decompiled body.
 * Tests exercise the exact objHit damage branch; presentation, door/CCTV and
 * bounce branches are deliberately outside this fixture. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "bondaicommands.h"

typedef int PLAYERFLAG;
#include "game/model.h"
#include "game/propobj.h"

extern void objApplyDamage(ObjectRecord *obj, f32 damage, coord3d *pos,
    ITEM_IDS item, s32 owner);
extern s32 objGetDestroyedLevel(ObjectRecord *obj);

void chrobjMaybeDetonateObjectIfFlags(ObjectRecord *obj, f32 damage,
    coord3d *pos, ITEM_IDS item, s32 owner)
{
    if ((obj->flags2 & 0x4000) == 0)
        objApplyDamage(obj, damage, pos, item, owner);
}

bool objIsHealthy(ObjectRecord *obj)
{
    return objGetDestroyedLevel(obj) == 0;
}

u32 modelFindNextProjectileHitCandidate(Model *model, coord3d *pos,
    coord3d *dir, ModelNode **node)
{
    (void)model; (void)pos; (void)dir; (void)node;
    return HIT_NULL_PART;
}

void objDropRecursively(PropRecord *prop)
{
    /* Focused objects have no attachment children. */
    (void)prop;
}
void door7F0526EC(DoorRecord *door, Mtxf *matrix)
{
    (void)door; (void)matrix;
}

/* The focused binary does not construct the 3DS HUD backend. */
void ge_original_hud_bottom_show_exact(char *message) { (void)message; }
