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

static struct player players[4];
static PropRecord props[4];
static ChrRecord chrs[4];
static int current_slot;
struct player *g_CurrentPlayer=&players[0];
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
s32 get_cur_playernum(void){return current_slot;}
s32 fileGetBondForCurrentFolder(void){return folder_bond;}
f32 bondviewGetPlayerYawRadians(void){return 1.25f;}
ITEM_IDS get_item_in_hand_or_watch_menu(GUNHAND hand)
{assert(hand==GUNRIGHT);return held_item;}
void remove_item_in_hand(GUNHAND hand)
{assert(hand==GUNLEFT||hand==GUNRIGHT);++remove_calls;}
void bondviewDeregisterPlayerRoom(struct player *value)
{assert(value==&players[current_slot]);++deregister_calls;}
s32 getPropForHeldItem(ITEM_IDS item)
{return item==ITEM_UNARMED?-1:7;}

static int construct(void *context,struct player *value,int32_t body,
                     int32_t head,float yaw)
{
    assert(context==&construct_calls&&value==&players[current_slot]);
    ++construct_calls;constructed_body=body;constructed_head=head;
    constructed_yaw=yaw;
    if(construct_result)value->prop->chr=&chrs[current_slot];
    return construct_result;
}

static int attach(void *context,struct player *value,int32_t prop_id,
                  int32_t item_id,uint32_t flags)
{
    assert(context==&construct_calls&&value==&players[current_slot]);
    assert(prop_id==7&&item_id==ITEM_WPPK&&flags==0U);
    ++attach_calls;return 1;
}

static void prepare(int bondtype,int bond)
{
    current_slot=0;g_CurrentPlayer=&players[0];
    memset(&players[current_slot],0,sizeof(players[current_slot]));memset(&props[current_slot],0,sizeof(props[current_slot]));
    memset(&chrs[current_slot],0,sizeof(chrs[current_slot]));players[current_slot].prop=&props[current_slot];players[current_slot].bondtype=bondtype;
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
           &&remove_calls==2&&deregister_calls==1&&props[current_slot].chr==&chrs[current_slot]);
}

static void multiplayer_fixture(int count)
{
    static const int selected[4]={9,10,7,15}; /* Jaws, Oddjob, Mishkin, scientist. */
    static const int expected_body[4]={BODY_Jaws,BODY_Oddjob,
        BODY_Siberian_Guard_1_Mishkin,BODY_Scientist_2_Female};
    static const int expected_head[4]={HEAD_Male_Brosnan_Tuxedo,
        HEAD_Male_Brosnan_Tuxedo,HEAD_Male_Mishkin,HEAD_Female_Sally};
    GeOriginalPlayerBodySnapshot snapshot;
    prepare(CUFF_BLUE,BOND_BROSNAN);player_count=count;
    memset(players,0,sizeof(players));memset(props,0,sizeof(props));
    memset(chrs,0,sizeof(chrs));ge_original_player_body_reset();
    for(current_slot=0;current_slot<count;++current_slot){
        g_CurrentPlayer=&players[current_slot];players[current_slot].prop=&props[current_slot];
        assert(ge_original_player_body_set_character(current_slot,selected[current_slot]));
        solo_char_load();
        assert(constructed_body==expected_body[current_slot]
               &&constructed_head==expected_head[current_slot]);
        assert(props[current_slot].chr==&chrs[current_slot]&&remove_calls==0);
        assert(ge_original_player_body_snapshot_player(current_slot,&snapshot));
        assert(snapshot.body_id==expected_body[current_slot]
               &&snapshot.head_id==expected_head[current_slot]
               &&snapshot.successful_loads==1U);
        for(int other=0;other<current_slot;++other)
            assert(props[other].chr==&chrs[other]&&props[other].chr!=props[current_slot].chr);
    }
    assert(attach_calls==count&&construct_calls==count&&deregister_calls==count);
    /* A canonical teardown clears only the dying players[current_slot]'s chrs[current_slot] relation.
     * Reconstruction must keep the selected appearance and other players. */
    current_slot=count-1;g_CurrentPlayer=&players[current_slot];props[current_slot].chr=NULL;
    solo_char_load();
    assert(constructed_body==expected_body[current_slot]
           &&constructed_head==expected_head[current_slot]);
    assert(ge_original_player_body_snapshot_player(current_slot,&snapshot)
           &&snapshot.successful_loads==2U&&snapshot.held_item_frontiers==0U);
    for(int other=0;other<count-1;++other)
        assert(props[other].chr==&chrs[other]);
    assert(attach_calls==count+1&&remove_calls==0);
    assert(fabsf(get_player_mp_char_height(0)-1.199f)<0.0001f);
    assert(fabsf(get_player_mp_char_height(1)-0.78780001f)<0.0001f);
    if(count==4)assert(get_player_mp_char_gender(3)==FEMALE);
    for(current_slot=0;current_slot<count;++current_slot){
        g_CurrentPlayer=&players[current_slot];solo_char_load();
        assert(ge_original_player_body_snapshot_player(current_slot,&snapshot));
        assert(snapshot.body_id==expected_body[current_slot]
            &&snapshot.head_id==expected_head[current_slot]);
    }
    assert(attach_calls==count+1&&remove_calls==0);
    current_slot=0;g_CurrentPlayer=&players[current_slot];
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
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_OK
           &&construct_calls==1&&remove_calls==0&&deregister_calls==1
           &&constructed_body==BODY_Brosnan_Tuxedo
           &&constructed_head==HEAD_Male_Brosnan_Tuxedo);
    prepare(CUFF_BLUE,BOND_BROSNAN);props[current_slot].chr=&chrs[current_slot];
    ge_original_player_body_reset();solo_char_load();
    ge_original_player_body_snapshot(&snapshot);
    assert(snapshot.status==GE_ORIGINAL_PLAYER_BODY_OK&&construct_calls==1
           &&remove_calls==0&&deregister_calls==0);
    multiplayer_fixture(2);multiplayer_fixture(4);
    assert(!ge_original_player_body_set_character(-1,0));
    assert(!ge_original_player_body_set_character(4,0));
    assert(!ge_original_player_body_set_character(0,64));
    assert(!ge_original_player_body_set_character(0,-2));
    assert(!ge_original_player_body_snapshot_player(4,&snapshot));
    assert(ge_original_player_body_set_character(1,-1));
    assert(get_player_mp_char_body(1)==BODY_Natalya_Skirt);
    ge_original_player_body_unbind(&construct_calls);
    printf("canonical solo/two/four player selection, hand retention, ownership and reconstruction passed\n");
    return 0;
}
