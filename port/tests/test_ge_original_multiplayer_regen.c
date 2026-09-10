#include "ge_original_multiplayer_regen.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

struct player *g_playerPointers[4];
static struct player players[4];
static PropRecord viewers[4];
static int player_count;
static unsigned rooms_removed, enabled, room_updates, sounds;
s32 getPlayerCount(void) { return player_count; }
void chrpropDeregisterRooms(PropRecord *prop) { assert(prop);++rooms_removed; }
void chrpropDelist(PropRecord *prop) { (void)prop;assert(!"regenerating props stay listed"); }
void chrpropFree(PropRecord *prop) { (void)prop;assert(!"regenerating props retain storage"); }
void objDetach(PropRecord *prop) { (void)prop;assert(!"root pickup has no owner"); }
void objFreeEmbedmentOrProjectile(PropRecord *prop) { (void)prop;assert(!"ordinary pickup has no projectile"); }
void chrpropReparent(PropRecord *prop,PropRecord *parent) { (void)prop;(void)parent;assert(!"FREE does not attach"); }
PropRecord *getCurrentPlayerProp(void) { assert(!"FREE does not select a player");return NULL; }
static void enable(void *context,void *prop) { (void)context;chrpropEnable(prop);++enabled; }
static void update_rooms(void *context,void *prop) { (void)context;assert(prop);++room_updates; }
static void play_sound(void *context,const float p[3]) { (void)context;assert(isfinite(p[0]));++sounds; }
static const GeOriginalMultiplayerRegenProviders providers={NULL,enable,update_rooms,play_sound};
extern void propExecuteTickOperation(PropRecord *,TICKOP);

static void lifecycle(int count,int type)
{
    BodyArmourRecord storage={0};
    ObjectRecord *obj=(void *)&storage;
    PropRecord prop={0};
    player_count=count;
    rooms_removed=enabled=room_updates=sounds=0;
    memset(players,0,sizeof(players));memset(viewers,0,sizeof(viewers));
    for(int i=0;i<count;++i){g_playerPointers[i]=&players[i];players[i].prop=&viewers[i];viewers[i].pos.x=1000+i*1000;}
    obj->type=type;obj->state=PROPSTATE_RESPAWN;obj->prop=&prop;
    storage.initialamount=0.75f;storage.amount=0.1f;
    prop.type=type==PROPDEF_COLLECTABLE?PROP_TYPE_WEAPON:PROP_TYPE_OBJ;
    prop.obj=obj;prop.flags=PROPFLAG_ENABLED;
    propExecuteTickOperation(&prop,TICKOP_FREE);
    assert(prop.timetoregen==1200 && !(prop.flags&PROPFLAG_ENABLED));
    assert(obj->runtime_bitflags&RUNTIMEBITFLAG_00000800);
    assert(rooms_removed==1);
    assert(ge_original_multiplayer_regen_tick(&prop,1140,&providers)==GE_ORIGINAL_MP_REGEN_OK);
    assert(prop.timetoregen==60 && !enabled);
    /* Every player independently defers the crossing, including slot 3. */
    for(int i=0;i<count;++i){
        float x=viewers[i].pos.x;viewers[i].pos.x=399.0f;
        assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_OK);
        assert(prop.timetoregen==119 && !enabled);
        viewers[i].pos.x=x;
        assert(ge_original_multiplayer_regen_tick(&prop,59,&providers)==GE_ORIGINAL_MP_REGEN_OK);
        assert(prop.timetoregen==60 && !enabled);
    }
    viewers[count-1].pos.x=400.0f; /* Exact boundary does not defer. */
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_OK);
    assert(prop.timetoregen==59 && (prop.flags&PROPFLAG_ENABLED));
    assert(!(obj->runtime_bitflags&RUNTIMEBITFLAG_00000800));
    assert(enabled==1 && room_updates==1 && sounds==1);
    if(type==PROPDEF_ARMOUR)assert(storage.amount==storage.initialamount);
    obj->state|=PROPSTATE_10;
    assert(ge_original_multiplayer_regen_tick(&prop,59,&providers)==GE_ORIGINAL_MP_REGEN_OK);
    assert(prop.timetoregen==0 && (obj->runtime_bitflags&RUNTIMEBITFLAG_00001000));
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_IDLE);
    /* A second pickup uses the same object storage and the original reset. */
    propExecuteTickOperation(&prop,TICKOP_FREE);
    obj->state&=~PROPSTATE_10;
    prop.timetoregen=1;
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_OK);
    assert(!(obj->runtime_bitflags&RUNTIMEBITFLAG_00001000));
    prop.timetoregen=60;
    assert(ge_original_multiplayer_regen_tick(&prop,1,NULL)==GE_ORIGINAL_MP_REGEN_MISSING_PROVIDER);
    assert(prop.timetoregen==60);
    g_playerPointers[count-1]=NULL;
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_INVALID_ROSTER);
    assert(prop.timetoregen==60);g_playerPointers[count-1]=&players[count-1];
    viewers[0].pos.x=NAN;
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_INVALID_ROSTER);
    assert(prop.timetoregen==60);viewers[0].pos.x=1000;
    obj->flags|=PROPFLAG_INSIDEANOTHEROBJ;
    assert(ge_original_multiplayer_regen_tick(&prop,1,&providers)==GE_ORIGINAL_MP_REGEN_UNSUPPORTED_OBJECT);
    assert(prop.timetoregen==60);obj->flags&=~PROPFLAG_INSIDEANOTHEROBJ;
    assert(ge_original_multiplayer_regen_tick(&prop,-1,&providers)==GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT);
    assert(ge_original_multiplayer_regen_tick(&prop,32768,&providers)==GE_ORIGINAL_MP_REGEN_INVALID_ARGUMENT);
}
int main(void)
{
    for(int n=2;n<=4;n+=2){lifecycle(n,PROPDEF_COLLECTABLE);lifecycle(n,PROPDEF_AMMO);lifecycle(n,PROPDEF_ARMOUR);}
    puts("canonical 2/4-player pickup regeneration: FREE operation,1200 ticks,all-player proximity,400-unit boundary,armour refill and repeated collection passed");
    return 0;
}
