/* Explicit resource frontiers for the PP7-only gun gameplay fixture.
 * These are never reached by a passing run. The original gun gameplay,
 * reload, inventory, audio service, model, matrices, beams and RNG are linked.
 * This fixture does not claim a world interaction or projectile acceptance. */
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "ge_original_guard_grenade_object.h"
#include "ge_original_covert_modem_object.h"
#include "game/stan.h"
#include "game/language.h"
#include <stdio.h>
#include <stdlib.h>

ModelFileHeader cartblue_header; /* Unused sniper casing resource. */
static void frontier(const char *name)
{
    fprintf(stderr,"unexpected gun gameplay resource frontier: %s\n",name);
    abort();
}
void ge_original_hud_bottom_show_exact(char *message)
{(void)message;frontier("HUD item text");}
u8 *langGet(s32 slot)
{(void)slot;frontier("language item text");return NULL;}
s32 walkTilesBetweenPoints_NoCallback(StandTile **tile,f32 x,f32 z,f32 dx,f32 dz)
{(void)tile;(void)x;(void)z;(void)dx;(void)dz;frontier("object interaction LOS");return 0;}
