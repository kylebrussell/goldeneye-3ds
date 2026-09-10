#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "ge_original_stage_active_props.h"
#include "ge_original_stage_setup.h"
PropRecord *g_ActivePropsHead, *g_ActivePropsTail;
ChrRecord *g_ChrSlots;
s32 g_NumChrSlots;
stagesetup g_CurrentSetup;
s32 g_ClockTimer=1, g_GlobalTimer=1;
f32 g_GlobalTimerDelta=1;
struct player *g_CurrentPlayer, *g_playerPointers[4];
extern PLAYER_ID array_PLAYER_IDs[4];
void chrpropDelist(PropRecord *prop);
void chrpropActivateThisFrame(PropRecord *prop);
static unsigned pre_ticks, ticks[4];
void ge_original_dam_guard_all_chr_tick_exact(void) { ++pre_ticks; }
void ge_original_dam_guard_props_tick_exact(void)
{
    unsigned i;
    for(i=0;i<4;++i) if(g_CurrentPlayer==g_playerPointers[i]) { ++ticks[i]; return; }
    assert(0);
}
static void roster(unsigned count)
{
    struct player players[4]={{0}};
    PropRecord viewer[4]={{0}}, object={0};
    ChrRecord chrs[4]={{0}};
    stagesetup native={0};
    GeOriginalStageSetupRuntime setup={.setup=&native,.prop_record_count=1};
    GeOriginalStageActivePropInput input={.prop=&object};
    GeOriginalStageActiveProps state={0};
    GeOriginalStageActiveProps replacement={0};
    void *props[4];
    unsigned i,before=pre_ticks;
    memset(ticks,0,sizeof(ticks));
    memset(g_playerPointers,0,sizeof(g_playerPointers));
    /* Nontrivial canonical shuffle, also filtering absent slots for solo/two. */
    array_PLAYER_IDs[0]=PLAYER_3;array_PLAYER_IDs[1]=PLAYER_1;
    array_PLAYER_IDs[2]=PLAYER_4;array_PLAYER_IDs[3]=PLAYER_2;
    object.type=PROP_TYPE_OBJ;
    for(i=0;i<count;++i){
        viewer[i].type=PROP_TYPE_VIEWER;props[i]=&viewer[i];
        players[i].prop=&viewer[i];g_playerPointers[i]=&players[i];
    }
    assert(ge_original_stage_active_props_compose_players(&state,&setup,
        props,count,chrs,count,&input,1)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(g_ActivePropsHead==props[0]&&g_ActivePropsTail==&object);
    for(i=0;i<count;++i)assert(viewer[i].next==(i+1<count?props[i+1]:&object));
    g_CurrentPlayer=&players[get_nth_player_from_shuffled(0)];
    assert(ge_original_stage_active_props_pre_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(pre_ticks==before+1);
    for(i=0;i<count;++i){
        unsigned slot=(unsigned)get_nth_player_from_shuffled((PLAYER_ID)i);
        g_CurrentPlayer=&players[slot];
        assert(ge_original_stage_active_props_pre_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
        assert(pre_ticks==before+1);
        assert(ge_original_stage_active_props_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
        assert(ticks[slot]==1);
        if(i+1<count){
            assert(state.pre_tick_pending);
            assert(ge_original_stage_active_props_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_PLAYER_UNBOUND);
        }
    }
    assert(state.ticks==1&&!state.pre_tick_pending);
    assert(ge_original_stage_active_props_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND);
    assert(ge_original_stage_active_props_compose_players(&state,&setup,
        state.player_props,count,chrs,count,&input,1)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(state.player_count==count&&g_ActivePropsHead==props[0]);
    /* Duplicate player rejection is transactional, retaining the live list. */
    if(count>1){
        props[1]=props[0];
        assert(ge_original_stage_active_props_compose_players(&state,&setup,
            props,count,chrs,count,&input,1)==GE_ORIGINAL_STAGE_ACTIVE_PROP_DUPLICATE);
        assert(state.bound&&g_ActivePropsTail==&object);props[1]=&viewer[1];
    }
    /* These exact canonical RETICK operations may legally change the head. */
    chrpropDelist(&object);chrpropActivateThisFrame(&object);
    assert(g_ActivePropsHead==&object);
    assert(ge_original_stage_active_props_pre_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    ge_original_stage_active_props_close(&state);
    assert(!g_ActivePropsHead&&!g_ActivePropsTail);
    /* Empty arena actors are valid; all viewer props still participate. */
    assert(ge_original_stage_active_props_compose_players(&state,&setup,
        props,count,chrs,count,NULL,0)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(g_ActivePropsTail==props[count-1]);
    /* An old owner cannot clear a replacement which shares the same roster,
     * chr pool and setup. Ownership must distinguish the composing state. */
    assert(ge_original_stage_active_props_compose_players(&replacement,&setup,
        props,count,chrs,count,NULL,0)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_stage_active_props_pre_tick_exact(&state)==GE_ORIGINAL_STAGE_ACTIVE_PROP_NOT_BOUND);
    ge_original_stage_active_props_close(&state);
    assert(g_ActivePropsHead==props[0]&&g_ChrSlots==chrs);
    ge_original_stage_active_props_close(&replacement);
    assert(!g_ActivePropsHead&&!g_ActivePropsTail&&!g_ChrSlots);
}
int main(void){roster(1);roster(2);roster(4);puts("active-prop roster passes: solo/two/four, one world pre-tick, one prop pass per player");}
