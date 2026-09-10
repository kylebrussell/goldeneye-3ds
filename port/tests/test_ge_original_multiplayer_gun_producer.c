/* Actual source-extracted gunUpdateAndFireBothHands with ROM PP7 models.
 * This is a producer/resource acceptance fixture, not a full match: hand
 * Default mode supplies firing state at the prior gunTickGameplay boundary;
 * GE_TEST_GUN_GAMEPLAY retains original trigger/hand state and reload too.
 * Projectile alternatives fail if reached; no invented projectile physics. */
#include "ge_original_first_person_assets.h"
#include "ge_original_gun_live.h"
#include "ge_original_bond_input_provider.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/gun.h"
#include "game/model.h"
#include "game/matrixmath.h"
#include <random.h>
#ifdef GE_TEST_GUN_GAMEPLAY
#include "ge_original_gameplay_services.h"
#include "ge_stan_native.h"
#include "ge_stage_assets.h"
static GeStanNativeMap arena;
#endif

extern struct hand D_8003FDA0;
extern WeaponStats wppk_stats,wppksil_stats;
extern ModelFileHeader wppk_header,wppksil_header;
extern CasingRecord g_Casings[20];
GunModelFileRecord gitem_structs[ITEM_IDS_MAX];
ModelFileHeader cartridge_header; /* MP never dereferences this solo resource. */
struct player *ge_original_spawn_player_get(void) {return g_CurrentPlayer;}
/* Unsupported weapon branches are not gameplay substitutes. */
void ge_original_generate_player_thrown_object_exact(s32 hand) {(void)hand;abort();}
void generate_player_thrown_grenade(GUNHAND hand) {(void)hand;abort();}
void generate_player_thrown_knife(GUNHAND hand) {(void)hand;abort();}
void gunFireTankShell(s32 hand) {(void)hand;abort();}
void gunSpawnGLGrenade(GUNHAND hand) {(void)hand;abort();}
void gunUpdateAttachedRocket(s32 hand) {(void)hand;abort();}

static uint64_t matrix_hash(const GeOriginalGunLiveHand *hand)
{
    const unsigned char *bytes=(const unsigned char *)hand->matrices;
    size_t size=hand->matrix_count*sizeof(*hand->matrices);
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*UINT64_C(1099511628211);
    return hash;
}

static void run(GeAssetPack *pack,unsigned count)
{
    struct player *players=calloc(count,sizeof(*players));
    PropRecord props[4]={{0}};
    ChrRecord chrs[4]={{0}};
    struct player_data perms[4]={{0}},*permissions[4];
    struct player *pointers[4];
    GeOriginalFirstPersonAssets assets[4]={{0}};
    unsigned char *buffers[4][2]={{0}};
    Mtxf matrices[4];
    GeOriginalGunLiveHand publications[4][2];
    uint64_t matrix_hashes[4][2];
    void *flash_rw[4][2];
    unsigned shots[4][2]={{0}};
    int flash_expected[4][2]={{0}};
#ifdef GE_TEST_GUN_GAMEPLAY
    int saw_reload[4][2]={{0}};
#endif
    assert(players);
    for(unsigned slot=0;slot<count;++slot){pointers[slot]=&players[slot];permissions[slot]=&perms[slot];}
    assert(ge_original_bond_input_bind_roster(pointers,permissions,count));
    for(unsigned slot=0;slot<count;++slot){
        struct player *player=&players[slot];
        g_CurrentPlayer=player;
        /* Native owner records are fixture resources. The prior body/held
         * item pass supplies world muzzle positions and hit endpoints. */
        player->prop=&props[slot];props[slot].chr=&chrs[slot];
        chrs[slot].prop=&props[slot];
#ifdef GE_TEST_GUN_GAMEPLAY
        props[slot].stan=(StandTile *)arena.spawn_tile;
        player->magnetattracttime=-1;
        player->ammoheldarr[AMMO_9MM]=40;
#endif
        for(unsigned hand=0;hand<2;++hand){buffers[slot][hand]=malloc(GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE);assert(buffers[slot][hand]);}
        assert(ge_original_first_person_assets_init(&assets[slot],pack,
            buffers[slot][0],GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE,
            buffers[slot][1],GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE)==GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
        matrix_4x4_set_identity(&matrices[slot]);
        player->viewtoworldmtxf=player->field_10CC=&matrices[slot];
        player->c_screenwidth=320;player->c_screenheight=240;
        player->c_halfwidth=160;player->c_halfheight=120;
        player->c_perspaspect=4.0f/3.0f;player->fovy=60;
        player->field_FFC.x=160;player->field_FFC.y=120;
        player->cur_item_weapon_getname=ITEM_WPPK;
        for(unsigned hand=0;hand<2;++hand){
            void *header=NULL;ITEM_IDS item=hand?ITEM_WPPKSIL:ITEM_WPPK;
            gitem_structs[item].item_header=hand?&wppksil_header:&wppk_header;
            gitem_structs[item].item_file_name=hand?"GwppksilZ":"GwppkZ";
            assert(ge_original_first_person_assets_load_item_native(&assets[slot],hand,item,&header)==GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
            player->copy_of_body_obj_header[hand]=*(ModelFileHeader *)header;
            gitem_structs[item].item_header=header;
            gitem_structs[item].item_weapon_stats=hand?&wppksil_stats:&wppk_stats;
            player->hands[hand]=D_8003FDA0;
            player->hands[hand].weaponnum=item;
            player->hands[hand].weaponnum_watchmenu=-1;
            player->hands[hand].weapon_ammo_in_magazine=7;
            player->hands[hand].item_related.z=-1000;
            player->field_2A18[hand].x=hand?-1:1;
            player->hand_invisible[hand]=1;
            player->hand_item[hand]=item;
        }
    }
    ge_original_gun_live_reset();g_randomSeed=UINT64_C(0x123456789abcdef0);
#ifdef GE_TEST_GUN_GAMEPLAY
    ge_original_gameplay_services_reset();
    ge_original_gameplay_services_set_exact_held_item_dispatch(1);
    const unsigned tick_count=420;
#else
    const unsigned tick_count=4;
#endif
    for(unsigned tick=0;tick<tick_count;++tick){
        assert(ge_original_gun_live_frame_begin());
        for(unsigned slot=0;slot<count;++slot){
            g_CurrentPlayer=&players[slot];
            ge_original_bond_input_set_timers(1,(int32_t)tick,1);
#ifdef GE_TEST_GUN_GAMEPLAY
            int locked=tick>=40&&tick<55;
            int menu=tick>=55&&tick<70;
            int trigger=tick>=slot*8&&tick<310;
            ge_original_bond_input_provider()->controls_locked=locked;
            players[slot].mpmenuon=menu;
            gunTickGameplay(trigger);
            assert(players[slot].trigger_down==trigger);
            for(unsigned hand=0;hand<2;++hand){
                if(locked||menu)assert(!players[slot].hands[hand].weapon_firing_status);
                if(players[slot].hands[hand].weapon_firing_status)++shots[slot][hand];
                flash_expected[slot][hand]=players[slot].hands[hand].field_87D!=0;
                int action=players[slot].hands[hand].weapon_action_state;
                if(action>=GUN_ANIM_STATE_RELOAD_START&&action<=GUN_ANIM_STATE_RELOAD_RAISE)
                    saw_reload[slot][hand]=1;
            }
#else
            for(unsigned hand=0;hand<2;++hand){
                int firing=(int)((tick+slot+hand)&1U);
                players[slot].hands[hand].weapon_firing_status=(s8)firing;
                players[slot].hands[hand].field_87D=(s8)firing;
                players[slot].hands[hand].volley=1;
                flash_expected[slot][hand]=firing;
                shots[slot][hand]+=(unsigned)firing;
            }
#endif
            assert(ge_original_gun_live_tick());
            for(unsigned hand=0;hand<2;++hand){
                assert(ge_original_gun_live_hand_snapshot(hand,&publications[slot][hand]));
                assert(publications[slot][hand].visible&&publications[slot][hand].matrix_count);
                Model *model=(Model *)publications[slot][hand].model;
                ModelNode *flash=model->obj->Switches[1];
                assert(flash&&model->obj->Switches[3]);
                flash_rw[slot][hand]=modelGetNodeRwData(model,flash);
                assert(modelGetNodeRwData(model,flash)->Switch.visible==flash_expected[slot][hand]);
                for(size_t m=0;m<publications[slot][hand].matrix_count;++m)
                    for(unsigned a=0;a<4;++a)for(unsigned b=0;b<4;++b)
                        assert(isfinite(publications[slot][hand].matrices[m][a][b]));
                matrix_hashes[slot][hand]=matrix_hash(&publications[slot][hand]);
            }
        }
        for(unsigned slot=0;slot<count;++slot)for(unsigned hand=0;hand<2;++hand)
            for(unsigned other=0;other<count;++other)for(unsigned oh=0;oh<2;++oh)
                if(slot!=other||hand!=oh){assert(publications[slot][hand].model!=publications[other][oh].model);assert(publications[slot][hand].matrices!=publications[other][oh].matrices);assert(flash_rw[slot][hand]!=flash_rw[other][oh]);}
        /* Every possible local viewport and repeated eye capture consumes the
         * same produced banks without advancing gameplay RNG. Later shooters
         * cannot overwrite an earlier hand's matrices or switch visibility. */
        uint64_t seed=g_randomSeed;
        for(unsigned local=0;local<count;++local){
            g_CurrentPlayer=&players[local];
            for(unsigned eye=0;eye<2;++eye)for(unsigned hand=0;hand<2;++hand){
                GeOriginalGunLiveHand capture;
                assert(ge_original_gun_live_hand_snapshot(hand,&capture));
                assert(capture.model==publications[local][hand].model);
                assert(matrix_hash(&capture)==matrix_hashes[local][hand]);
                Model *model=(Model *)capture.model;
                assert(modelGetNodeRwData(model,model->obj->Switches[1])->Switch.visible==flash_expected[local][hand]);
            }
        }
        assert(g_randomSeed==seed);
        assert(ge_original_gun_live_frame_finalize(NULL));
    }
    for(unsigned slot=0;slot<count;++slot)for(unsigned hand=0;hand<2;++hand){
        assert(shots[slot][hand]>=2);
#ifndef GE_TEST_GUN_GAMEPLAY
        assert(players[slot].hands[hand].field_8A0==2);
#endif
        assert(chrs[slot].beams[hand].item_id==(hand?ITEM_WPPKSIL:ITEM_WPPK));
        assert(chrs[slot].beams[hand].unk1c>900);
        assert(chrs[slot].beams[hand].pos.x==players[slot].field_2A18[hand].x);
        for(unsigned axis=0;axis<3;++axis)assert(isfinite(chrs[slot].beams[hand].delta.f[axis]));
    }
    for(unsigned i=0;i<20;++i)assert(g_Casings[i].header==NULL);
#ifdef GE_TEST_GUN_GAMEPLAY
    for(unsigned slot=0;slot<count;++slot){
        int remaining=players[slot].ammoheldarr[AMMO_9MM]
            +players[slot].hands[0].weapon_ammo_in_magazine
            +players[slot].hands[1].weapon_ammo_in_magazine;
        assert(remaining==54-(int)(shots[slot][0]+shots[slot][1]));
        assert(saw_reload[slot][0]&&saw_reload[slot][1]);
        assert(players[slot].ammoheldarr[AMMO_9MM]<40);
        printf("actual gun gameplay %u players slot%u: %u/%u right/left shots, %d total rounds, both reload; lock/menu suppress firing while raw trigger persists\n",count,slot,shots[slot][0],shots[slot][1],remaining);
    }
    GeOriginalGameplayServiceStats service_stats;
    ge_original_gameplay_services_snapshot(&service_stats);
    assert(service_stats.sound_play_calls>0&&!service_stats.unsupported_object_calls);
    ge_original_gameplay_services_reset();
#else
    printf("actual %u-player gun producer: both native PP7 hands, distinct model/matrix/RW banks, idle/fire flashes, multiplayer beams, durable all-local eye snapshots without RNG; prior firing inputs supplied; no match lifecycle claim\n",count);
#endif
    ge_original_bond_input_unbind_roster();g_CurrentPlayer=NULL;
    for(unsigned slot=0;slot<count;++slot){ge_original_first_person_assets_close(&assets[slot]);free(buffers[slot][0]);free(buffers[slot][1]);}
    free(players);
}

int main(int argc,char **argv)
{
    GeAssetPack pack;
#ifdef GE_TEST_GUN_GAMEPLAY
    GeAssetPack arena_pack;
    GeStanCollisionSurface surface;
    const GeStageAssetDescriptor *descriptor=ge_stage_asset_multiplayer(0);
    const GeAssetPackEntry *entry;
    void *collision,*storage;
    size_t storage_size;
    assert(argc==3&&ge_asset_pack_open(&arena_pack,argv[2])==GE_ASSET_PACK_OK);
    entry=ge_asset_pack_find(&arena_pack,descriptor->collision_path);
    assert(entry&&entry->data_size<SIZE_MAX);
    collision=malloc((size_t)entry->data_size);assert(collision);
    assert(ge_asset_pack_read(&arena_pack,descriptor->collision_path,collision,(size_t)entry->data_size,NULL)==GE_ASSET_PACK_OK);
    assert(ge_stan_collision_open(collision,(size_t)entry->data_size,&surface)==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface,&storage_size)==GE_STAN_COLLISION_OK);
    storage=malloc(storage_size);assert(storage);
    assert(ge_stan_native_materialize(&surface,descriptor->level_scale,storage,storage_size,&arena)==GE_STAN_COLLISION_OK);
#else
    assert(argc==2);
#endif
    assert(ge_asset_pack_open(&pack,argv[1])==GE_ASSET_PACK_OK);
    run(&pack,2);run(&pack,4);ge_asset_pack_close(&pack);
#ifdef GE_TEST_GUN_GAMEPLAY
    free(storage);free(collision);ge_asset_pack_close(&arena_pack);
    puts("gun gameplay: real trigger/hand state/noise/inventory/ammo/audio resource and ROM models on native Temple STAN; no held-item/world/match lifecycle claim");
#endif
    return 0;
}
