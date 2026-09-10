/* Resource-only boundaries for unchanged hand/drop/free fixture bodies. */
#define PLAYERFLAG int
#define RUNTIMEBITFLAG_HASPROJECTILE (1u << 7)
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/propobj.h"
#include "game/chrai.h"
#include "game/model.h"
#include "game/player.h"
#include "game/bondinv.h"
#include "game/bondview.h"
#include "game/stan.h"
#include "snd.h"
#include <assert.h>
#include <string.h>

static Projectile projectiles[30];
static unsigned char inventory[ITEM_IDS_MAX];
static size_t free_calls;
static size_t grenade_fallback_calls;
coord3d ZeroCoord={0,0,0};

void lifecycle_inventory_set(ITEM_IDS item,int owned)
{assert(item>=0&&item<ITEM_IDS_MAX);inventory[item]=(unsigned char)owned;}
bool bondinvHasInvItem(ITEM_IDS item)
{return item>=0&&item<ITEM_IDS_MAX&&inventory[item]!=0U;}
size_t lifecycle_free_calls(void){return free_calls;}
size_t lifecycle_grenade_fallback_calls(void){return grenade_fallback_calls;}
void *ge_original_guard_grenade_object_create(void *chr,int32_t model,int32_t item,int32_t flags)
{(void)chr;(void)model;(void)item;(void)flags;++grenade_fallback_calls;return NULL;}
void lifecycle_deregister_rooms(PropRecord *prop)
{assert(prop!=NULL);prop->rooms[0]=UINT8_MAX;}
void lifecycle_free_prop(PropRecord *prop)
{assert(prop!=NULL&&prop->parent==NULL);++free_calls;}
void sub_GAME_7F03FDA8(PropRecord *prop)
{
    assert(prop!=NULL&&prop->obj!=NULL);
    for(size_t i=0;i<30U;++i)if(projectiles[i].obj==NULL){
        memset(&projectiles[i],0,sizeof(projectiles[i]));
        projectiles[i].obj=prop->obj;prop->obj->projectile=&projectiles[i];
        prop->obj->runtime_bitflags|=RUNTIMEBITFLAG_HASPROJECTILE;return;
    }
    assert(!"projectile resource fixture exhausted");
}
void projectileFree(Projectile *projectile)
{assert(projectile!=NULL);projectile->obj=NULL;}
void embedmentFree(Embedment *embedment)
{(void)embedment;assert(!"fixture has no embedded weapons");}
void explosionClearBulletImpactRoomByFlag(PropRecord *prop,s8 flag)
{assert(prop!=NULL);(void)flag;}
void sub_GAME_7F050DE8(Model *model)
{assert(model!=NULL);}
u8 sndGetPlayingState(ALSoundState *state)
{(void)state;assert(!"weapon fixture has no sound handles");return 0;}
void sndDeactivate(ALSoundState *state)
{(void)state;assert(!"weapon fixture has no sound handles");}
void bgToggleDataPortalsContrlBytes1Bit1(s32 portal,s32 flag)
{(void)portal;(void)flag;assert(!"weapon fixture has no tinted glass");}
PropRecord *getCurrentPlayerProp(void)
{return g_CurrentPlayer->prop;}
Mtxf *currentPlayerGetViewToWorldMtxf(void)
{assert(!"drop fixture uses canonical offscreen branch");return NULL;}
f32 getsubroty(Model *model)
{(void)model;assert(!"drop fixture uses default player drop");return 0.0f;}
void sub_GAME_7F03D058(PropRecord *prop,bool unset)
{(void)prop;(void)unset;assert(!"drop fixture uses canonical offscreen branch");}
void sub_GAME_7F0402B4(PropRecord *prop,rgba_u8 *color)
{assert(prop!=NULL&&color!=NULL);color->r=color->g=color->b=255U;color->a=0;}
void setupUpdateObjectRoomPosition(ObjectRecord *object)
{assert(object!=NULL&&object->prop!=NULL&&object->prop->stan!=NULL);object->prop->rooms[0]=object->prop->stan->room;}
