/* Headless production composition. All gameplay producers are linked from the
 * production recipe. Callbacks below own native assets/cameras/publications;
 * they do not replace movement, props, gun, held-item hits or death rules.
 * The initial milestone runs the real arena/player/body graph. Authored supply
 * construction and a hit/death/respawn transcript are subsequent acceptance
 * gates; reaching this file's bootstrap is not a completed match claim. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include "game/player.h"
#include "game/chrai.h"
#include "game/gun.h"
#include "game/bondview_r.h"
#include "game/initanitable.h"
extern void chrpropDeregisterRoom(PropRecord *,s16);
#include "ge_port.h"
#include "ge_dam_world.h"
#include "ge_dam_visibility_runtime.h"
#include "ge_original_bg_geometry.h"
#include "ge_original_bond_movement.h"
#include "ge_original_character_appearance.h"
#include "ge_original_effect_runtime.h"
#include "ge_original_multiplayer_fog.h"
#include "ge_original_first_person_assets.h"
#include "ge_original_first_person_pose.h"
#include "ge_original_gameplay_services.h"
#include "ge_original_guard_animation_table.h"
#include "ge_original_gun_live.h"
#include "ge_original_multiplayer_players.h"
#include "ge_original_multiplayer_runtime.h"
#include "ge_original_multiplayer_session.h"
#include "ge_original_multiplayer_spawn.h"
#include "ge_original_multiplayer_stage_reset.h"
#include "ge_original_player_body.h"
#include "ge_original_player_gait.h"
#include "ge_original_player_held_items.h"
#include "ge_original_prop_state.h"
#include "ge_original_stage_supplies.h"

typedef struct PlayerResources {
    GeOriginalPlayerViewState view;
    GeOriginalBondMovementStatus movement;
    GeOriginalBondLiveState live;
    GeOriginalFirstPersonAssets assets;
    GeOriginalFirstPersonLoaderState loader;
    GeOriginalFirstPersonPoseState pose;
    GeOriginalPlayerGait *gait;
    GeOriginalMultiplayerSpawnResult spawn;
    GeOriginalMultiplayerSpawnProviders spawn_providers;
    void *hand_buffers[2];
} PlayerResources;

typedef struct Match {
    unsigned count;
    GePortState port;
    GeStanNativeMap stan;
    void *collision, *stan_storage;
    GeOriginalStageSetupRuntime setup;
    GeOriginalStageMultiplayerSetup prepared;
    GeOriginalPropState props;
    GeOriginalPlayerSpawnProviders spawn;
    GeOriginalStageActiveProps active;
    GeOriginalCharacterModelProvider *models;
    GeOriginalPitemModelProvider *items;
    GeOriginalStageGuardRuntime *guards;
    GeDamWorld world;
    GeDamVisibilityRuntime visibility;
    GeOriginalBgGeometry *geometry;
    GeOriginalMultiplayerDeathState death;
    GeOriginalMultiplayerHud hud;
    GeOriginalMultiplayerViews views;
    GeOriginalBondCameraResult local_camera;
    PlayerResources players[4];
} Match;

static const GeOriginalBondMovementProviders movement = {
    .collision_types=ge_original_bond_movement_normal_collision_types,
    .set_prop_collision=ge_original_bond_movement_set_current_player_collision
};
static unsigned char *file_bytes(const char *path, size_t *size)
{
    FILE *file=fopen(path,"rb"); assert(file && !fseek(file,0,SEEK_END));
    long length=ftell(file); assert(length>0 && !fseek(file,0,SEEK_SET));
    unsigned char *bytes=malloc((size_t)length); assert(bytes);
    assert(fread(bytes,1,(size_t)length,file)==(size_t)length);
    fclose(file); *size=(size_t)length; return bytes;
}
static void *asset_bytes(GeAssetPack *pack,const char *path,size_t *size)
{
    const GeAssetPackEntry *entry=ge_asset_pack_find(pack,path);
    if(!entry)fprintf(stderr,"missing arena resource: %s\n",path);
    assert(entry); *size=(size_t)entry->data_size;
    void *bytes=malloc(*size);assert(bytes);
    assert(ge_asset_pack_read(pack,path,bytes,*size,NULL)==GE_ASSET_PACK_OK);
    return bytes;
}
static int select_player(void *context,uint32_t slot)
{
    Match *match=context; if(slot>=match->count || !ge_original_multiplayer_select_player(slot))return 0;
    PlayerResources *player=&match->players[slot];
    ge_original_player_spawn_select(&match->spawn,&player->view);
    ge_original_bond_movement_select(&movement,&player->movement);
    ge_original_first_person_assets_select_loader(&player->assets,&player->loader);
    ge_original_first_person_pose_select(&player->pose);
    return !player->gait || ge_original_player_gait_select_current_player(player->gait);
}
static void deregister(void *context,void *prop,int16_t room)
{ (void)context; if(room>=0)chrpropDeregisterRoom(prop,room); }
static float floor_y(void *context,void *stan,float x,float z)
{ Match *match=context;return ge_original_stan_get_position_y(&match->stan,stan,x,z); }
static int life_ready(void *context,uint32_t slot)
{ Match *match=context;return slot<match->count && match->players[slot].live.initialized; }
static void *allocate_body_prop(void *context)
{ Match *match=context;return ge_original_prop_state_allocate_player(&match->props); }
static int resident(void *context,uint8_t room)
{ Match *match=context;return ge_dam_world_room(&match->world,room)!=NULL; }
static int tile_rgb(void *context,void *opaque,float x,float z,uint8_t rgb[3])
{
    Match *match=context; const GeStanNativeTile *tile=opaque;(void)x;(void)z;
    if(!tile || !rgb || !ge_stan_native_original_binding_matches(&match->stan))return 0;
    unsigned mid=(uint16_t)tile->mid;
    rgb[0]=(uint8_t)(((mid>>8)&15)*17);rgb[1]=(uint8_t)(((mid>>4)&15)*17);rgb[2]=(uint8_t)((mid&15)*17);
    return 1;
}
static int projectile_models(void *context,int32_t item)
{ (void)context;(void)weaponLoadProjectileModels((ITEM_IDS)item);return 1; }
static int camera(void *context,uint32_t slot,GeOriginalBondCameraConfig *config,GeOriginalBondCameraResult *result)
{
    Match *match=context;const GeOriginalPlayerViewState *view=&match->players[slot].view;
    const GeStageAssetDescriptor *stage=match->setup.descriptor;
    const GeDamWorldRoom *room=ge_dam_world_room(&match->world,(uint8_t)view->room);float fovy;
    if(!room || !ge_original_multiplayer_runtime_camera_fovy(&fovy))return 0;
    memset(config,0,sizeof(*config));
    memcpy(config->camera_position,view->camera_position,sizeof(config->camera_position));
    memcpy(config->camera_look_direction,view->camera_look,sizeof(config->camera_look_direction));
    memcpy(config->camera_up,view->camera_up,sizeof(config->camera_up));
    for(unsigned axis=0;axis<3;++axis)config->room_origin[axis]=room->origin[axis]/stage->level_scale;
    config->room_position_scale=stage->level_scale;
    config->camera_local_scale=config->visibility_scale=stage->visibility_scale;
    config->viewport_scale[0]=640;config->viewport_scale[1]=480;config->viewport_scale[2]=511;
    config->viewport_translation[0]=800;config->viewport_translation[1]=480;config->viewport_translation[2]=511;
    config->room=(uint8_t)view->room;
    return ge_original_bond_camera_set_perspective(config,fovy,4.0f/3.0f,
        match->visibility.near_distance,match->visibility.far_distance)==GE_ORIGINAL_BOND_CAMERA_OK
        && ge_original_bond_camera_run(config,result)==GE_ORIGINAL_BOND_CAMERA_OK;
}
static int visibility(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *camera_result)
{
    Match *match=context;GeOriginalBgVisibilityResult result;(void)slot;
    return ge_dam_visibility_runtime_run(&match->visibility,camera_result,config->camera_position,&result)==GE_DAM_VISIBILITY_RUNTIME_OK;
}
static int capture(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{ Match *match=context;(void)slot;(void)config;match->local_camera=*result;return 1; }
static int restore(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{ Match *match=context;(void)slot;(void)config;return memcmp(&match->local_camera,result,sizeof(*result))==0; }
static int prepare_renderer(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{
    Match *match=context;(void)config;(void)result;
    ge_original_stage_guard_runtime_set_local_player_view(match->guards,ge_original_multiplayer_player(slot),1);
    return 1;
}
static int sync_gun(void *context,uint32_t slot)
{
    Match *match=context;PlayerResources *player=&match->players[slot];
    for(unsigned hand=0;hand<2;++hand){
        int32_t item=ge_original_first_person_pose_current_item(hand);void *header;unsigned asset_slot;
        if(item==player->pose.model_item[hand] || !ge_original_first_person_assets_supports_item(item))continue;
        if(ge_original_first_person_assets_acquire_item_native(&player->assets,hand,item,&header,&asset_slot)!=GE_ORIGINAL_FIRST_PERSON_ASSET_OK)return 0;
        if(ge_original_first_person_pose_bind_hand_model(hand,item,header)!=GE_ORIGINAL_FIRST_PERSON_POSE_OK)return 0;
    }
    return 1;
}

static void run(GeAssetPack *arena,GeAssetPack *models,GeAssetPack *guns,
    const char *animation_dir,unsigned count)
{
    Match *match=calloc(1,sizeof(*match));assert(match);match->count=count;
    const GeStageAssetDescriptor *stage=ge_stage_asset_multiplayer(0);
    GeStanCollisionSurface surface;size_t size,storage_size;
    GeOriginalBgGeometryReport bg_report;
    GeOriginalCharacterModelStatus model_status;GeOriginalPitemModelStatus item_status;
    GeOriginalStageGuardRuntimeStatus body_status;GeOriginalStageGuardWeaponBindReport weapon_report;
    GeOriginalMultiplayerConfig config={(int32_t)count,SCENARIO_NORMAL,1,stage->level_id,{0,0,1,1}};
    ge_port_init(&match->port);assert(ge_port_start_stage(&match->port,stage->level_id));
    match->collision=asset_bytes(arena,stage->collision_path,&size);
    assert(ge_stan_collision_open(match->collision,size,&surface)==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_required_size(&surface,&storage_size)==GE_STAN_COLLISION_OK);
    match->stan_storage=malloc(storage_size);assert(match->stan_storage);
    assert(ge_stan_native_materialize(&surface,stage->level_scale,match->stan_storage,storage_size,&match->stan)==GE_STAN_COLLISION_OK);
    assert(ge_stan_native_bind_original(&match->stan)==GE_STAN_COLLISION_OK);
    assert(ge_original_stage_setup_load(arena,stage,&match->setup)==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_bind_stan(&match->setup,&match->stan)==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_dam_visibility_runtime_load_for_stage(arena,stage,&match->visibility)==GE_DAM_VISIBILITY_RUNTIME_OK);
    assert(ge_dam_world_parse(match->visibility.background,match->visibility.background_size,&match->world)==GE_DAM_WORLD_OK);
    match->geometry=ge_original_bg_geometry_load(arena,stage,&match->world,match->visibility.room_bounds,&bg_report);
    assert(match->geometry);ge_original_bg_geometry_bind(match->geometry);
    ge_original_player_held_items_bind_lights(match->geometry,ge_original_bg_geometry_light_break);
    assert(ge_original_prop_state_reset(&match->props,256));
    assert(ge_original_multiplayer_players_create(count,0,UINT64_C(0x73d600),UINT64_C(0x73d601)));
    assert(ge_original_multiplayer_fog_load(stage->level_id,0,count,stage->visibility_scale));
    assert(ge_original_multiplayer_configure(&config)==GE_ORIGINAL_MP_OK);
    assert(ge_original_stage_setup_publish(&match->setup));
    assert(ge_original_stage_multiplayer_setup_prepare(&match->setup,count,config.scenario,config.weapon_set,&match->prepared)==GE_ORIGINAL_STAGE_SUPPLY_OK);
    ge_original_multiplayer_stage_intro_shared_reset_exact();
    match->spawn=(GeOriginalPlayerSpawnProviders){&match->props,ge_original_prop_state_allocate_player,
        ge_original_prop_state_activate,ge_original_prop_state_enable,deregister,ge_original_prop_state_register_room};
    for(unsigned slot=0;slot<count;++slot){
        PlayerResources *player=&match->players[slot];
        for(unsigned hand=0;hand<2;++hand){player->hand_buffers[hand]=malloc(GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE);assert(player->hand_buffers[hand]);}
        assert(ge_original_first_person_assets_init(&player->assets,guns,player->hand_buffers[0],GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE,
            player->hand_buffers[1],GE_ORIGINAL_FIRST_PERSON_HAND_BUFFER_SIZE)==GE_ORIGINAL_FIRST_PERSON_ASSET_OK);
        player->spawn_providers=(GeOriginalMultiplayerSpawnProviders){&match->setup,&match->spawn,match,floor_y,life_ready};
        assert(select_player(match,slot));
        assert(ge_original_multiplayer_spawn_initial(slot,&player->spawn_providers,&player->view,&player->spawn)==GE_ORIGINAL_MP_SPAWN_OK);
    }
    char path[1024];snprintf(path,sizeof(path),"%s/animation_data.bin",animation_dir);
    unsigned char *animation=file_bytes(path,&size);size_t animation_size=size;
    assert(ge_original_guard_animation_table_bind(animation,animation_size));
    snprintf(path,sizeof(path),"%s/animation_entries.bin",animation_dir);unsigned char *entries=file_bytes(path,&size);
    assert(ge_original_guard_animation_entries_bind(entries,size));
    assert(ge_original_effect_runtime_reset(count,stage->level_id));
    ge_original_gameplay_services_reset();
    match->models=ge_original_character_model_provider_create(models,ge_original_character_model_dependency_count(),14,&model_status);assert(match->models);
    match->items=ge_original_pitem_model_provider_create(models,64,64,&item_status);assert(match->items);
    ge_original_gameplay_services_bind_model_loader(match->items,ge_original_pitem_model_load);
    GeOriginalStageGuardRuntimeServices body_services={match,allocate_body_prop,
        ge_original_character_appearance_choose_head,ge_original_character_appearance_choose_sunglasses,resident,tile_rgb};
    ge_original_character_appearance_begin_stage();
    match->guards=ge_original_stage_guard_runtime_create(match->models,10,&body_services,&body_status);assert(match->guards);
    ge_original_player_body_bind(match->guards,ge_original_stage_guard_runtime_load_player_body);
    ge_original_player_body_bind_held_item(match->guards,ge_original_stage_guard_runtime_attach_player_held_item);
    assert(ge_original_stage_guard_runtime_bind_authored_weapons(match->guards,&match->setup,match->items,
        projectile_models,match,&weapon_report)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    GeOriginalAnimationRoot *roots[3];void *frames[3];const char *names[]={"sprinting","bond_eye_walk","idle"};
    for(unsigned index=0;index<3;++index){
        roots[index]=ge_original_animation_root_create(animation,animation_size,(GeOriginalBondAnimationId)index);assert(roots[index]);
        snprintf(path,sizeof(path),"%s/%s.entry.bin",animation_dir,names[index]);frames[index]=file_bytes(path,&size);
        assert(ge_original_animation_root_bind_frames(roots[index],frames[index],size));
    }
    ge_original_player_gait_bind_bond_animations(roots[1],roots[0]);
    void *player_props[4];
    for(unsigned slot=0;slot<count;++slot){
        PlayerResources *player=&match->players[slot];GeOriginalPlayerGaitStatus gait_status;
        assert(select_player(match,slot));
        assert(ge_original_bond_live_initialize_multiplayer(slot,player->hand_buffers[0],player->hand_buffers[1],&player->live));
        ge_original_first_person_pose_bind(&player->pose);
        assert(sync_gun(match,slot));
        assert(ge_original_multiplayer_spawn_finalize(&player->spawn_providers,&player->spawn)==GE_ORIGINAL_MP_SPAWN_OK);
        assert(g_CurrentPlayer->bodyModel && g_CurrentPlayer->prop->chr);
        player->gait=ge_original_player_gait_create_current_player(roots[1],&gait_status);assert(player->gait);
        assert(ge_original_player_gait_calibrate_current_player_standing(player->gait,roots[2],roots[1]));
        player_props[slot]=g_CurrentPlayer->prop;
    }
    size_t chr_count;ChrRecord *chrs=ge_original_stage_guard_runtime_chr_pool(match->guards,&chr_count);
    assert(ge_original_stage_active_props_compose_players(&match->active,&match->setup,player_props,count,chrs,chr_count,NULL,0)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_multiplayer_death_reset(&match->death));
    assert(ge_original_multiplayer_session_reset(0,0)==GE_ORIGINAL_MP_SESSION_OK);
    ge_original_gun_live_reset();ge_original_gameplay_services_set_exact_gun_dispatch(1);ge_original_gameplay_services_set_exact_held_item_dispatch(1);
    GeOriginalMultiplayerRuntimeFrame frame={.count=count,.local=0,.death=&match->death,.hud=&match->hud,
        .views=&match->views,.active_props=&match->active,.guards=match->guards,.context=match,
        .select_player=select_player,.produce_camera=camera,.sync_gun_resources=sync_gun,.prepare_local_renderer=prepare_renderer,
        .view_providers={match,select_player,visibility,capture,restore}};
    for(unsigned slot=0;slot<count;++slot)frame.lives[slot]=&match->players[slot].live;
    assert(select_player(match,0)); /* Initial prior viewport, then retain last shuffled. */
    for(unsigned tick=0;tick<2;++tick){
        GeOriginalInputPad pads[4][2]={0};GeOriginalMultiplayerRegenBatch regen;GeOriginalMultiplayerSessionState session;
        assert(ge_port_advance_multiplayer_tick(&match->port,pads,count)==1);
        assert(ge_original_multiplayer_session_tick(match->port.original_clock_timer,(int32_t)tick,&session)==GE_ORIGINAL_MP_SESSION_OK);
        assert(ge_original_gun_live_frame_begin());
        frame.tick=tick;frame.clock_timer=match->port.original_clock_timer;frame.global_timer=match->port.original_global_timer;frame.global_timer_delta=match->port.original_global_timer_delta;
        assert(ge_original_multiplayer_runtime_begin_world(&match->active,frame.clock_timer,&regen)==GE_ORIGINAL_MP_RUNTIME_OK);
        GeOriginalMultiplayerRuntimeStatus status=ge_original_multiplayer_runtime_tick(&frame);
        fprintf(stderr,"native arena %s players %u tick %u slot %u status %d\n",stage->key,count,tick,frame.last_slot,status);
        assert(status==GE_ORIGINAL_MP_RUNTIME_OK);
        assert(ge_original_gun_live_frame_finalize(NULL));
    }
    for(unsigned slot=0;slot<count;++slot){
        assert(ge_original_multiplayer_select_player(slot));
        GeOriginalGunLiveStats gun_stats;ge_original_gun_live_snapshot(&gun_stats);
        assert(gun_stats.ticks==2);
        const void *native_animation=NULL;uint32_t animation_offset=UINT32_MAX;
        assert(ge_original_guard_animation_legacy_reference(g_CurrentPlayer->players_cur_animation,
            (uintptr_t)ptr_animation_table,&native_animation,&animation_offset));
        assert(ge_port_guard_animation_owns(native_animation));
        assert(g_CurrentPlayer->prop&&g_CurrentPlayer->prop->chr&&g_CurrentPlayer->bodyModel);
    }
    /* Teardown is stage-owned; leave no canonical pool pointers to resources
     * after closing the consumers. Per-prop gameplay free remains canonical. */
    ge_original_stage_active_props_close(&match->active);
    ge_original_stage_guard_runtime_destroy(match->guards);
    for(unsigned slot=0;slot<count;++slot){
        PlayerResources *player=&match->players[slot];ge_original_player_gait_destroy(player->gait);
        ge_original_first_person_assets_close(&player->assets);free(player->hand_buffers[0]);free(player->hand_buffers[1]);
    }
    ge_original_character_model_provider_destroy(match->models);ge_original_pitem_model_provider_destroy(match->items);
    ge_original_effect_runtime_end();ge_original_multiplayer_fog_close();
    ge_original_multiplayer_players_destroy();assert(ge_original_prop_state_end(&match->props));
    ge_original_bg_geometry_close(match->geometry);ge_dam_visibility_runtime_close(&match->visibility);
    ge_original_stage_multiplayer_setup_close(&match->prepared);ge_original_stage_setup_close(&match->setup);
    for(unsigned index=0;index<3;++index){ge_original_animation_root_destroy(roots[index]);free(frames[index]);}
    ge_original_guard_animation_table_reset();free(entries);free(animation);free(match->stan_storage);free(match->collision);free(match);
}
int main(int argc,char **argv)
{
    GeAssetPack arena,models,guns;assert(argc==5);
    assert(ge_asset_pack_open(&arena,argv[1])==GE_ASSET_PACK_OK);
    assert(ge_asset_pack_open(&models,argv[2])==GE_ASSET_PACK_OK);
    assert(ge_asset_pack_open(&guns,argv[3])==GE_ASSET_PACK_OK);
    run(&arena,&models,&guns,argv[4],2);run(&arena,&models,&guns,argv[4],4);
    ge_asset_pack_close(&guns);ge_asset_pack_close(&models);ge_asset_pack_close(&arena);return 0;
}
