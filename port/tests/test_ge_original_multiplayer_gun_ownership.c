/* Resource-publication ownership test. Canonical firing/damage has separate
 * gameplay fixtures; this producer double writes recognizable model outputs. */
#include <assert.h>
#include <stdio.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
typedef int PLAYERFLAG;
#include "game/player.h"
#include "game/model.h"
#include "game/dyn.h"
#include "ge_original_gun_live.h"
#include "ge_original_bond_input_provider.h"
static ModelFileHeader headers[4][2];
static unsigned producer_calls;
void ge_original_gun_update_and_fire_both_hands_exact(void)
{
    GeOriginalBondInputProvider *p=ge_original_bond_input_provider();
    unsigned hand,slot=(unsigned)p->player_number;
    ++producer_calls;
    for(hand=0;hand<2;++hand){
        unsigned matrix;
        Model *m=ge_original_gun_host_model(hand);
        m->obj=&headers[slot][hand];
        headers[slot][hand].numMatrices=(s16)(1+slot+hand);
        p->current_player->hands[hand].field_87F=1;
        p->current_player->hands[hand].mtxlist=
            dynAllocate((s32)(sizeof(Mtxf)*(1+slot+hand)));
        for(matrix=0;matrix<1+slot+hand;++matrix)
            p->current_player->hands[hand].mtxlist[matrix].m[0][0]=
                (float)(100*slot+10*hand+matrix);
    }
}
static void run(unsigned count)
{
    struct player players[4]={{0}};
    struct player_data perms[4]={{0}};
    struct player *ptrs[4];struct player_data *pp[4];
    GeOriginalGunLiveHand publications[4][2];
    unsigned p,h;
    for(p=0;p<count;++p){ptrs[p]=&players[p];pp[p]=&perms[p];}
    assert(ge_original_bond_input_bind_roster(ptrs,pp,count));
    ge_original_gun_live_reset();
    producer_calls=0;
    /* Implicit per-player frames would reuse the same backing matrices. */
    if(count>1){
        assert(!ge_original_gun_live_tick());
        assert(!producer_calls&&!ge_original_gun_frame_arena_active());
    }
    assert(ge_original_gun_live_frame_begin());
    for(p=0;p<count;++p){
        g_CurrentPlayer=ptrs[p];
        assert(ge_original_gun_live_tick());
        for(h=0;h<2;++h)assert(ge_original_gun_live_hand_snapshot(h,&publications[p][h]));
    }
    for(p=0;p<count;++p){
        GeOriginalGunLiveStats stats;
        g_CurrentPlayer=ptrs[p];ge_original_gun_live_snapshot(&stats);
        assert(stats.ticks==1);
        assert(stats.last_frame_bytes==sizeof(Mtxf)*(3+2*p));
        for(h=0;h<2;++h){
            unsigned matrix;
            GeOriginalGunLiveHand view;
            assert(ge_original_gun_live_hand_snapshot(h,&view));
            assert(view.model==publications[p][h].model);
            assert(((const Model *)view.model)->obj==&headers[p][h]);
            assert(view.matrix_count==1+p+h&&view.visible);
            for(matrix=0;matrix<view.matrix_count;++matrix)
                assert(view.matrices[matrix][0][0]==(float)(100*p+10*h+matrix));
            if(p){
                assert(view.model!=publications[p-1][h].model);
                assert(view.matrices!=publications[p-1][h].matrices);
            }
        }
    }
    assert(ge_original_gun_live_frame_finalize(NULL));
    assert(producer_calls==count);
    assert(!ge_original_gun_live_tick());
    assert(producer_calls==count);
    ge_original_bond_input_unbind_roster();g_CurrentPlayer=NULL;
}
int main(void)
{
    struct player solo={0};
    struct player_data permissions={0};
    run(2);run(4);
    /* Preserve the existing one-player implicit-frame entry point. */
    ge_original_bond_input_bind_player(&solo,&permissions);
    ge_original_bond_input_provider_reset_normal_dam();
    assert(ge_original_gun_live_tick());
    assert(!ge_original_gun_frame_arena_active());
    puts("multiplayer gun ownership: shared-frame original dynAllocate matrices preserved for 2/4 players; implicit multiplayer rejected, solo retained");
}
