/* Reuse the real packed-model/STAN providers; this executable exercises native
 * room publication separately from the large rendering fixture's main. */
#define main pitem_fixture_main
#include "test_ge_original_pitem_models.c"
#undef main
#include "ge_original_prop_state.h"
#include "game/chrai.h"

extern PropRecord g_Props[MAX_PROPS];
extern void ge_door_collision_roomGetProps(s32 *rooms);
extern s32 ge_door_collision_chrobjTestPolygonsTouchingOrOverlap2D(rect4f *,s32,rect4f *,s32);
static s16 lookup[MAX_PROPS+1];
s16 *ptr_list_object_lookup_indices=lookup;
u32 num_obj_position_data_entries;
static GeOriginalPropState pool;
static unsigned registrations;
static void register_room(void *context,void *prop,int16_t room) {
    (void)context;++registrations;
    ge_original_prop_state_register_room(&pool,prop,room);
}
static unsigned room_count(int room,PropRecord *wanted) {
    s32 rooms[]={room,-1};unsigned found=0;
    ge_door_collision_roomGetProps(rooms);
    for(s16 *i=lookup;*i>=0;i++)if(&g_Props[*i]==wanted)found++;
    return found;
}
static int overlap_query(PropRecord *source,PropRecord *target) {
    s32 rooms[8];unsigned i;
    for(i=0;source->rooms[i]!=255;i++)rooms[i]=source->rooms[i];
    rooms[i]=-1;ge_door_collision_roomGetProps(rooms);
    for(s16 *index=lookup;*index>=0;index++) {
        if(&g_Props[*index]!=target)continue;
        collision_data *a=source->obj->ptr_allocated_collisiondata_block;
        collision_data *b=target->obj->ptr_allocated_collisiondata_block;
        return a->bottom<=b->top && b->bottom<=a->top &&
            ge_door_collision_chrobjTestPolygonsTouchingOrOverlap2D(
                (rect4f*)a->polygon,a->edges,(rect4f*)b->polygon,b->edges);
    }
    return 0;
}
static PropRecord *collision_truck;
static int runtime_collision(void *context,void *prop) {
    (void)context;return !overlap_query(prop,collision_truck);
}
int main(int argc,char **argv) {
    assert(argc==2);GeAssetPack pack;GeOriginalStageSetupRuntime setup;
    GeOriginalPitemModelStatus status;StageStanHarness stan;
    assert(!ge_asset_pack_open(&pack,argv[1]));
    assert(!ge_original_stage_setup_load(&pack,ge_stage_asset_dam(),&setup));
    stage_stan_open(&pack,ge_stage_asset_dam(),&setup,&stan);
    g_CurrentSetup=*setup.setup;assert(ge_original_prop_state_reset(&pool,256));
    CanonicalDoorHarness h={.stan=&stan.native};
    h.models=ge_original_pitem_model_provider_create(&pack,8,8,&status);assert(h.models);
    GeOriginalDoorPrepared prepared;
    GeOriginalDoorProviders p={.context=&h,.model_load=canonical_door_model_load,
        .resolve_model_instance=canonical_door_resolve_model,
        .allocate_collision=canonical_door_allocate_collision,
        .walk_tiles=canonical_door_walk,.get_tile_rgb=canonical_door_tile_rgb,
        .find_portal=canonical_door_find_portal,.set_portal_open=canonical_door_set_portal_open,
        .register_room=register_room};
    ge_original_door_bind(&p,&prepared);
    GeOriginalStagePropConstructionRequest request;
    assert(ge_original_stage_prop_construction_request(&setup,267,&request));
    size_t size=ge_original_stage_prop_native_definition_size(&request);
    DoorRecord *door=calloc(1,size);assert(door);
    assert(ge_original_stage_prop_native_definition_init(&request,door,size));
    PropRecord *prop=ge_original_prop_state_allocate(&pool,door);assert(prop);
    assert(ge_original_stage_prop_native_bind_prop(&request,door,prop,sizeof(*prop)));
    assert(ge_original_door_construct(door,267)==GE_ORIGINAL_DOOR_OK);

    assert(prop->rooms[0]==111 && prop->rooms[1]==113 && registrations==2);
    assert(room_count(111,prop)==1 && room_count(113,prop)==1);
    ge_original_prop_state_activate(&pool,prop);ge_original_prop_state_enable(&pool,prop);
    /* Close the authored-open gate through the unchanged native tick. */
    GeOriginalDoorRuntimeState state;
    GeOriginalDoorRuntimeProviders runtime={.context=&h,
        .global_timer=canonical_door_global_timer,.clock_timer=canonical_door_clock_timer,
        .test_collision=canonical_door_collision_test,.sound_event=canonical_door_sound_event};
    h.clock_timer=4;h.global_timer=1;ge_original_door_runtime_bind(&runtime,&state);
    door->openPosition=.001f;door->openstate=DOORSTATE_CLOSING;door->speed=-.01f;
    assert(!ge_original_door_runtime_tick(door));assert(door->openPosition==0);
    /* Exact hardware truck hull, independently reproduced from the real model
     * and Dam STAN using canonical terrain alignment (2026-09-12 witness). */
    ObjectRecord truck={0};collision_data hull={.edges=4,.bottom=-106.850967f,.top=254.738327f,
        .polygon={{16936.0547f,8738.36426f},{17230.9629f,8735.71289f},
                  {17237.8926f,9506.61035f},{16942.9844f,9509.26172f}}};
    PropRecord *truckprop=ge_original_prop_state_allocate(&pool,&truck);assert(truckprop);
    truckprop->obj=&truck;truck.ptr_allocated_collisiondata_block=&hull;
    truckprop->rooms[0]=111;truckprop->rooms[1]=255;
    ge_original_prop_state_register_room(&pool,truckprop,111);
    assert(overlap_query(prop,truckprop));
    assert(overlap_query(truckprop,prop)); /* Failed before primary publication. */
    /* A truck stopped before the gate is clear in both query directions. */
    for(unsigned i=0;i<4;i++)hull.polygon[i].y+=10;
    assert(!overlap_query(prop,truckprop) && !overlap_query(truckprop,prop));
    /* Proposed next truck movement is rejected by the primary-room query,
     * preserving the clear stopping hull instead of entering the closed gate. */
    for(unsigned i=0;i<4;i++)hull.polygon[i].y-=10;
    assert(overlap_query(truckprop,prop));
    for(unsigned i=0;i<4;i++)hull.polygon[i].y+=10;
    collision_truck=truckprop;runtime.test_collision=runtime_collision;
    ge_original_door_runtime_bind(&runtime,&state);
    door->openstate=DOORSTATE_OPENING;
    for(unsigned i=0;i<1000 && door->openstate!=DOORSTATE_STATIONARY;i++) {
        ++h.global_timer;assert(!ge_original_door_runtime_tick(door));
    }
    assert(door->openstate==DOORSTATE_STATIONARY && door->openPosition==door->maxFrac);
    for(unsigned i=0;i<4;i++)hull.polygon[i].y-=100;
    assert(!overlap_query(truckprop,prop)); /* Truck can now pass the open gate. */
    assert(ge_original_prop_state_release(&pool,prop));
    assert(room_count(111,prop)==0);assert(ge_original_door_release(door));
    assert(ge_original_pitem_model_release_instance(h.models,door->model));free(door);
    /* A door without portal flags publishes its primary room exactly once. */
    registrations=0;ge_original_door_bind(&p,&prepared);
    door=calloc(1,size);assert(ge_original_stage_prop_native_definition_init(&request,door,size));
    door->flags &= ~(PROPFLAG_CULL_BEHIND_DOOR | PROPFLAG_NO_PORTAL_CLOSE);
    prop=ge_original_prop_state_allocate(&pool,door);assert(prop);
    assert(ge_original_stage_prop_native_bind_prop(&request,door,prop,sizeof(*prop)));
    assert(!ge_original_door_construct(door,267));
    assert(registrations==1 && room_count(111,prop)==1 && prop->rooms[1]==255);
    ge_original_prop_state_activate(&pool,prop);ge_original_prop_state_enable(&pool,prop);
    assert(ge_original_prop_state_release(&pool,prop));
    assert(room_count(111,prop)==0 && room_count(113,prop)==0);
    assert(ge_original_door_release(door));
    assert(ge_original_pitem_model_release_instance(h.models,door->model));free(door);
    for(size_t i=0;i<h.collision_count;i++)free(h.collisions[i]);
    ge_original_door_runtime_bind(NULL,NULL);ge_original_door_bind(NULL,NULL);
    ge_original_pitem_model_provider_destroy(h.models);
    stage_stan_close(&stan);ge_original_stage_setup_close(&setup);ge_asset_pack_close(&pack);
    assert(ge_original_prop_state_end(&pool));
    puts("Real Dam gate: primary/secondary room publication, bidirectional truck collision and cleanup passed");
    return 0;
}
