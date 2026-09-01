#include <assert.h>
#include <math.h>
#include <string.h>

#include <bondconstants.h>
#include <bondtypes.h>
#ifndef PLAYERFLAG
typedef int PLAYERFLAG;
#endif
#include "game/chrai.h"
#include "game/player.h"

extern void chrpropUpdateAutoaimTarget(void);

#ifdef VERSION_EU
#define TEST_BONDVIEW_AUTOAIM_TIME 0x19
#else
#define TEST_BONDVIEW_AUTOAIM_TIME 0x1e
#endif

s32 g_ClockTimer=1;
struct player *g_CurrentPlayer;
struct player *g_playerPointers[4];
PropRecord *g_OnScreenPropList[ONSCREEN_PROP_LIST_LEN];
PropRecord **g_LastOnScreenProp=g_OnScreenPropList;

static int los_result=1;
static int los_same_stan=1;
static StandTile *los_destination_stan;
static unsigned los_calls;
static unsigned tank_flag_calls;

bool currentPlayerGetYAutoAimEnabledRedirect(void)
{return g_CurrentPlayer->autoyaimenabled;}

bool currentPlayerGetXAutoAimEnabledRedirect(void)
{return g_CurrentPlayer->autoxaimenabled;}

s32 getPlayerCount(void)
{return 1;}

s32 get_cur_playernum(void)
{return 0;}

s32 getPlayerPointerIndex(PropRecord *prop)
{return prop==g_CurrentPlayer->prop?0:-1;}

PropRecord *getCurrentPlayerProp(void)
{return g_CurrentPlayer->prop;}

f32 getPlayer_c_screenwidth(void)
{return g_CurrentPlayer->c_screenwidth;}

f32 getPlayer_c_screenheight(void)
{return g_CurrentPlayer->c_screenheight;}

f32 getPlayer_c_screenleft(void)
{return g_CurrentPlayer->c_screenleft;}

f32 getPlayer_c_screentop(void)
{return g_CurrentPlayer->c_screentop;}

f32 floorFloat(f32 value)
{return floorf(value);}

f32 ceilFloat(f32 value)
{return ceilf(value);}

void transform3Dto2DCoords(coord3d *in,coord2d *out)
{
    const f32 inv_z=1.0f/in->z;
    out->y=in->y*inv_z*g_CurrentPlayer->c_recipscaley
        +g_CurrentPlayer->c_screentop+g_CurrentPlayer->c_halfheight;
    out->x=g_CurrentPlayer->c_screenleft+g_CurrentPlayer->c_halfwidth
        -in->x*inv_z*g_CurrentPlayer->c_recipscalex;
}

PropRecord *chrGetEquippedWeaponProp(ChrRecord *chr,GUNHAND hand)
{return chr->weapons_held[hand];}

void modelGetXYExtents(Model *model,f32 *xmax,f32 *xmin,
                       f32 *ymax,f32 *ymin)
{
    (void)model;
    *xmax=20.0f;*xmin=-20.0f;*ymax=90.0f;*ymin=-90.0f;
}

f32 bondviewGetPlayerDuckingHeightRelated(struct player *player)
{(void)player;return 100.0f;}

void bondviewUpdateGuardTankFlagsRelated(PropRecord *prop,bool enabled)
{assert(prop==g_CurrentPlayer->prop);(void)enabled;++tank_flag_calls;}

s32 stanTestLineUnobstructed(StandTile **stan,f32 start_x,f32 start_z,
    f32 end_x,f32 end_z,s32 cdtypes,f32 start_height,f32 end_height,
    f32 slope_start,f32 slope_end)
{
    (void)start_x;(void)start_z;(void)end_x;(void)end_z;(void)cdtypes;
    (void)start_height;(void)end_height;(void)slope_start;(void)slope_end;
    ++los_calls;
    if(los_result&&los_same_stan)*stan=los_destination_stan;
    return los_result;
}

/* Unchanged bondviewUpdateYAutoAimTime body. X is retained unchanged in the
 * generated canonical autoaim slice used by this test. */
void bondviewUpdateYAutoAimTime(PropRecord *target,f32 amount)
{
    if(g_CurrentPlayer->autoyaimtime60>=0)
        g_CurrentPlayer->autoyaimtime60-=g_ClockTimer;
    if(target!=g_CurrentPlayer->autoaim_target_y){
        if(g_CurrentPlayer->autoyaimtime60<0){
            g_CurrentPlayer->autoyaimtime60=TEST_BONDVIEW_AUTOAIM_TIME;
            g_CurrentPlayer->autoaim_target_y=target;
        }else return;
    }
    g_CurrentPlayer->autoaimy=amount;
}

static void reset_aim(struct player *player)
{
    player->autoaim_target_x=NULL;player->autoaim_target_y=NULL;
    player->autoxaimtime60=-1;player->autoyaimtime60=-1;
    player->autoaimx=player->autoaimy=123.0f;
    los_calls=0U;tank_flag_calls=0U;
}

int main(void)
{
    struct player player;
    PropRecord viewer,target,weapon;
    ChrRecord chr;
    Model model;
    ModelFileHeader header;
    RenderPosView render_pos[2];
    StandTile player_stan,target_stan,wrong_stan;

    memset(&player,0,sizeof(player));memset(&viewer,0,sizeof(viewer));
    memset(&target,0,sizeof(target));memset(&weapon,0,sizeof(weapon));
    memset(&chr,0,sizeof(chr));memset(&model,0,sizeof(model));
    memset(&header,0,sizeof(header));memset(render_pos,0,sizeof(render_pos));
    memset(&player_stan,0,sizeof(player_stan));
    memset(&target_stan,0,sizeof(target_stan));
    memset(&wrong_stan,0,sizeof(wrong_stan));

    g_CurrentPlayer=&player;g_playerPointers[0]=&player;
    player.prop=&viewer;viewer.type=PROP_TYPE_VIEWER;viewer.stan=&player_stan;
    /* Live top-screen viewport publication: 320x240 inset at x=40. */
    player.c_screenleft=40.0f;player.c_screenwidth=320.0f;
    player.c_screenheight=240.0f;
    player.c_halfwidth=160.0f;player.c_halfheight=120.0f;
    player.c_recipscalex=166.2769f;player.c_recipscaley=207.8461f;
    player.autoxaimenabled=TRUE;player.autoyaimenabled=TRUE;
    player.crosshair_angle.x=200.0f;player.crosshair_angle.y=120.0f;

    target.type=PROP_TYPE_CHR;target.flags=PROPFLAG_ENABLED|PROPFLAG_ONSCREEN;
    target.chr=&chr;target.stan=&target_stan;target.pos.z=-500.0f;
    chr.prop=&target;chr.model=&model;chr.actiontype=ACT_STAND;
    chr.weapons_held[GUNRIGHT]=&weapon;
    model.obj=&header;model.render_pos=render_pos;header.numMatrices=2;
    render_pos[0].pos.m[3][2]=-500.0f;
    render_pos[1].pos.m[3][2]=-500.0f;
    los_destination_stan=&target_stan;
    g_OnScreenPropList[0]=&target;g_LastOnScreenProp=&g_OnScreenPropList[1];

    /* Fully published, armed, centred, unobstructed and same-STAN: the
     * unchanged original pipeline must acquire the authored guard. */
    reset_aim(&player);los_result=1;los_same_stan=1;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==&target
           &&player.autoaim_target_y==&target
           &&fabsf(player.autoaimx)<0.0001f
           &&fabsf(player.autoaimy)<0.0001f
           &&los_calls==1U&&tank_flag_calls==2U);

    /* Exact rejection predicates, isolated one at a time. */
    reset_aim(&player);chr.weapons_held[GUNRIGHT]=NULL;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==NULL&&player.autoaim_target_y==NULL
           &&los_calls==0U);
    chr.weapons_held[GUNRIGHT]=&weapon;

    reset_aim(&player);target.flags&=(u8)~PROPFLAG_ONSCREEN;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==NULL&&player.autoaim_target_y==NULL
           &&los_calls==0U);
    target.flags|=PROPFLAG_ONSCREEN;

    reset_aim(&player);los_result=0;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==NULL&&player.autoaim_target_y==NULL
           &&los_calls==1U);

    reset_aim(&player);los_result=1;los_same_stan=0;
    los_destination_stan=&wrong_stan;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==NULL&&player.autoaim_target_y==NULL
           &&los_calls==1U);
    los_same_stan=1;los_destination_stan=&target_stan;

    reset_aim(&player);player.autoyaimenabled=FALSE;
    chrpropUpdateAutoaimTarget();
    assert(player.autoaim_target_x==NULL&&player.autoaim_target_y==NULL
           &&los_calls==0U);

    return 0;
}
