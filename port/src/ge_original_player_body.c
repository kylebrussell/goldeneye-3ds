#include "ge_original_player_body.h"

#include <string.h>

#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/bondinv.h"
#include "game/bondview.h"
#include "game/file.h"
#include "game/gun.h"
#include "game/player.h"

typedef struct GeOriginalPlayerBodyBinding {
    void *context;
    GeOriginalPlayerBodyConstruct construct;
    GeOriginalPlayerBodyAttachHeldItem attach_held_item;
} GeOriginalPlayerBodyBinding;

static GeOriginalPlayerBodyBinding ge_player_body_binding;
static GeOriginalPlayerBodySnapshot ge_player_body_snapshot;

extern enum CAMERAMODE g_CameraMode;
/* Canonical bondview2.c BSS is omitted by its bounded movement translation
 * unit. Keep the real game-facing symbol so bondview_r's unchanged intro
 * parser publishes the authored loadout into the same state. */
ITEM_IDS starting_weapon[2] __attribute__((weak));
extern void bondviewDeregisterPlayerRoom(struct player *player);
extern s32 getPropForHeldItem(ITEM_IDS item);

void ge_original_player_body_bind(
    void *context,GeOriginalPlayerBodyConstruct construct)
{
    ge_player_body_binding.context=context;
    ge_player_body_binding.construct=construct;
}

void ge_original_player_body_bind_held_item(
    void *context,GeOriginalPlayerBodyAttachHeldItem attach)
{
    if(ge_player_body_binding.context==context)
        ge_player_body_binding.attach_held_item=attach;
}

void ge_original_player_body_unbind(void *context)
{
    if(ge_player_body_binding.context==context){
        ge_player_body_binding.context=NULL;
        ge_player_body_binding.construct=NULL;
        ge_player_body_binding.attach_held_item=NULL;
    }
}

void ge_original_player_body_reset(void)
{
    memset(&ge_player_body_snapshot,0,sizeof(ge_player_body_snapshot));
    ge_player_body_snapshot.body_id=-1;
    ge_player_body_snapshot.head_id=-1;
}

void ge_original_player_body_snapshot(GeOriginalPlayerBodySnapshot *snapshot)
{
    if(snapshot!=NULL)*snapshot=ge_player_body_snapshot;
}

const char *ge_original_player_body_status_name(
    GeOriginalPlayerBodyStatus status)
{
    switch(status){
    case GE_ORIGINAL_PLAYER_BODY_OK:return "ok";
    case GE_ORIGINAL_PLAYER_BODY_UNBOUND:return "unbound";
    case GE_ORIGINAL_PLAYER_BODY_INVALID_PLAYER:return "invalid player";
    case GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE:
        return "multiplayer unavailable";
    case GE_ORIGINAL_PLAYER_BODY_CONSTRUCT_FAILED:return "construct failed";
    default:return "unknown";
    }
}

/* Resource loading is the only substituted boundary here. Body/head choice,
 * hand removal, room deregistration and lifecycle ordering mirror the
 * unchanged solo_char_load body. Character construction is delegated to the
 * stage's canonical model/ChrRecord owner. */
void solo_char_load(void)
{
    s32 helddst,body=BODY_Formal_Wear,head=HEAD_Male_Brosnan_Default;
    ITEM_IDS item;
    f32 yaw;
    ++ge_player_body_snapshot.load_requests;
    if(g_CurrentPlayer==NULL||g_CurrentPlayer->prop==NULL){
        ge_player_body_snapshot.status=
            GE_ORIGINAL_PLAYER_BODY_INVALID_PLAYER;return;
    }
    if(ge_player_body_binding.construct==NULL){
        ge_player_body_snapshot.status=GE_ORIGINAL_PLAYER_BODY_UNBOUND;return;
    }
    yaw=bondviewGetPlayerYawRadians();
    if(g_CurrentPlayer->prop->chr!=NULL){
        if(ge_player_body_binding.construct(ge_player_body_binding.context,
                g_CurrentPlayer,body,head,yaw)){
            ++ge_player_body_snapshot.successful_loads;
            ge_player_body_snapshot.status=GE_ORIGINAL_PLAYER_BODY_OK;
        }else ge_player_body_snapshot.status=
            GE_ORIGINAL_PLAYER_BODY_CONSTRUCT_FAILED;
        return;
    }
    if(getPlayerCount()!=1){
        ge_player_body_snapshot.status=
            GE_ORIGINAL_PLAYER_BODY_MULTIPLAYER_UNAVAILABLE;return;
    }
    item=get_item_in_hand_or_watch_menu(GUNRIGHT);
    helddst=fileGetBondForCurrentFolder();
    switch(g_CurrentPlayer->bondtype){
    case CUFF_BOILER:body=BODY_Special_Operations_Uniform;break;
    case CUFF_JUNGLE:body=BODY_Jungle_Fatigues;break;
    case CUFF_SNOW:body=BODY_Parka;break;
    case CUFF_BROSNAN:case CUFF_CONNERY:case CUFF_DALTON:case CUFF_MOORE:
        body=BODY_Brosnan_Tuxedo;break;
    case CUFF_FOLDER:
        if(helddst==BOND_BROSNAN||helddst==BOND_CONNERY
                ||helddst==BOND_DALTON||helddst==BOND_MOORE)
            body=BODY_Brosnan_Tuxedo;
        break;
    default:break;
    }
    switch(helddst){
    case BOND_BROSNAN:
        switch(g_CurrentPlayer->bondtype){
        case CUFF_BOILER:head=HEAD_Male_Brosnan_Boiler;break;
        case CUFF_JUNGLE:head=HEAD_Male_Brosnan_Jungle;break;
        case CUFF_BROSNAN:case CUFF_CONNERY:case CUFF_DALTON:
        case CUFF_MOORE:case CUFF_FOLDER:
            head=HEAD_Male_Brosnan_Tuxedo;break;
        default:break;
        }
        break;
    case BOND_CONNERY:case BOND_DALTON:case BOND_MOORE:
        head=HEAD_Male_Brosnan_Tuxedo;break;
    default:break;
    }
    if(g_CameraMode==CAMERAMODE_SWIRL)item=starting_weapon[GUNRIGHT];
    remove_item_in_hand(GUNLEFT);
    remove_item_in_hand(GUNRIGHT);
    bondviewDeregisterPlayerRoom(g_CurrentPlayer);
    ge_player_body_snapshot.body_id=body;
    ge_player_body_snapshot.head_id=head;
    if(!ge_player_body_binding.construct(ge_player_body_binding.context,
            g_CurrentPlayer,body,head,yaw)){
        ge_player_body_snapshot.status=
            GE_ORIGINAL_PLAYER_BODY_CONSTRUCT_FAILED;return;
    }
    {
        const s32 prop=getPropForHeldItem(item);
        if(prop>=0&&(ge_player_body_binding.attach_held_item==NULL
                ||!ge_player_body_binding.attach_held_item(
                    ge_player_body_binding.context,g_CurrentPlayer,
                    prop,item,0U)))
            ++ge_player_body_snapshot.held_item_frontiers;
    }
    ++ge_player_body_snapshot.successful_loads;
    ge_player_body_snapshot.status=GE_ORIGINAL_PLAYER_BODY_OK;
}
