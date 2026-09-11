#include "ge_original_multiplayer_fog.h"
#include "ge_original_stage_environment.h"
#include "ge_stage_assets.h"
#include <ultra64.h>
#include <bondconstants.h>
#include "game/bgfog.h"
#include "game/propobj.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static s32 players = 1;
static float clip[2], lod = 1;
static unsigned vi_writes;
static coord3d camera_position;
static Mtxf camera_matrix;

/* Primitive native player/VI boundaries. All selection, state writes, fog
 * coefficient arithmetic, near fade and culling come from the real bodies. */
s32 getPlayerCount(void) { return players; }
void viSetZRange(f32 near, f32 far) { clip[0]=near; clip[1]=far; ++vi_writes; }
void viGetZRange(f32 *out) { out[0]=clip[0]; out[1]=clip[1]; }
f32 getPlayer_c_lodscalez(void) { return lod; }
coord3d *bondviewGetCurrentPlayersPosition(void) { return &camera_position; }
Mtxf *camGetWorldToScreenMtxf(void) { return &camera_matrix; }

static void select_stage(int32_t level, uint32_t count, int cinema, float scale)
{
    GeOriginalStageEnvironment pure;
    GeOriginalMultiplayerFogSnapshot native;
    players=(s32)count;
    ge_original_multiplayer_fog_close();
    assert(ge_original_stage_environment_select_for_player_count(level,cinema,count,&pure));
    unsigned before=vi_writes;
    assert(ge_original_multiplayer_fog_load(level,cinema,count,scale));
    assert(vi_writes == before+1);
    assert(ge_original_multiplayer_fog_snapshot(&native));
    assert(native.level_id == level && native.player_count == count);
    assert(native.visibility_scale == scale && native.cinematic == (cinema != 0));
    assert(native.fog_enabled == pure.fog_enabled);
    assert(native.red == pure.red && native.green == pure.green && native.blue == pure.blue);
    CurrentEnvironmentRecord *env=fogGetCurrentEnvironmentp();
    assert(env->Clouds == pure.clouds && env->CloudRepeat == pure.cloud_repeat
        && env->SkyImageId == pure.sky_image_id && env->CloudRed == pure.cloud_red
        && env->CloudGreen == pure.cloud_green && env->CloudBlue == pure.cloud_blue
        && env->IsWater == pure.is_water && env->WaterRepeat == pure.water_repeat
        && env->WaterImageId == pure.water_image_id && env->WaterRed == pure.water_red
        && env->WaterGreen == pure.water_green && env->WaterBlue == pure.water_blue
        && env->WaterConcavity == pure.water_concavity);
    if (pure.fog_enabled) {
        assert(native.zrange[0] == pure.blend_multiplier && native.zrange[1] == pure.far_fog);
        assert(native.distances[0] == pure.blend_multiplier/scale
            && native.distances[1] == pure.far_fog/scale);
        assert(native.has_near_fog == (pure.near_fog != 0));
        if (native.has_near_fog) {
            assert(native.near_fog[0] == pure.near_fog
                && native.near_fog[1] == pure.max_visibility_range
                && native.near_fog[2] == pure.max_obfuscation_range);
        }
        assert(native.difference_from_far_intensity == pure.difference_from_far_intensity
            && native.far_intensity == pure.far_intensity);
        assert(native.intensities[0] == (float)pure.far_intensity/1000.0f);
        assert(native.intensities[1] == (float)pure.difference_from_far_intensity/1000.0f);
        assert(isfinite(native.distances[2]) && isfinite(native.distances[3]));
    } else {
        assert(native.selected_id == -1 && native.main_id == -1);
        assert(!native.has_near_fog && native.zrange[0] == 15 && native.zrange[1] == 10000);
        assert(native.intensities[2] == FLT_MAX);
    }
}

int main(void)
{
    /* First solo load: spawn exists before input publishes the player roster.
     * The live startup must wait for that publication before loading fog. */
    players=0;
    unsigned initial_writes=vi_writes;
    assert(!ge_original_multiplayer_fog_load(LEVELID_DAM,0,1,0.2f));
    assert(vi_writes==initial_writes);
    players=1;
    assert(ge_original_multiplayer_fog_load(LEVELID_DAM,0,1,0.2f));
    assert(vi_writes==initial_writes+1);
    ge_original_multiplayer_fog_close();

    unsigned selections=0;
    for (size_t stage=0; stage<GE_STAGE_COUNT+GE_MULTIPLAYER_STAGE_COUNT; ++stage) {
        const GeStageAssetDescriptor *d=stage<GE_STAGE_COUNT
            ? ge_stage_asset_descriptor((GeStageId)stage)
            : ge_stage_asset_multiplayer(stage-GE_STAGE_COUNT);
        assert(d);
        for (uint32_t count=1;count<=4;++count)
            for (int cinema=0;cinema<=1;++cinema) {
                select_stage(d->level_id,count,cinema,d->visibility_scale);
                ++selections;
            }
    }
    GeOriginalMultiplayerFogSnapshot s,before,after;
    for (uint32_t count=2;count<=4;++count) {
        select_stage(LEVELID_BUNKER1,count,0,0.2f);
        assert(ge_original_multiplayer_fog_snapshot(&s));
        assert(s.selected_id == LEVELID_BUNKER1+(int32_t)count*100);
        assert(s.zrange[1] == 3500-(float)count*500);
        select_stage(LEVELID_FACILITY,count,0,0.2f);
        assert(ge_original_multiplayer_fog_snapshot(&s));
        assert(s.selected_id == (int32_t)count*100);
        assert(s.zrange[1] == (count==2?7500:count==3?6000:5000));
        select_stage(LEVELID_DAM,count,1,0.2f);
        assert(ge_original_multiplayer_fog_snapshot(&s));
        assert(s.selected_id == LEVELID_DAM+900 && s.zrange[0] == 30);
    }
    select_stage(LEVELID_CRADLE,2,0,0.2f);
    assert(ge_original_multiplayer_fog_snapshot(&before));
    PropRecord prop={0}; rgba_f32 color={{0}};
    prop.zDepth=-10; color.rgba[0]=42; color.rgba[3]=43;
    assert(fogGetPropDistColor(&prop,&color)==2 && color.rgba[0]==42 && color.rgba[3]==43);
    prop.zDepth=0;
    assert(fogGetPropDistColor(&prop,&color)==2 && isinf(color.rgba[3]) && color.rgba[3]<0);
    unsigned seen[3]={0};
    for (unsigned z=1;z<=200000;z+=17) {
        prop.zDepth=(float)z;
        s32 result=fogGetPropDistColor(&prop,&color);
        assert(result>=0 && result<=2); ++seen[result];
        assert(color.rgba[0]==(float)before.red/255.0f);
        assert(color.rgba[3]==before.distances[2]/prop.zDepth+before.distances[3]);
        assert(result==(color.rgba[3]<0?2:color.rgba[3]>1?0:1));
    }
    assert(seen[0] && seen[1] && seen[2]);
    prop.zDepth=800; assert(chrobjFogVisRangeRelated(&prop,100)==1);
    prop.zDepth=3333; assert(chrobjFogVisRangeRelated(&prop,100)==1);
    prop.zDepth=3888.5f; assert(chrobjFogVisRangeRelated(&prop,100)==0.5f);
    prop.zDepth=4444; assert(chrobjFogVisRangeRelated(&prop,100)==0);
    prop.zDepth=6977; assert(chrobjFogVisRangeRelated(&prop,200)==0.5f);
    lod=0.5f; prop.zDepth=7777; assert(chrobjFogVisRangeRelated(&prop,100)==0.5f);
    lod=1;
    camera_matrix.m[0][0]=1;
    coord3d point={{before.intensities[2],0,0}};
    assert(fogPositionIsVisibleThroughFog(&point,0));
    point.x+=1; assert(!fogPositionIsVisibleThroughFog(&point,0));
    assert(fogPositionIsVisibleThroughFog(&point,1));
    assert(fogGetScaledFarFogIntensitySquared()==before.intensities[2]*before.intensities[2]);
    assert(ge_original_multiplayer_fog_snapshot(&after));
    assert(memcmp(&before,&after,sizeof(before))==0);
    /* Validation never replaces a valid native owner or changes its VI. */
    unsigned writes=vi_writes;
    assert(!ge_original_multiplayer_fog_load(LEVELID_DAM,0,3,0.2f));
    assert(!ge_original_multiplayer_fog_load(LEVELID_DAM,0,2,0));
    assert(!ge_original_multiplayer_fog_load(LEVELID_DAM,0,2,NAN));
    assert(!ge_original_multiplayer_fog_load(INT32_MAX,0,2,0.2f));
    assert(!ge_original_stage_environment_select_for_player_count(33,0,0,NULL));
    assert(vi_writes==writes && ge_original_multiplayer_fog_snapshot(&after));
    assert(memcmp(&before,&after,sizeof(before))==0);
    /* Native loader preserves near-fog pointers on an in-stage fogless load. */
    players=1;
    assert(ge_original_multiplayer_fog_load(LEVELID_FRIGATE,0,1,0.2f));
    assert(ge_original_multiplayer_fog_snapshot(&s));
    assert(!s.fog_enabled && s.has_near_fog);
    select_stage(LEVELID_FRIGATE,1,0,0.2f);
    assert(ge_original_multiplayer_fog_snapshot(&s));
    prop.zDepth=10000; color.rgba[3]=42;
    assert(!s.has_near_fog && chrobjFogVisRangeRelated(&prop,100)==1);
    assert(fogGetPropDistColor(&prop,&color)==2 && color.rgba[3]==42);
    /* Alternate transition uses the original adjacent record and quantizes RGB. */
    select_stage(LEVELID_FACILITY,1,0,0.2f);
    fogSwitchToSolosky2(1);
    assert(ge_original_multiplayer_fog_snapshot(&s));
    assert(s.zrange[1]==1000 && s.red==0x40 && s.green==0x80 && s.blue==0x40);
    writes=vi_writes;
    ge_original_multiplayer_fog_close();
    assert(!ge_original_multiplayer_fog_snapshot(&s));
    assert(fogGetNearFogValuesP()==NULL && vi_writes==writes);
    puts("Native fog: player-count/cinematic selection, body alpha gates, lifecycle and read-only queries passed");
    printf("Compared %u native/pure selections across all solo and multiplayer descriptors\n",selections);
    return 0;
}
