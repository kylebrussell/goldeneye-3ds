#include "ge_original_multiplayer_spawn.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_rules.h"
#include "ge_original_prop_state.h"
#include "ge_original_player_body.h"
#include "ge_original_bond_input_provider.h"
#include "ge_original_input.h"
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/bondinv.h"
#include "game/chrObjRandom.h"
#include "game/chrai.h"
#include <random.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

stagesetup g_CurrentSetup;
s32 scenario;
static const GeStanNativeMap *current_stan;
static unsigned equip_calls[4];
static int life_ready;
static Model body_models[4];
extern s32 startpadcount;
extern PadRecord *g_Startpad[16];
extern s32 player_char[4];
extern void ge_mp_spawn_reference(GeOriginalPlayerSpawnConfig *,s32 *);
extern void bondinvReinitInv(void);
extern s32 bondinvHasInvItem(ITEM_IDS);
MPSCENARIOS get_scenario(void) {return scenario;}
s32 getMissiontimer(void) {return 123;}
s32 currentPlayerEquipWeaponWrapper(GUNHAND hand,s32 weapon)
{assert(hand==GUNLEFT||hand==GUNRIGHT);assert(weapon==ITEM_FIST||weapon==ITEM_UNARMED);++equip_calls[get_cur_playernum()];return 1;}
void solo_char_load(void) {assert(!"fixture supplies body resources before BeginLife");}
f32 bondviewYPositionRelated(StandTile *stan,f32 x,f32 z)
{return ge_original_stan_get_position_y(current_stan,(const GeStanNativeTile *)stan,x,z);}
static float floor_y(void *context,void *stan,float x,float z)
{(void)context;return bondviewYPositionRelated(stan,x,z);}
static int ready(void *context,uint32_t slot)
{(void)context;assert(slot==(uint32_t)get_cur_playernum());return life_ready;}
static void deregister(void *context,void *prop,int16_t room)
{(void)context;if(room>=0)chrpropDeregisterRoom(prop,room);}

static void run(GeAssetPack *pack,size_t arena,uint32_t count)
{
    GeOriginalStageSetupRuntime stage={0};
    const GeStageAssetDescriptor *descriptor=ge_stage_asset_multiplayer(arena);
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
    GeOriginalPropState props;
    GeOriginalPlayerViewState views[4]={{0}};
    GeOriginalMultiplayerSpawnResult results[4]={{0}};
    GeOriginalPlayerSpawnProviders player_providers={&props,
        ge_original_prop_state_allocate_player,ge_original_prop_state_activate,
        ge_original_prop_state_enable,deregister,ge_original_prop_state_register_room};
    GeOriginalMultiplayerSpawnProviders providers={&stage,&player_providers,NULL,floor_y,ready};
    GeOriginalMultiplayerConfig config={(int)count,SCENARIO_NORMAL,1,descriptor->level_id,{0,0,1,1}};
    const GeAssetPackEntry *entry=ge_asset_pack_find(pack,descriptor->collision_path);
    void *collision,*storage;
    size_t bytes;
    uint64_t chr_seed=UINT64_C(0xabcdef2345678910);
    assert(entry && entry->data_size<SIZE_MAX);
    collision=malloc((size_t)entry->data_size);assert(collision);
    assert(ge_asset_pack_read(pack,descriptor->collision_path,collision,(size_t)entry->data_size,NULL)==GE_ASSET_PACK_OK);
    assert(ge_stan_collision_open(collision,(size_t)entry->data_size,&surface)==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface,&bytes)==GE_STAN_COLLISION_OK);
    storage=malloc(bytes);assert(storage);
    assert(ge_stan_native_materialize(&surface,descriptor->level_scale,storage,bytes,&native)==GE_STAN_COLLISION_OK);
    current_stan=&native;
    assert(ge_original_stage_setup_load(pack,descriptor,&stage)==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_bind_stan(&stage,&native)==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_publish(&stage));
    assert(ge_original_prop_state_reset(&props,256));
    assert(ge_original_multiplayer_players_create(count,count-1,UINT64_C(0x123456789abcdeff),chr_seed));
    assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);
    ge_original_multiplayer_bind_intro_slot(0,0,0);
    assert(ge_original_multiplayer_publish_spawns(stage.intro_storage,stage.intro_word_count,stage.pads_storage,stage.pad_count,0)==GE_ORIGINAL_MP_OK);
    memset(equip_calls,0,sizeof(equip_calls));
    /* Use an authored non-default perspective (Jaws), without inventing height. */
    player_char[count-1]=9;
    for(uint32_t slot=0;slot<count;++slot){
        GeOriginalPlayerSpawnConfig expected;
        int expected_index,cursor;
        uint64_t before_rng,expected_rng;
        float field70,initial_eye;
        assert(ge_original_multiplayer_select_player(slot));
        assert(g_CurrentPlayer->prop==NULL);
        initial_eye=g_CurrentPlayer->eyeheight;assert(initial_eye==1.0f);
        g_CurrentPlayer->num_suicides=17+(s32)slot;
        g_CurrentPlayer->kills_this_life=5;
        g_CurrentPlayer->hands[0].weapon_ammo_in_magazine=9+(s32)slot;
        g_playerPerm->kill_counts[0]=6+(s32)slot;
        g_playerPerm->have_token_or_goldengun=slot;
        before_rng=g_randomSeed;cursor=g_CurrentPlayer->field_29E0;field70=g_CurrentPlayer->field_70;
        if(slot==0){
            const int32_t terminal=stage.intro_storage[stage.intro_word_count-1];
            assert(ge_original_multiplayer_spawn_initial(1,&providers,&views[0],&results[0])==GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER);
            stage.intro_storage[stage.intro_word_count-1]=INTROTYPE_AMMO;
            assert(ge_original_multiplayer_spawn_initial(slot,&providers,&views[0],&results[0])==GE_ORIGINAL_MP_SPAWN_INVALID_SETUP);
            stage.intro_storage[stage.intro_word_count-1]=terminal;
            g_CurrentPlayer->field_29E0=INT_MAX;
            assert(ge_original_multiplayer_spawn_initial(slot,&providers,&views[0],&results[0])==GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER);
            g_CurrentPlayer->field_29E0=-2;
            assert(ge_original_multiplayer_spawn_initial(slot,&providers,&views[0],&results[0])==GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER);
            g_CurrentPlayer->field_29E0=cursor;
            assert(g_randomSeed==before_rng && g_CurrentPlayer->prop==NULL);
        }
        ge_mp_spawn_reference(&expected,&expected_index);
        expected_rng=g_randomSeed;
        const int expected_cursor=g_CurrentPlayer->field_29E0;
        g_randomSeed=before_rng;g_CurrentPlayer->field_29E0=cursor;g_CurrentPlayer->field_70=field70;
        assert(ge_original_multiplayer_spawn_initial(slot,&providers,&views[slot],&results[slot])==GE_ORIGINAL_MP_SPAWN_OK);
        assert(results[slot].cursor_before==cursor && results[slot].cursor_after==expected_cursor);
        assert(results[slot].spawn_index==expected_index && g_randomSeed==expected_rng);
        assert(g_chrObjRandomSeed==chr_seed);
        if(expected_rng==before_rng){
            for(uint32_t other=0;other<slot;++other)
                assert(results[other].placement.position[0]!=results[slot].placement.position[0]
                    || results[other].placement.position[2]!=results[slot].placement.position[2]);
        }
        assert(memcmp(results[slot].placement.position,expected.position,sizeof(expected.position))==0);
        assert(results[slot].placement.floor_y==expected.floor_y && results[slot].placement.stan==expected.stan);
        assert(results[slot].placement.look_angle_radians==expected.look_angle_radians);
        assert(g_CurrentPlayer->eyeheight==initial_eye);
        assert(g_playerPerm->player_perspective_height==get_player_mp_char_height(slot));
        assert(g_CurrentPlayer->num_suicides==17+(s32)slot && g_CurrentPlayer->kills_this_life==5);
        assert(g_CurrentPlayer->hands[0].weapon_ammo_in_magazine==9+(s32)slot);
        assert(g_playerPerm->kill_counts[0]==6+(s32)slot && g_playerPerm->have_token_or_goldengun==(s32)slot);
        assert(g_CurrentPlayer->prop==results[slot].prop && views[slot].prop==results[slot].prop);
        assert(ge_original_prop_state_is_active(results[slot].prop));
        assert(ge_original_prop_state_is_enabled(results[slot].prop));
        assert(ge_original_prop_state_room_contains(views[slot].room,results[slot].prop));
        assert(views[slot].allocation_calls==1 && views[slot].activation_calls==1 && views[slot].registration_calls==1);
        assert(get_cur_playernum()==(s32)slot && ge_original_bond_input_provider()->current_player==g_CurrentPlayer);
        assert(ge_original_multiplayer_spawn_initial(slot,&providers,&views[slot],&results[slot])==GE_ORIGINAL_MP_SPAWN_INVALID_PLAYER);
        assert(ge_original_multiplayer_player_bind_inventory(slot));bondinvReinitInv();
        life_ready=0;
        assert(ge_original_multiplayer_spawn_finalize(&providers,&results[slot])==GE_ORIGINAL_MP_SPAWN_LIFE_NOT_READY);
        assert(g_CurrentPlayer->kills_this_life==5 && !equip_calls[slot]);
        g_CurrentPlayer->bodyModel=&body_models[slot];life_ready=1;
        assert(ge_original_multiplayer_spawn_finalize(&providers,&results[slot])==GE_ORIGINAL_MP_SPAWN_OK);
        assert(g_CurrentPlayer->eyeheight==get_player_mp_char_height(slot)*185.0f-10.0f);
        assert(g_CurrentPlayer->kills_this_life==0 && g_CurrentPlayer->lifestarttime60==123);
        assert(bondinvHasInvItem(ITEM_FIST) && equip_calls[slot]==2);
        assert(g_playerPerm->kill_counts[0]==6+(s32)slot && g_CurrentPlayer->num_suicides==17+(s32)slot);
        assert(g_randomSeed==expected_rng && g_chrObjRandomSeed==chr_seed);
        assert(ge_original_multiplayer_spawn_finalize(&providers,&results[slot])==GE_ORIGINAL_MP_SPAWN_INVALID_ARGUMENT);
    }
    assert(props.allocation_calls==count && props.activation_calls==count && props.enable_calls==count);
    assert(props.room_registration_calls==count && ge_original_prop_state_active_count()==count);
    printf("%s %u players: original initial selection, native props/rooms, separate BeginLife; pads",descriptor->key,count);
    for(uint32_t slot=0;slot<count;++slot)printf(" %d",results[slot].pad_index);
    puts("");
    player_char[count-1]=-1;
    ge_original_multiplayer_players_destroy();ge_original_stage_setup_close(&stage);
    free(storage);free(collision);current_stan=NULL;
}
int main(int argc,char **argv)
{
    GeAssetPack pack;
    assert(argc==2 && ge_asset_pack_open(&pack,argv[1])==GE_ASSET_PACK_OK);
    for(size_t arena=0;arena<GE_MULTIPLAYER_STAGE_COUNT;++arena){
        run(&pack,arena,2);
        if(ge_stage_asset_multiplayer_max_players(arena)==4)run(&pack,arena,4);
    }
    ge_asset_pack_close(&pack);return 0;
}
