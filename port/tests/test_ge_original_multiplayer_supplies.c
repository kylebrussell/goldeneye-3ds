/* Reuse the existing real Pitem/model/STAN construction resource harness. */
#define main ge_campaign_pitem_fixture_main
#include "test_ge_original_pitem_models.c"
#undef main
#include "ge_original_multiplayer_rules.h"
#include "game/mp_weapon.h"

extern struct s_mp_weapon_set_text mp_weapon_set_text_table[];

static int selected_players, selected_scenario;
static int fallback_ammo_quantity;
static int no_ammo(void *context,int32_t *type,int32_t *quantity)
{(void)context;*type=AMMO_NONE;*quantity=fallback_ammo_quantity;return 1;}
static size_t selected_items, selected_ammo, selected_armour;
static int32_t multiplayer_players(void *context) {(void)context;return selected_players;}
static int32_t multiplayer_scenario(void *context) {(void)context;return selected_scenario;}

static int multiplayer_construct(void *context,
    const GeOriginalStagePropConstructionRequest *request)
{
    InteractiveHarness *h=context;
    GeOriginalDefaultObjectProviders object_providers={0};
    GeOriginalStageSupplyProviders supply={0};
    GeOriginalStageSupplyInstance instance;
    size_t bytes=ge_original_stage_prop_native_definition_size(request);
    ObjectRecord *object=calloc(1,bytes);
    PropRecord *prop=calloc(1,sizeof(*prop));
    uint8_t state;
    assert(object && prop && bytes);
    assert(ge_original_stage_prop_native_definition_init(request,object,bytes));
    object_providers.context=h;
    object_providers.model_load=interactive_item_model_load;
    object_providers.get_player_count=multiplayer_players;
    object_providers.get_scenario=multiplayer_scenario;
    object_providers.resolve_model_instance=interactive_item_resolve;
    object_providers.allocate_collision=interactive_item_collision;
    object_providers.get_floor_y=interactive_item_floor;
    object_providers.get_room_object_bounds=interactive_item_room_bounds;
    object_providers.walk_tiles=interactive_item_walk;
    object_providers.get_tile_rgb=interactive_item_rgb;
    if(request->record->type==PROPDEF_COLLECTABLE){
        int result=ge_original_stage_item_construct_standard_exact(request,object,
            prop,sizeof(*prop),&object_providers,&h->prepared);
        if(result!=GE_ORIGINAL_STAGE_ITEM_OK)fprintf(stderr,"item %zu model %d: %s\n",request->command_index,request->model_id,ge_original_stage_item_status_name(result));
        assert(result==GE_ORIGINAL_STAGE_ITEM_OK);
        assert(prop->type==PROP_TYPE_WEAPON);
        assert(((WeaponObjRecord *)object)->weaponnum==(int8_t)(request->record->words[32]>>24));
        ++selected_items;
    }else{
        supply.default_object=&object_providers;supply.prepared=&h->prepared;
        supply.update_room_position=supply_update_room;
        supply.activate_prop=supply_activate;supply.enable_prop=supply_enable;
        h->supply_phase=0;
        int result=ge_original_stage_supply_construct_exact(request,object,bytes,
            prop,sizeof(*prop),&supply,&instance);
        if(result!=GE_ORIGINAL_STAGE_SUPPLY_OK)fprintf(stderr,"supply %zu model %d: %s\n",request->command_index,request->model_id,ge_original_stage_supply_status_name(result));
        assert(result==GE_ORIGINAL_STAGE_SUPPLY_OK && instance.constructed && instance.activated);
        if(request->record->type==PROPDEF_AMMO){
            MultiAmmoCrateRecord *ammo=(void *)object;
            int type=request->record->multiplayer_ammo_type;
            assert(type>AMMO_NONE && type<=AMMOTYPE_GLOBAL_MAX);
            assert(ammo->slots[type-1].quantity==request->record->multiplayer_ammo_quantity);
            assert(ammo->slots[type-1].quantity>0);
            ++selected_ammo;
        }else{
            BodyArmourRecord *armour=(void *)object;
            assert(request->record->type==PROPDEF_ARMOUR && armour->amount>0);
            assert(armour->amount==armour->initialamount);
            ++selected_armour;
        }
    }
    assert(ge_original_stage_prop_native_definition_header(object,NULL,&state,NULL));
    assert(!!(state&PROPSTATE_RESPAWN)==!((selected_scenario==SCENARIO_TLD && object->obj==PROP_FLAG)
           ||(selected_scenario==SCENARIO_MWTGG && object->obj==PROP_CHRGOLDEN)));
    assert(object->prop==prop && object->model && prop->stan);
    assert(ge_stan_native_contains_tile(h->stan,prop->stan));
    assert(ge_original_pitem_model_release_instance(h->models,object->model));
    free(object->ptr_allocated_collisiondata_block);free(prop);free(object);
    return 1;
}

int main(int argc,char **argv)
{
    GeAssetPack pack;
    GeOriginalPitemModelStatus status;
    GeOriginalStageSetupRuntime stage={0};
    StageStanHarness stan;
    InteractiveHarness h={0};
    const GeStageAssetDescriptor *descriptor=ge_stage_asset_multiplayer(0);
    uint32_t *raw;
    assert(argc==2 && ge_asset_pack_open(&pack,argv[1])==GE_ASSET_PACK_OK);
    h.models=ge_original_pitem_model_provider_create(&pack,128,8,&status);
    assert(h.models && status==GE_ORIGINAL_PITEM_MODEL_OK);
    assert(ge_original_stage_setup_load(&pack,descriptor,&stage)==GE_ORIGINAL_STAGE_SETUP_OK);
    stage_stan_open(&pack,descriptor,&stage,&stan);h.stan=&stan.native;
    assert(ge_original_stage_setup_bind_stan(&stage,&stan.native)==GE_ORIGINAL_STAGE_SETUP_OK);
    assert(ge_original_stage_setup_publish(&stage));
    raw=malloc(stage.prop_word_count*sizeof(*raw));assert(raw);
    memcpy(raw,stage.propdefs_storage,stage.prop_word_count*sizeof(*raw));
    selected_players=2;
    for(size_t i=0;i<stage.prop_record_count;++i){
        GeOriginalStagePropConstructionRequest request;
        GeOriginalDefaultObjectProviders object_provider={0};
        GeOriginalDefaultObjectPrepared prepared;
        GeOriginalStageSupplyProviders supply={0};
        GeOriginalStageSupplyInstance instance;
        PropRecord prop={0};
        if(stage.prop_records[i].type!=PROPDEF_AMMO)continue;
        assert(ge_original_stage_prop_construction_request(&stage,i,&request));
        size_t bytes=ge_original_stage_prop_native_definition_size(&request);
        void *definition=malloc(bytes);assert(definition);
        object_provider.get_player_count=multiplayer_players;
        supply.default_object=&object_provider;supply.prepared=&prepared;
        supply.get_multiplayer_ammo=no_ammo;
        fallback_ammo_quantity=0;
        assert(ge_original_stage_supply_construct_exact(&request,definition,bytes,&prop,sizeof(prop),&supply,&instance)==GE_ORIGINAL_STAGE_SUPPLY_CANONICAL_NO_OBJECT);
        fallback_ammo_quantity=1;
        assert(ge_original_stage_supply_construct_exact(&request,definition,bytes,&prop,sizeof(prop),&supply,&instance)==GE_ORIGINAL_STAGE_SUPPLY_MULTIPLAYER_AMMO_INVALID);
        free(definition);break;
    }
    for(selected_players=2;selected_players<=4;selected_players+=2)
    for(selected_scenario=SCENARIO_NORMAL;selected_scenario<=SCENARIO_MWTGG;++selected_scenario)
    for(int set=0;set<14;++set){
        GeOriginalStageMultiplayerSetup prepared={0};
        GeOriginalStagePropMaterializerReport report;
        GeOriginalStagePropMaterializerProviders providers={0};
        size_t expected_items=0,expected_ammo=0,expected_armour=0;
        assert(ge_original_stage_multiplayer_setup_prepare(&stage,selected_players,
            selected_scenario,set,&prepared)==GE_ORIGINAL_STAGE_SUPPLY_OK);
        for(size_t i=0;i<stage.prop_record_count;++i){
            const GeOriginalStagePropRecord *r=&prepared.records[i];
            if(r->multiplayer_omitted)continue;
            expected_items+=r->type==PROPDEF_COLLECTABLE;
            expected_ammo+=r->type==PROPDEF_AMMO;
            expected_armour+=r->type==PROPDEF_ARMOUR;
            if(r->type==PROPDEF_COLLECTABLE && r->multiplayer_weapon_slot>=0){
                const struct s_mp_weapon_set *w=&mp_weapon_set_text_table[set].weapon_set[(int)r->multiplayer_weapon_slot];
                assert(r->model_id==w->propID && (int)(r->words[32]>>24)==w->itemID);
                assert((uint16_t)(r->words[0]>>16)==(uint16_t)(w->size*256));
            }
        }
        providers.context=&h;providers.capabilities=GE_ORIGINAL_STAGE_PROP_CAP_ITEM|GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY;
        providers.model_available=interactive_model_available;
        providers.construct_item=multiplayer_construct;providers.construct_special_object=multiplayer_construct;
        selected_items=selected_ammo=selected_armour=0;
        assert(ge_original_stage_prop_materialize_ready(&prepared.view,&providers,&report));
        if(report.failed || report.missing_model || report.missing_service!=6 || report.unsupported_branch){
            fprintf(stderr,"set%d scenario%d report fail%zu model%zu service%zu unsupported%zu\n",set,selected_scenario,report.failed,report.missing_model,report.missing_service,report.unsupported_branch);
            for(size_t i=0;i<stage.prop_record_count;++i){GeOriginalStagePropClassification c=ge_original_stage_prop_classify(&prepared.records[i],&providers);if(c.blocker!=GE_ORIGINAL_STAGE_PROP_READY && c.blocker!=GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY && c.blocker!=GE_ORIGINAL_STAGE_PROP_CANONICAL_OMITTED)fprintf(stderr,"record%zu type%d model%d pad%d blocker %s\n",i,prepared.records[i].type,prepared.records[i].model_id,prepared.records[i].pad_id,ge_original_stage_prop_blocker_name(c.blocker));}
        }
        assert(!report.failed && !report.missing_model && report.missing_service==6 && !report.unsupported_branch);
        assert(report.service_counts[GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR]==6);
        assert(report.canonical_omitted==prepared.omitted);
        assert(selected_items==expected_items && selected_ammo==expected_ammo && selected_armour==expected_armour);
        assert(memcmp(raw,stage.propdefs_storage,stage.prop_word_count*sizeof(*raw))==0);
        assert(prepared.view.setup==stage.setup);
        printf("Temple %d players scenario %d set %d: %zu weapons, %zu ammo, %zu armour, %zu canonical omissions\n",
            selected_players,selected_scenario,set,selected_items,selected_ammo,selected_armour,report.canonical_omitted);
        ge_original_stage_multiplayer_setup_close(&prepared);
    }
    /* Every arena preserves the same authored last-slot contract. */
    for(size_t arena=0;arena<GE_MULTIPLAYER_STAGE_COUNT;++arena){
        GeOriginalStageSetupRuntime other={0};
        assert(ge_original_stage_setup_load(&pack,ge_stage_asset_multiplayer(arena),&other)==GE_ORIGINAL_STAGE_SETUP_OK);
        for(int set=0;set<14;++set){
            GeOriginalStageMultiplayerSetup view={0};
            assert(ge_original_stage_multiplayer_setup_prepare(&other,2,SCENARIO_NORMAL,set,&view)==GE_ORIGINAL_STAGE_SUPPLY_OK);
            assert(view.weapon_slots>0 && view.ammo_crates>0);
            ge_original_stage_multiplayer_setup_close(&view);
        }
        ge_original_stage_setup_close(&other);
    }
    free(raw);stage_stan_close(&stan);ge_original_stage_setup_close(&stage);
    ge_original_pitem_model_provider_destroy(h.models);ge_asset_pack_close(&pack);
    return 0;
}
