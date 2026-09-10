/* Headless production composition. All gameplay producers are linked from the
 * production recipe. Callbacks below own native assets/cameras/publications;
 * they do not replace movement, props, gun, held-item hits or death rules.
 * Temple is a two-frame bootstrap gate. Optional Caves runs all authored live
 * supplies, controller pickup/equip/fire/reload and native impacts for 480
 * frames. --combat adds controller-routed player hits, death/respawn, scoring
 * and native timed results/menu completion. No gameplay state is assigned. */
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
#include "game/bgroomtrans.h"
#include "game/bg.h"
#include "ge_original_boss.h"
#include "ge_original_multiplayer_watch.h"
#include "game/explosion.h"
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
#include "ge_original_guard_bullet_hit.h"
#include "ge_original_pp7_fire.h"
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
#include "ge_original_stage_items.h"

static int combat;

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

typedef struct ObjectOwner {
    void *definition, *prop;
    size_t command_index;
    int16_t room;
    GeOriginalDefaultObjectPrepared prepared;
} ObjectOwner;

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
    ObjectOwner objects[128];
    GeOriginalStageActivePropInput ordinary_props[128];
    void *collision_blocks[128];
    size_t object_count, collision_count;
    unsigned native_visible_hands[2], native_firing_frames;
    GeOriginalDefaultObjectProviders object_providers;
    const GeStanNativeTile *route[1024];
    size_t route_count, route_at;
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
    if(ge_dam_visibility_runtime_run(&match->visibility,camera_result,config->camera_position,&result)!=GE_DAM_VISIBILITY_RUNTIME_OK)return 0;
    for(size_t room=0;room<match->world.room_count;++room){
        GeOriginalBgRoomVisibilitySnapshot snapshot;
        assert(ge_original_bg_visibility_room_snapshot((uint8_t)room,&snapshot));
        assert(getROOMID_isRendered((s32)room)==snapshot.rendered);
    }
    for(size_t i=0;i<result.room_count;++i){
        struct bbox2d box;
        assert(bgGet2dBboxByRoomId(result.rooms[i].room,&box));
        assert(memcmp(box.f,result.rooms[i].screen_box,sizeof(box.f))==0);
    }
    return 1;
}
static int capture(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{ Match *match=context;(void)slot;(void)config;match->local_camera=*result;return 1; }
static int restore(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{ Match *match=context;(void)slot;(void)config;return memcmp(&match->local_camera,result,sizeof(*result))==0; }
static int prepare_renderer(void *context,uint32_t slot,const GeOriginalBondCameraConfig *config,const GeOriginalBondCameraResult *result)
{
    Match *match=context;(void)config;(void)result;
    for(unsigned hand=0;hand<2;++hand){
        GeOriginalGunLiveHand native;
        assert(ge_original_gun_live_hand_snapshot(hand,&native));
        if(native.visible){
            assert(native.model && native.matrices && native.matrix_count);
            const Model *model=native.model;assert(model->datas && model->obj);
            ++match->native_visible_hands[hand];
        }
        if(g_CurrentPlayer->hands[hand].weapon_firing_status)++match->native_firing_frames;
    }
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

static int32_t item_load(void *context,int32_t model)
{ return ge_original_pitem_model_load(((Match *)context)->items,model); }
static int item_available(void *context,int32_t model)
{ return ge_original_pitem_model_available(((Match *)context)->items,model); }
static int item_resolve(void *context,int32_t model,void **header,void **instance,float *scale)
{ return ge_original_pitem_model_resolve_instance(((Match *)context)->items,model,header,instance,scale); }
static int item_hit_ready(void *context,const void *model)
{ return ge_original_pitem_model_hit_ready(((Match *)context)->items,model); }
static int32_t player_count(void *context)
{ (void)context;return (int32_t)ge_original_multiplayer_player_count(); }
static int32_t scenario(void *context)
{ (void)context;return SCENARIO_NORMAL; }
static void *item_collision(void *context,uint32_t size)
{
    Match *match=context;assert(size==0x50 && match->collision_count<128);
    void *block=calloc(1,size);assert(block);
    match->collision_blocks[match->collision_count++]=block;return block;
}
static int item_floor(void *context,void *stan,float x,float z,float *out)
{ if(!stan || !out)return -1;*out=floor_y(context,stan,x,z);return 1; }
static int room_bounds(void *context,const float pos[3],int16_t room,float *top,float *bottom)
{ (void)context;return ge_original_prop_state_room_object_at_position(pos,room,top,bottom)!=NULL; }
static int walk_tiles(void *context,void **stan,float x,float z,float end_x,float end_z)
{ return ge_original_stan_walk_tiles_between_points(&((Match *)context)->stan,(GeStanNativeTile **)stan,x,z,end_x,end_z); }
static int supply_room(void *context,void *definition)
{
    Match *match=context;
    for(size_t i=0;i<match->object_count;++i){ObjectOwner *owner=&match->objects[i];
        if(owner->definition!=definition)continue;
        ge_original_prop_state_register_room(&match->props,owner->prop,owner->room);
        return ge_original_prop_state_room_contains(owner->room,owner->prop);
    }
    return 0;
}
static int supply_activate(void *context,void *prop)
{ ge_original_prop_state_activate(&((Match *)context)->props,prop);return ge_original_prop_state_is_active(prop); }
static int supply_enable(void *context,void *prop)
{ ge_original_prop_state_enable(&((Match *)context)->props,prop);return ge_original_prop_state_is_enabled(prop); }
static int construct_object(void *context,const GeOriginalStagePropConstructionRequest *request)
{
    Match *match=context;assert(match->object_count<128);
    ObjectOwner *owner=&match->objects[match->object_count++];
    size_t size=ge_original_stage_prop_native_definition_size(request);assert(size);
    owner->definition=calloc(1,size);assert(owner->definition);
    owner->room=request->placement.room;owner->command_index=request->command_index;
    owner->prop=ge_original_prop_state_allocate(&match->props,owner->definition);assert(owner->prop);
    if(request->record->type==PROPDEF_COLLECTABLE){
        assert(ge_original_stage_prop_native_definition_init(request,owner->definition,size));
        assert(projectile_models(match,(int8_t)(request->record->words[32]>>24)));
        GeOriginalStageItemStatus status=ge_original_stage_item_construct_standard_exact(request,
            owner->definition,owner->prop,sizeof(PropRecord),&match->object_providers,&owner->prepared);
        if(status)fprintf(stderr,"item command %zu status %s\n",request->command_index,ge_original_stage_item_status_name(status));
        assert(status==GE_ORIGINAL_STAGE_ITEM_OK);
        assert(supply_room(match,owner->definition));assert(supply_activate(match,owner->prop));
        assert(supply_enable(match,owner->prop));
    }else{
        GeOriginalStageSupplyInstance instance;
        GeOriginalStageSupplyProviders supply={.default_object=&match->object_providers,
            .prepared=&owner->prepared,.update_room_position=supply_room,
            .activate_prop=supply_activate,.enable_prop=supply_enable};
        GeOriginalStageSupplyStatus status=ge_original_stage_supply_construct_exact(request,
            owner->definition,size,owner->prop,sizeof(PropRecord),&supply,&instance);
        if(status)fprintf(stderr,"supply command %zu status %s\n",request->command_index,ge_original_stage_supply_status_name(status));
        assert(status==GE_ORIGINAL_STAGE_SUPPLY_OK);
    }
    match->ordinary_props[match->object_count-1]=(GeOriginalStageActivePropInput){request->command_index,owner->prop,GE_ORIGINAL_STAGE_ACTIVE_PROP_AUTHORED};
    assert(((ObjectRecord *)owner->definition)->model && ((PropRecord *)owner->prop)->stan);
    return 1;
}
static size_t hit_player_count(void *context)
{ return ((Match *)context)->count; }
static void *hit_player(void *context,size_t slot)
{ (void)context;return ge_original_multiplayer_player((uint32_t)slot); }

/* Test controller only: reads authored adjacency, emits pads. Canonical
 * MoveBond remains the sole owner of placement and collision. */
static int tile_index(const GeStanNativeTile *const *tiles,size_t count,const void *tile)
{ for(size_t i=0;i<count;++i)if(tiles[i]==tile)return (int)i;return -1; }
static void plan_route(Match *match,unsigned walker,unsigned destination)
{
    const GeStanNativeTile *tiles[1024];int parent[1024],queue[1024];
    size_t count=match->stan.tile_count;assert(count<=1024);
    for(size_t i=0;i<count;++i){tiles[i]=ge_stan_native_tile_at(&match->stan,(uint32_t)i);parent[i]=-1;}
    struct player *player=ge_original_multiplayer_player(walker),*target=ge_original_multiplayer_player(destination);
    int start=tile_index(tiles,count,player->prop->stan),goal=tile_index(tiles,count,target->prop->stan);
    assert(start>=0 && goal>=0);size_t head=0,tail=0;queue[tail++]=start;parent[start]=start;
    while(head<tail && parent[goal]<0){
        int current=queue[head++];const GeStanNativeTile *tile=tiles[current];
        for(unsigned edge=0;edge<ge_stan_native_point_count(tile);++edge){
            unsigned link=tile->points[edge].link;if(!link)continue;
            int next=tile_index(tiles,count,match->stan.base+link*GE_STAN_NATIVE_LINK_UNIT);
            if(next<0 || parent[next]>=0)continue;parent[next]=current;queue[tail++]=next;
        }
    }
    assert(parent[goal]>=0);match->route_count=0;
    for(int index=goal;index!=start;index=parent[index])match->route[match->route_count++]=tiles[index];
    for(size_t i=0;i<match->route_count/2;++i){const GeStanNativeTile *tmp=match->route[i];
        match->route[i]=match->route[match->route_count-1-i];match->route[match->route_count-1-i]=tmp;}
    match->route_at=0;fprintf(stderr,"controller route: %zu authored STAN tiles\n",match->route_count);
}
static float signed_angle(float value)
{ return value>3.14159265f?value-6.28318531f:value; }
static void walk_route(Match *match,GeOriginalInputPad pads[4][2])
{
    struct player *player=ge_original_multiplayer_player(1),*target=ge_original_multiplayer_player(0);
    if(!match->route_count)return;
    for(size_t i=match->route_at;i<match->route_count;++i)
        if(player->prop->stan==(void *)match->route[i]){match->route_at=i+1;break;}
    float x=target->prop->pos.x,z=target->prop->pos.z;
    if(match->route_at<match->route_count){
        const GeStanNativeTile *tile=match->route[match->route_at];unsigned n=ge_stan_native_point_count(tile);
        x=z=0;for(unsigned i=0;i<n;++i){x+=tile->points[i].x;z+=tile->points[i].z;}
        x=x/n*match->stan.inverse_level_scale;z=z/n*match->stan.inverse_level_scale;
    }
    float dx=x-player->prop->pos.x,dz=z-player->prop->pos.z;const float *look=match->players[1].view.camera_look;
    float angle=signed_angle(atan2f(dx*look[2]-dz*look[0],dx*look[0]+dz*look[2]));
    pads[1][0].stick_x=(int8_t)fmaxf(-80,fminf(80,-angle*240));
    float goal_dx=target->prop->pos.x-player->prop->pos.x,goal_dz=target->prop->pos.z-player->prop->pos.z;
    if(goal_dx*goal_dx+goal_dz*goal_dz>90*90 || fabsf(target->prop->pos.y-player->prop->pos.y)>100)
        if(fabsf(angle)<0.35f)pads[1][0].stick_y=60;
}

static void run(GeAssetPack *arena,GeAssetPack *models,GeAssetPack *guns,
    const char *animation_dir,unsigned count,GeAssetPack *supplies)
{
    Match *match=calloc(1,sizeof(*match));assert(match);match->count=count;
    const GeStageAssetDescriptor *stage=ge_stage_asset_multiplayer(supplies?2:0);
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
    if(supplies)for(size_t room=0;room<match->world.room_count;++room){
        const struct coord3d *origin=ge_original_bg_geometry_room_origin((int32_t)room);
        assert(origin && getRoomPositionByIndex((s32)room)==origin);
        assert(memcmp(origin->f,match->world.rooms[room].origin,sizeof(origin->f))==0);
    }
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
    ge_original_gameplay_services_reset();
    assert(ge_original_effect_runtime_reset(count,stage->level_id));
    match->models=ge_original_character_model_provider_create(models,ge_original_character_model_dependency_count(),14,&model_status);assert(match->models);
    match->items=ge_original_pitem_model_provider_create(supplies?supplies:models,128,160,&item_status);assert(match->items);
    ge_original_gameplay_services_bind_model_loader(match->items,ge_original_pitem_model_load);
    GeOriginalStageGuardRuntimeServices body_services={match,allocate_body_prop,
        ge_original_character_appearance_choose_head,ge_original_character_appearance_choose_sunglasses,resident,tile_rgb};
    ge_original_character_appearance_begin_stage();
    match->guards=ge_original_stage_guard_runtime_create(match->models,10,&body_services,&body_status);assert(match->guards);
    ge_original_player_body_bind(match->guards,ge_original_stage_guard_runtime_load_player_body);
    ge_original_player_body_bind_held_item(match->guards,ge_original_stage_guard_runtime_attach_player_held_item);
    assert(ge_original_stage_guard_runtime_bind_authored_weapons(match->guards,&match->setup,match->items,
        projectile_models,match,&weapon_report)==GE_ORIGINAL_STAGE_GUARD_RUNTIME_OK);
    match->object_providers=(GeOriginalDefaultObjectProviders){match,item_load,player_count,scenario,
        item_resolve,item_collision,item_floor,room_bounds,walk_tiles,tile_rgb};
    if(supplies){
        GeOriginalStagePropMaterializerProviders providers={.context=match,
            .capabilities=GE_ORIGINAL_STAGE_PROP_CAP_ITEM|GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY,
            .model_available=item_available,.construct_item=construct_object,.construct_special_object=construct_object};
        GeOriginalStagePropMaterializerReport report;
        assert(ge_original_stage_prop_materialize_ready(&match->prepared.view,&providers,&report));
        fprintf(stderr,"native arena %s constructed %zu omitted %zu missing %zu model %zu unsupported %zu\n",
            stage->key,report.constructed,report.canonical_omitted,report.missing_service,report.missing_model,report.unsupported_branch);
        assert(!report.failed && !report.missing_model && !report.missing_service && !report.unsupported_branch);
        assert(report.constructed==match->object_count && report.canonical_omitted==match->prepared.omitted);
    }
    ge_original_pp7_fire_bind_object_hit_ready(match,item_hit_ready);
    ge_original_guard_bullet_hit_bind_players(match,hit_player_count,hit_player);
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
    assert(ge_original_stage_active_props_compose_players(&match->active,&match->setup,player_props,count,chrs,chr_count,match->ordinary_props,match->object_count)==GE_ORIGINAL_STAGE_ACTIVE_PROP_OK);
    assert(ge_original_multiplayer_death_reset(&match->death));
    assert(ge_original_multiplayer_session_reset(combat&&supplies?1800:0,0)==GE_ORIGINAL_MP_SESSION_OK);
    ge_original_gun_live_reset();ge_original_gameplay_services_set_exact_gun_dispatch(1);ge_original_gameplay_services_set_exact_held_item_dispatch(1);
    GeOriginalMultiplayerRuntimeFrame frame={.count=count,.local=0,.death=&match->death,.hud=&match->hud,
        .views=&match->views,.active_props=&match->active,.guards=match->guards,.context=match,
        .select_player=select_player,.produce_camera=camera,.sync_gun_resources=sync_gun,.prepare_local_renderer=prepare_renderer,
        .view_providers={match,select_player,visibility,capture,restore}};
    for(unsigned slot=0;slot<count;++slot)frame.lives[slot]=&match->players[slot].live;
    assert(select_player(match,0)); /* Initial prior viewport, then retain last shuffled. */
    unsigned tick_count=supplies?(combat?2400:480):2;
    int last_item=-1,last_mag=-1,last_reserve=-1,reloads=0,pickup_timer=0;
    PropRecord *picked_prop=NULL;
    unsigned victim_deaths=0,respawns=0,results=0,title=0,executed=0;
    int last_dead=0;float last_health=1.0f;
    struct player *victim_owner=ge_original_multiplayer_player(1);
    PropRecord *victim_prop=victim_owner->prop;
    void *victim_inventory=victim_owner->p_itemcur;
    Model *victim_body=victim_owner->bodyModel;
    GeOriginalMultiplayerInventoryStorage victim_storage;
    assert(ge_original_multiplayer_inventory_storage(1,&victim_storage));
    for(unsigned tick=0;tick<tick_count;++tick){
        GeOriginalInputPad pads[4][2]={0};GeOriginalMultiplayerRegenBatch regen;GeOriginalMultiplayerSessionState session;
        if(supplies){
            /* Original Honey raw-pad route from the fixed canonical spawn:
             * walk toward the authored PP7, tap inventory, then pulse Z.
             * No position, inventory, ammo, hand state or damage is assigned. */
            pads[0][0].stick_y=tick<110?60:0;
            pads[0][0].stick_x=tick<30?20:0;
            pads[0][0].buttons=tick>=128 && tick%16<8?Z_TRIG:0;
            if(tick==100)pads[0][0].buttons=A_BUTTON;
            if(combat){
                pads[0][0].buttons=0;
                if(tick==150)plan_route(match,1,0);
                if(tick>=150 && victim_deaths==0)walk_route(match,pads);
                if(victim_owner->bonddead && tick%16<8)pads[1][0].buttons|=A_BUTTON;
                if(tick==100)pads[0][0].buttons=A_BUTTON;
                if(tick>110 && victim_deaths==0){
                    const struct player *p=ge_original_multiplayer_player(0),*target=ge_original_multiplayer_player(1);
                    const float *look=match->players[0].view.camera_look;
                    float dx=target->prop->pos.x-p->prop->pos.x,dz=target->prop->pos.z-p->prop->pos.z;
                    float angle=atan2f(dx*look[2]-dz*look[0],dx*look[0]+dz*look[2]);
                    if(angle>3.14159265f)angle-=6.28318531f;
                    pads[0][0].stick_x=(int8_t)fmaxf(-80,fminf(80,-angle*240));
                    float distance=sqrtf(dx*dx+dz*dz);
                    if(tick>=240 && match->route_at==match->route_count && distance<400 && fabsf(target->prop->pos.y-p->prop->pos.y)<150 && tick%16<8)
                        pads[0][0].buttons|=Z_TRIG;
                    float desired=atan2f(target->prop->pos.y-60.0f-p->prop->pos.y,distance);
                    float pitch=atan2f(look[1],sqrtf(look[0]*look[0]+look[2]*look[2]));
                    if(desired>3.14159265f)desired-=6.28318531f;
                    if(pitch>3.14159265f)pitch-=6.28318531f;
                    if(desired-pitch>0.025f)pads[0][0].buttons|=D_CBUTTONS;
                    if(desired-pitch< -0.025f)pads[0][0].buttons|=U_CBUTTONS;
                }
            }
        }
        if(supplies && combat && results && tick%16==0)
            for(unsigned slot=0;slot<count;++slot)pads[slot][0].buttons=START_BUTTON;
        assert(ge_port_advance_multiplayer_tick(&match->port,pads,count)==1);
        assert(ge_original_multiplayer_session_tick(match->port.original_clock_timer,(int32_t)tick,&session)==GE_ORIGINAL_MP_SESSION_OK);
        assert(ge_original_gun_live_frame_begin());
        frame.tick=tick;frame.clock_timer=match->port.original_clock_timer;frame.global_timer=match->port.original_global_timer;frame.global_timer_delta=match->port.original_global_timer_delta;
        assert(ge_original_multiplayer_runtime_begin_world(&match->active,frame.clock_timer,&regen)==GE_ORIGINAL_MP_RUNTIME_OK);
        GeOriginalMultiplayerRuntimeStatus status=ge_original_multiplayer_runtime_tick(&frame);
        if(!supplies || status)fprintf(stderr,"native arena %s players %u tick %u slot %u status %d\n",stage->key,count,tick,frame.last_slot,status);
        assert(status==GE_ORIGINAL_MP_RUNTIME_OK);
        if(supplies){
            struct player *player=ge_original_multiplayer_player(0);
            int item=player->hands[0].weaponnum,mag=player->hands[0].weapon_ammo_in_magazine;
            int reserve=player->ammoheldarr[AMMO_9MM];
            if(last_mag==0 && mag>0 && g_playerPlayerData[0].shot_count[0]>0)++reloads;
            if(item!=last_item || mag!=last_mag || reserve!=last_reserve){
                fprintf(stderr,"native Caves %u players tick%u item%d mag%d reserve%d shots%d impacts%d\n",
                    count,tick,item,mag,reserve,g_playerPlayerData[0].shot_count[0],g_NumImpactEntries);
                last_item=item;last_mag=mag;last_reserve=reserve;
            }
            if(!picked_prop)for(size_t i=0;i<match->object_count;++i){
                PropRecord *prop=match->objects[i].prop;
                if(prop->type==PROP_TYPE_WEAPON && prop->weapon->weaponnum==ITEM_WPPKSIL && prop->timetoregen>0){
                    assert(!ge_original_prop_state_is_enabled(prop));
                    picked_prop=prop;pickup_timer=prop->timetoregen;
                    fprintf(stderr,"native pickup command%zu tick%u regeneration%d\n",match->objects[i].command_index,tick,pickup_timer);
                    break;
                }
            }
        }
        if(supplies && combat){
            struct player *target=ge_original_multiplayer_player(1);
            assert(target==victim_owner && target->prop==victim_prop && target->p_itemcur==victim_inventory);
            if(target->bondhealth!=last_health || target->bonddead!=last_dead){
                fprintf(stderr,"native combat %u players tick%u health%.3f dead%d kills%d deathanim%d blood%d fade%.1f\n",count,tick,target->bondhealth,target->bonddead,g_playerPlayerData[0].kill_count,target->deathanimfinished,target->redbloodfinished,target->colourfadetimemax60);
                if(!last_dead && target->bonddead)++victim_deaths;
                if(last_dead && !target->bonddead){
                    ++respawns;
                    GeOriginalMultiplayerInventoryStorage current_storage;
                    assert(ge_original_multiplayer_inventory_storage(1,&current_storage));
                    assert(current_storage.player==victim_storage.player &&
                           current_storage.items==victim_storage.items &&
                           current_storage.capacity==victim_storage.capacity &&
                           current_storage.lifetime==victim_storage.lifetime);
                    assert(target->bondhealth==1.0f && target->bodyModel==victim_body);
                    fprintf(stderr,"native respawn %u players tick%u original player/prop/inventory owners reused\n",count,tick);
                }
                last_health=target->bondhealth;last_dead=target->bonddead;
            }
            if(session.game_over && !results){
                ++results;
                assert(session.results_causes==GE_ORIGINAL_MP_SESSION_CAUSE_TIME);
                for(unsigned slot=0;slot<count;++slot)for(unsigned award=0;award<2;++award){
                    const char *text=NULL;
                    assert(ge_original_multiplayer_watch_award_text(slot,award,&text));
                }
                fprintf(stderr,"native results %u players tick%u cause%u kills%d\n",count,tick,session.results_causes,g_playerPlayerData[0].kill_count);
            }
        }
        assert(ge_original_gun_live_frame_finalize(NULL));
        ++executed;
        if(supplies && combat && results){
            GeOriginalBossState boss;ge_original_boss_snapshot(&boss);
            if(boss.requested_stage==LEVELID_TITLE){
                for(unsigned slot=0;slot<count;++slot)
                    assert(((struct player*)ge_original_multiplayer_player(slot))->mpmenumode==MENU_FINISHED);
                ++title;
                fprintf(stderr,"native match complete %u players tick%u all MENU_FINISHED and original TITLE request\n",count,tick);
                break;
            }
        }
    }
    if(supplies && combat){
        assert(victim_deaths==1 && respawns==1 && results==1 && title==1);
        assert(g_playerPlayerData[0].kill_count==1);
    }
    if(supplies && !combat){
        assert(picked_prop && picked_prop->timetoregen>0 && picked_prop->timetoregen<pickup_timer);
        assert(g_playerPlayerData[0].shot_count[0]==10 && g_NumImpactEntries==10 && reloads==1);
        assert(last_item==ITEM_FIST && last_mag==0 && last_reserve==0);
        assert(match->native_firing_frames>=10 && match->native_visible_hands[0]>0);
        for(int i=0;i<g_NumImpactEntries;++i){
            assert(g_BulletImpactBuffer[i].prop==NULL);
            assert(g_BulletImpactBuffer[i].room>=0 && (size_t)g_BulletImpactBuffer[i].room<match->world.room_count);
        }
        fprintf(stderr,"native Caves %u players:480 frames,26 authored supplies,PP7 pickup,10 shots/impacts,1 reload,ammo depletion; player damage/death/results not asserted\n",count);
    }
    for(unsigned slot=0;slot<count;++slot){
        assert(ge_original_multiplayer_select_player(slot));
        GeOriginalGunLiveStats gun_stats;ge_original_gun_live_snapshot(&gun_stats);
        assert(gun_stats.ticks==executed);
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
    ge_original_guard_bullet_hit_bind_players(NULL,NULL,NULL);ge_original_pp7_fire_bind_object_hit_ready(NULL,NULL);
    for(size_t i=0;i<match->object_count;++i)free(match->objects[i].definition);
    for(size_t i=0;i<match->collision_count;++i)free(match->collision_blocks[i]);
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
    GeAssetPack arena,models,guns,supplies;assert(argc==5 || argc==6 || argc==7);
    if(argc==7){assert(!strcmp(argv[6],"--combat"));combat=1;}
    assert(ge_asset_pack_open(&arena,argv[1])==GE_ASSET_PACK_OK);
    assert(ge_asset_pack_open(&models,argv[2])==GE_ASSET_PACK_OK);
    assert(ge_asset_pack_open(&guns,argv[3])==GE_ASSET_PACK_OK);
    run(&arena,&models,&guns,argv[4],2,NULL);run(&arena,&models,&guns,argv[4],4,NULL);
    if(argc>=6){assert(ge_asset_pack_open(&supplies,argv[5])==GE_ASSET_PACK_OK);
        run(&arena,&models,&guns,argv[4],2,&supplies);run(&arena,&models,&guns,argv[4],4,&supplies);
        ge_asset_pack_close(&supplies);}
    ge_asset_pack_close(&guns);ge_asset_pack_close(&models);ge_asset_pack_close(&arena);return 0;
}
