#include "ge_original_player_body.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/player.h"

static struct player player;
static PropRecord prop;
static ChrRecord chr;
struct player *g_CurrentPlayer=&player;
enum CAMERAMODE g_CameraMode=CAMERAMODE_NONE;
ITEM_IDS starting_weapon[2]={ITEM_UNARMED,ITEM_WPPK};
static int player_count=1;
static int folder_bond=BOND_BROSNAN;
static ITEM_IDS held_item=ITEM_WPPK;
static int remove_calls,deregister_calls,construct_calls,construct_result=1;
static int attach_calls;
static int32_t constructed_body,constructed_head;
static float constructed_yaw;

s32 getPlayerCount(void){return player_count;}
s32 fileGetBondForCurrentFolder(void){return folder_bond;}
f32 bondviewGetPlayerYawRadians(void){return 1.25f;}
ITEM_IDS get_item_in_hand_or_watch_menu(GUNHAND hand)
{assert(hand==GUNRIGHT);return held_item;}
void remove_item_in_hand(GUNHAND hand)
{assert(hand==GUNLEFT||hand==GUNRIGHT);++remove_calls;}
void bondviewDeregisterPlayerRoom(struct player *value)
{assert(value==&player);++deregister_calls;}
s32 getPropForHeldItem(ITEM_IDS item)
{return item==ITEM_UNARMED?-1:7;}

static int construct(void *context,struct player *value,int32_t body,
                     int32_t head,float yaw)
{
    assert(context==&construct_calls&&value==&player);
    ++construct_calls;constructed_body=body;constructed_head=head;
    constructed_yaw=yaw;
    if(construct_result)value->prop->chr=&chr;
    return construct_result;
}

static int attach(void *context,struct player *value,int32_t prop_id,
                  int32_t item_id,uint32_t flags)
{
    assert(context==&construct_calls&&value==&player);
    assert(prop_id==7&&item_id==ITEM_WPPK&&flags==0U);
    ++attach_calls;return 1;
}

static void prepare(int bondtype,int bond)
{
    memset(&player,0,sizeof(player));memset(&prop,0,sizeof(prop));
    memset(&chr,0,sizeof(chr));player.prop=&prop;player.bondtype=bondtype;
    folder_bond=bond;held_item=ITEM_WPPK;player_count=1;
    remove_calls=0;deregister_calls=0;construct_calls=0;construct_result=1;
    attach_calls=0;
    constructed_body=-1;constructed_head=-1;constructed_yaw=0.0f;
    g_CameraMode=CAMERAMODE_POSEND;
}

static void expect(int bondtype,int bond,int body,int head)
{
    GeOriginalPlayerBodySnapshot snapshot;
    prepare(bondtype,bond);ge_original_player_body_reset();solo_char_load();
    ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_OK
           &&snapshot.load_requests==1U&&snapshot.successful_loads==1U
           &&snapshot.held_item_frontiers==1U
           &&snapshot.body_id==body&&snapshot.head_id==head);
    assert(construct_calls==1&&constructed_body==body
           &&constructed_head==head&&fabsf(constructed_yaw-1.25f)<0.0001f
           &&remove_calls==2&&deregister_calls==1&&prop.chr==&chr);
}

int main(void)
{
    GeOriginalPlayerBodySnapshot snapshot;
    prepare(CUFF_BLUE,BOND_BROSNAN);ge_original_player_body_reset();
    solo_char_load();ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_UNBOUND);
    ge_original_player_body_bind(&construct_calls,construct);
    expect(CUFF_BLUE,BOND_BROSNAN,
        BODY_Formal_Wear,HEAD_Male_Brosnan_Default);
    expect(CUFF_BOILER,BOND_BROSNAN,
        BODY_Special_Operations_Uniform,HEAD_Male_Brosnan_Boiler);
    expect(CUFF_JUNGLE,BOND_BROSNAN,
        BODY_Jungle_Fatigues,HEAD_Male_Brosnan_Jungle);
    expect(CUFF_SNOW,BOND_BROSNAN,
        BODY_Parka,HEAD_Male_Brosnan_Default);
    expect(CUFF_FOLDER,BOND_CONNERY,
        BODY_Brosnan_Tuxedo,HEAD_Male_Brosnan_Tuxedo);
    ge_original_player_body_bind_held_item(&construct_calls,attach);
    prepare(CUFF_BLUE,BOND_BROSNAN);ge_original_player_body_reset();
    solo_char_load();ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_OK
           &&snapshot.held_item_frontiers==0U&&attach_calls==1);
    prepare(CUFF_BLUE,BOND_BROSNAN);player_count=2;
    ge_original_player_body_reset();solo_char_load();
    ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE
           &&construct_calls==0&&remove_calls==0&&deregister_calls==0);
    prepare(CUFF_BLUE,BOND_BROSNAN);prop.chr=&chr;
    ge_original_player_body_reset();solo_char_load();
    ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_OK&&construct_calls==1
           &&remove_calls==0&&deregister_calls==0);
    ge_original_player_body_unbind(&construct_calls);
    printf("canonical solo player body selection/lifecycle adapter passed\n");
    return 0;
}
