#include "ge_original_stage_setup.h"
#include "ge_original_dam_intro.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_stage_assets.h"

#include "bondtypes.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MaterializerHarness {
    size_t calls;
    size_t previous_index;
    size_t ordinary_pads;
    size_t bound_pads;
    int32_t first_model;
    int32_t first_pad;
    uint64_t placement_hash;
    size_t service_calls[GE_ORIGINAL_STAGE_PROP_SERVICE_COUNT];
} MaterializerHarness;

static uint64_t hash_bytes(uint64_t hash, const void *data, size_t size)
{
    const uint8_t *bytes = data;
    while (size-- != 0U) {
        hash ^= *bytes++;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int model_available(void *context, int32_t model_id)
{
    (void)context;
    return model_id >= 0;
}

static int model_available_except_20(void *context, int32_t model_id)
{
    (void)context;
    return model_id >= 0 && model_id != 20;
}

static int construct_ready(void *context, GeOriginalStagePropService service,
                           const GeOriginalStagePropRecord *record,
                           size_t command_index)
{
    MaterializerHarness *harness = context;
    assert(service == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT);
    assert(record != NULL && (record->type == PROPDEF_PROP
                              || record->type == PROPDEF_GLASS));
    assert(harness->calls == 0U || command_index > harness->previous_index);
    harness->previous_index = command_index;
    ++harness->calls;
    return 1;
}

static int construct_default_request(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    MaterializerHarness *harness = context;
    const size_t definition_size =
        ge_original_stage_prop_native_definition_size(request);
    ObjectRecord *definition;
    PropRecord *prop;
    uint16_t extrascale;
    uint8_t state;
    uint8_t type;
    assert(request != NULL && request->runtime != NULL
           && request->record != NULL);
    assert(request->service == GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT);
    assert(request->record->type == PROPDEF_PROP
           || request->record->type == PROPDEF_GLASS);
    assert(request->model_id == request->record->model_id
           && request->pad_id == request->record->pad_id);
    assert(request->flags == request->record->words[2]
           && request->flags2 == request->record->words[3]
           && request->runtime_flags == request->record->words[25]);
    assert(request->placement_resolved != 0U
           && request->placement.has_stan != 0U
           && request->placement.stan != NULL
           && request->placement.plink != NULL
           && request->placement.room >= 0);
    assert(definition_size == sizeof(ObjectRecord));
    definition = malloc(definition_size);
    prop = malloc(ge_original_stage_prop_native_prop_size());
    assert(definition != NULL && prop != NULL);
    assert(ge_original_stage_prop_native_definition_init(
        request, definition, definition_size));
    assert(ge_original_stage_prop_native_definition_header(
        definition, &extrascale, &state, &type));
    assert(extrascale == (uint16_t)(request->record->words[0] >> 16U)
           && state == (uint8_t)(request->record->words[0] >> 8U)
           && type == request->record->type);
    assert(definition->obj == request->model_id
           && definition->pad == request->pad_id
           && definition->flags == request->flags
           && definition->flags2 == request->flags2
           && definition->runtime_bitflags == request->runtime_flags
           && definition->prop == NULL && definition->model == NULL);
    assert(ge_original_stage_prop_native_bind_prop(
        request, definition, prop,
        ge_original_stage_prop_native_prop_size()));
    assert(definition->prop == prop && prop->obj == definition
           && prop->stan == request->placement.stan
           && prop->rooms[0] == (uint8_t)request->placement.room);
    assert(ge_original_stage_prop_native_definition_set_state(
        definition, (uint8_t)(state ^ 1U)));
    assert(ge_original_stage_prop_native_definition_header(
        definition, NULL, &type, NULL) && type == (uint8_t)(state ^ 1U));
    free(definition);
    free(prop);
    assert(harness->calls == 0U
           || request->command_index > harness->previous_index);
    if (harness->calls == 0U) {
        harness->first_model = request->model_id;
        harness->first_pad = request->pad_id;
        harness->placement_hash = UINT64_C(1469598103934665603);
    }
    harness->previous_index = request->command_index;
    harness->ordinary_pads += request->placement.is_bound_pad == 0U;
    harness->bound_pads += request->placement.is_bound_pad != 0U;
    harness->placement_hash = hash_bytes(harness->placement_hash,
        &request->command_index, sizeof(request->command_index));
    harness->placement_hash = hash_bytes(harness->placement_hash,
        &request->model_id, sizeof(request->model_id));
    harness->placement_hash = hash_bytes(harness->placement_hash,
        &request->pad_id, sizeof(request->pad_id));
    harness->placement_hash = hash_bytes(harness->placement_hash,
        &request->placement.room, sizeof(request->placement.room));
    harness->placement_hash = hash_bytes(harness->placement_hash,
        request->placement.position, sizeof(request->placement.position));
    ++harness->service_calls[request->service];
    ++harness->calls;
    return 1;
}

static int construct_nondefault_request(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    MaterializerHarness *harness = context;
    assert(request != NULL && request->runtime != NULL
           && request->record != NULL
           && request->model_id == request->record->model_id
           && request->pad_id == request->record->pad_id);
    assert(request->service == GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR
           || request->service == GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD
           || request->service == GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM);
    if (request->pad_id >= 0) {
        assert(request->placement_resolved != 0U
               && request->placement.plink != NULL);
    }
    ++harness->service_calls[request->service];
    ++harness->calls;
    return 1;
}

static stagesetup *original_pad_load_setup(void *context, int32_t stage_id)
{
    GeOriginalStageSetupRuntime *setup = context;
    assert(stage_id == setup->descriptor->level_id);
    return setup->setup;
}

static float original_pad_scale(void *context)
{
    return 1.0f / ((GeOriginalStageSetupRuntime *)context)->descriptor->level_scale;
}

static void exercise_original_pad_bootstrap(GeAssetPack *pack)
{
    for (int stage = 0; stage < GE_STAGE_COUNT; ++stage) {
        GeOriginalStageSetupRuntime setup = {0};
        GeOriginalSetupPadState state;
        assert(ge_original_stage_setup_load(pack, ge_stage_asset_descriptor(stage), &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        const size_t pad_bytes = (setup.pad_count + 1U) * sizeof(PadRecord);
        const size_t bound_bytes = (setup.boundpad_count + 1U) * sizeof(BoundPadRecord);
        void *pads = malloc(pad_bytes), *boundpads = malloc(bound_bytes);
        assert(pads && boundpads);
        memcpy(pads, setup.pads_storage, pad_bytes);
        memcpy(boundpads, setup.boundpads_storage, bound_bytes);
        GeOriginalSetupPadProviders providers = {
            .context = &setup, .load_setup = original_pad_load_setup,
            .get_room_scale_reciprocal = original_pad_scale,
        };
        for (unsigned restart = 0U; restart < 3U; ++restart) {
            assert(ge_original_stage_setup_prepare_original_pad_load(&setup));
            ge_original_setup_pad_bind(&providers, &state);
            ge_original_setup_pad_load(setup.descriptor->level_id);
            assert(state.loaded && state.pad_count == setup.pad_count);
            assert(memcmp(pads, setup.pads_storage, pad_bytes) == 0);
            assert(memcmp(boundpads, setup.boundpads_storage, bound_bytes) == 0);
        }
        ge_original_setup_pad_bind(NULL, NULL);
        free(boundpads); free(pads);
        ge_original_stage_setup_close(&setup);
    }
    puts("all-stage original pad bootstrap: exact once-only scaling and restart round trips");
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    const GeAssetPackEntry *collision_entry;
    uint8_t *collision;
    GeStanCollisionSurface surface;
    GeOriginalStageSetupRuntime setup;
    GeOriginalStageSetupRuntime dam_setup;
    GeOriginalStageSpawn spawn = {0};
    GeStanNativeMap native;
    stagesetup *original;
    void *native_storage;
    size_t native_size;
    float x;
    float z;
    float floor;
    const GeOriginalStagePropRecord *record;
    MaterializerHarness materializer_harness = {0};
    GeOriginalStagePropMaterializerProviders materializer_providers = {
        .context = &materializer_harness,
        .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT,
        .model_available = model_available,
        .construct = construct_ready,
    };
    GeOriginalStagePropMaterializerReport materializer_report;
    int32_t model_dependencies[16];

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    exercise_original_pad_bootstrap(&pack);
    assert(ge_original_stage_setup_load(&pack, ge_stage_asset_dam(), &dam_setup)
        == GE_ORIGINAL_STAGE_SETUP_OK);
    assert(dam_setup.pad_count == 367U && dam_setup.boundpad_count == 95U);
    assert(dam_setup.prop_record_count == 329U
           && dam_setup.prop_word_count == 9565U);
    original = ge_original_stage_setup_get(&dam_setup);
    assert(original != NULL);
    assert(original->pads[367].plink == NULL
           && original->pads[367].stan == NULL);
    assert(original->boundpads[95].plink == NULL
           && original->boundpads[95].stan == NULL);
    assert(ge_original_stage_setup_normal_spawn(&dam_setup, &spawn));
    assert(spawn.pad_id == 33 && strcmp(spawn.plink, "p6g1") == 0);
    ge_original_stage_setup_close(&dam_setup);
    memset(&spawn, 0, sizeof(spawn));
    collision_entry = ge_asset_pack_find(&pack,
        ge_stage_asset_facility()->collision_path);
    assert(collision_entry != NULL
           && collision_entry->data_size
               == ge_stage_asset_facility()->expected_collision_size);
    collision = malloc((size_t)collision_entry->data_size);
    assert(collision != NULL);
    assert(ge_asset_pack_read(&pack, ge_stage_asset_facility()->collision_path,
        collision, (size_t)collision_entry->data_size, NULL) == GE_ASSET_PACK_OK);
    assert(ge_stan_collision_open(collision, (size_t)collision_entry->data_size,
        &surface) == GE_STAN_COLLISION_OK);
    assert(ge_original_stage_setup_load(&pack, ge_stage_asset_facility(), &setup)
        == GE_ORIGINAL_STAGE_SETUP_OK);
    assert(setup.loaded != 0U && setup.source_size == 96160U);
    assert(setup.source_fnv1a64 == UINT64_C(0xcb7dcf0c0e6d5f55));
    assert(setup.pad_count == 311U);
    assert(setup.boundpad_count == 135U);
    assert(setup.relocated == (GE_ORIGINAL_STAGE_SETUP_PADS
        | GE_ORIGINAL_STAGE_SETUP_BOUNDPADS | GE_ORIGINAL_STAGE_SETUP_INTRO
        | GE_ORIGINAL_STAGE_SETUP_PATHS | GE_ORIGINAL_STAGE_SETUP_AI
        | GE_ORIGINAL_STAGE_SETUP_PROPDEFS));
    assert(setup.waypoint_count == 157U && setup.waygroup_count == 22U);
    assert(setup.patrolpath_count == 7U && setup.ailist_count == 56U);
    original = ge_original_stage_setup_get(&setup);
    assert(original != NULL && original->pads != NULL
           && original->boundpads != NULL && original->intro != NULL);
    assert(original->pathwaypoints != NULL && original->waypointgroups != NULL
           && original->patrolpaths != NULL && original->ailists != NULL);
    assert(original->propDefs == (PropDefHeaderRecord *)setup.propdefs_storage);
    assert(setup.prop_record_count == 524U && setup.prop_word_count == 14033U);
    assert(ge_original_stage_setup_prop_type_count(&setup, PROPDEF_DOOR) == 46U);
    assert(ge_original_stage_setup_prop_type_count(&setup, PROPDEF_PROP) == 77U);
    assert(ge_original_stage_setup_prop_type_count(&setup, PROPDEF_GUARD) == 65U);
    assert(ge_original_stage_setup_prop_type_count(
        &setup, PROPDEF_COLLECTABLE) == 65U);
    assert(ge_original_stage_setup_prop_type_count(&setup, PROPDEF_TAG) == 45U);
    assert(ge_original_stage_setup_prop_type_count(
        &setup, PROPDEF_TINTED_GLASS) == 20U);
    record = ge_original_stage_setup_prop_record(&setup, 6U);
    assert(record != NULL && record->type == PROPDEF_OBJECTIVE_ENTER_ROOM
           && record->words[1] == 86U
           && record->objective == ge_original_stage_setup_prop_record(&setup, 5U));
    record = ge_original_stage_setup_prop_record(&setup, 32U);
    assert(record != NULL && record->type == PROPDEF_DOOR
           && record->word_count == 64U && record->model_id == 159
           && record->pad_id == 66);
    record = ge_original_stage_setup_prop_record(&setup, 17U);
    assert(record != NULL
           && record->type == PROPDEF_OBJECTIVE_DESTROY_OBJECT
           && record->objective
               == ge_original_stage_setup_prop_record(&setup, 16U)
           && record->relations[0] != NULL
           && record->relations[0]->type == PROPDEF_TAG
           && record->relations[0]->tag_id == 1U);
    record = ge_original_stage_setup_prop_record(&setup, 59U);
    assert(record != NULL && record->type == PROPDEF_TAG
           && record->tag_id == 34U && record->relation_offsets[0] == -2
           && record->relations[0]
               == ge_original_stage_setup_prop_record(&setup, 57U));
    record = ge_original_stage_setup_prop_record(&setup, 65U);
    assert(record != NULL && record->type == PROPDEF_SWITCH
           && record->relations[0]
               == ge_original_stage_setup_prop_record(&setup, 64U)
           && record->relations[1]
               == ge_original_stage_setup_prop_record(&setup, 63U));
    record = ge_original_stage_setup_prop_record(&setup, 126U);
    assert(record != NULL && record->type == PROPDEF_COLLECTABLE
           && record->model_id == 184 && record->pad_id == -1);
    record = ge_original_stage_setup_prop_record(&setup, 315U);
    assert(record != NULL && record->type == PROPDEF_GUARD
           && record->chr_id == 0 && record->pad_id == 254
           && record->model_id == 2 && record->ai_list_id == 2);
    record = ge_original_stage_setup_prop_record(&setup, 523U);
    assert(record != NULL && record->type == PROPDEF_END
           && record->word_count == 1U);
    assert(ge_original_stage_prop_materialize_ready(&setup,
        &materializer_providers, &materializer_report));
    assert(materializer_report.records == 524U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL] == 118U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT] == 109U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR] == 46U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD] == 65U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM] == 118U);
    assert(materializer_report.service_counts[
        GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT] == 68U);
    /* Before setup STAN binding, domakedefaultobj's getposstan branch skips
     * every authored object. The materializer must not construct an object at
     * a pad whose canonical collision locus is still unresolved. */
    assert(materializer_report.ready == 0U
           && materializer_report.constructed == 0U
           && materializer_harness.calls == 0U);
    assert(materializer_report.control_only == 118U
           && materializer_report.missing_service == 229U
           && materializer_report.unsupported_branch == 177U
           && materializer_report.missing_model == 0U
           && materializer_report.failed == 0U);
    assert(ge_original_stage_prop_model_dependencies(&setup,
        GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT,
        model_dependencies, 16U) == 12U);
    assert(model_dependencies[0] == 82 && model_dependencies[1] == 20
           && model_dependencies[11] == 96);
    assert(ge_original_stage_prop_model_dependencies(&setup,
        GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR,
        model_dependencies, 16U) == 4U);
    assert(model_dependencies[0] == 159 && model_dependencies[1] == 155
           && model_dependencies[2] == 158 && model_dependencies[3] == 160);
    assert(ge_original_stage_prop_model_dependencies(&setup,
        GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD,
        model_dependencies, 16U) == 3U);
    assert(model_dependencies[0] == 2 && model_dependencies[1] == 35
           && model_dependencies[2] == 9);
    assert(ge_original_stage_prop_model_dependencies(&setup,
        GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM,
        model_dependencies, 16U) == 6U);
    assert(model_dependencies[0] == 184 && model_dependencies[5] == 217);
    assert(ge_original_stage_prop_model_dependencies(&setup,
        GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT,
        model_dependencies, 16U) == 10U);
    assert(model_dependencies[0] == 337 && model_dependencies[9] == 116);
    memset(&materializer_harness, 0, sizeof(materializer_harness));
    materializer_providers.model_available = model_available_except_20;
    assert(ge_original_stage_prop_materialize_ready(&setup,
        &materializer_providers, &materializer_report));
    assert(materializer_report.ready == 0U
           && materializer_report.constructed == 0U
           && materializer_report.missing_model == 14U
           && materializer_report.unsupported_branch == 163U);
    materializer_providers.model_available = model_available;
    assert(original->pads[311].plink == NULL && original->pads[311].stan == NULL);
    assert(original->boundpads[135].plink == NULL
           && original->boundpads[135].stan == NULL);
    assert(original->pathwaypoints[0].padID == 0x9c);
    assert(original->pathwaypoints[0].neighbours[0] == 1);
    assert(original->waypointgroups[0].waypoints[0] == 0);
    assert(original->patrolpaths[0].ID == 0U);
    assert(original->ailists[0].ID == 0x401);
    assert(ge_stan_native_required_size(&surface, &native_size)
        == GE_STAN_COLLISION_OK);
    native_storage = malloc(native_size);
    assert(native_storage != NULL);
    assert(ge_stan_native_materialize(&surface,
        ge_stage_asset_facility()->level_scale,
        native_storage, native_size, &native) == GE_STAN_COLLISION_OK);
    assert(ge_original_stage_setup_bind_stan(&setup, &native)
        == GE_ORIGINAL_STAGE_SETUP_OK);
    assert(setup.pad_stan_count == 311U);
    assert(setup.bound_pad_stan_count == 135U);
    assert(ge_original_stage_setup_normal_spawn(&setup, &spawn));
    assert(spawn.pad_id == 167);
    assert(fabsf(spawn.position[0]
            - 137.0f / ge_stage_asset_facility()->level_scale) < 0.001f
        && fabsf(spawn.position[1]
            - 562.0f / ge_stage_asset_facility()->level_scale) < 0.001f
        && fabsf(spawn.position[2]
            - -1154.0f / ge_stage_asset_facility()->level_scale) < 0.001f);
    assert(spawn.up[0] == 0.0f && spawn.up[1] == 1.0f && spawn.up[2] == 0.0f);
    assert(spawn.look[0] == -1.0f && spawn.look[1] == 0.0f
           && spawn.look[2] == 0.0f);
    assert(strcmp(spawn.plink, "p1682a1") == 0);
    assert(spawn.stan == native.spawn_tile);
    assert(((GeStanNativeTile *)spawn.stan)->room == 13U);
    assert(ge_stan_native_tile_id(spawn.stan) == 0x00069201U);
    x = spawn.position[0];
    z = spawn.position[2];
    assert(ge_original_stan_test_point_within_bounds(
        &native, spawn.stan, x, z));
    floor = ge_original_stan_get_position_y(&native, spawn.stan, x, z);
    assert(fabsf(floor
            - 272.0f / ge_stage_asset_facility()->level_scale) < 0.001f);
    memset(&materializer_harness, 0, sizeof(materializer_harness));
    materializer_providers.capabilities =
        GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT;
    materializer_providers.model_available = model_available;
    materializer_providers.construct = NULL;
    materializer_providers.construct_default_object =
        construct_default_request;
    assert(ge_original_stage_prop_materialize_ready(&setup,
        &materializer_providers, &materializer_report));
    assert(materializer_report.ready == 109U
           && materializer_report.constructed == 109U
           && materializer_harness.calls == 109U);
    assert(materializer_harness.ordinary_pads
               + materializer_harness.bound_pads == 109U);
    assert(materializer_harness.first_model == 82
           && materializer_harness.first_pad == 207);
    printf("facility default placement hash: %016llx (%zu ordinary, "
           "%zu bound; first model %d pad %d)\n",
           (unsigned long long)materializer_harness.placement_hash,
           materializer_harness.ordinary_pads,
           materializer_harness.bound_pads,
           materializer_harness.first_model, materializer_harness.first_pad);
    fflush(stdout);
    assert(materializer_harness.placement_hash
           == UINT64_C(0x17964d49f36360cd));
    memset(&materializer_harness, 0, sizeof(materializer_harness));
    materializer_providers.capabilities =
        GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT
        | GE_ORIGINAL_STAGE_PROP_CAP_DOOR
        | GE_ORIGINAL_STAGE_PROP_CAP_GUARD
        | GE_ORIGINAL_STAGE_PROP_CAP_ITEM;
    materializer_providers.construct_door = construct_nondefault_request;
    materializer_providers.construct_guard = construct_nondefault_request;
    materializer_providers.construct_item = construct_nondefault_request;
    assert(ge_original_stage_prop_materialize_ready(&setup,
        &materializer_providers, &materializer_report));
    assert(materializer_report.ready == 338U
           && materializer_report.constructed == 338U
           && materializer_report.failed == 0U);
    assert(materializer_harness.service_calls[
               GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT] == 109U);
    assert(materializer_harness.service_calls[
               GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR] == 46U);
    assert(materializer_harness.service_calls[
               GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD] == 65U);
    assert(materializer_harness.service_calls[
               GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM] == 118U);
    assert(!ge_original_stage_setup_normal_spawn(&setup, NULL));
    ge_original_stage_setup_close(&setup);
    assert(setup.loaded == 0U && setup.setup == NULL && setup.source_blob == NULL);
    free(collision);
    collision = NULL;
    free(native_storage);
    native_storage = NULL;
    {
        const GeStageAssetDescriptor *runway =
            ge_stage_asset_descriptor(GE_STAGE_RUNWAY);
        collision_entry = ge_asset_pack_find(&pack, runway->collision_path);
        assert(collision_entry != NULL);
        collision = malloc((size_t)collision_entry->data_size);
        assert(collision != NULL);
        assert(ge_asset_pack_read(&pack, runway->collision_path, collision,
            (size_t)collision_entry->data_size, NULL) == GE_ASSET_PACK_OK);
        assert(ge_stan_collision_open(collision,
            (size_t)collision_entry->data_size, &surface)
            == GE_STAN_COLLISION_OK);
        assert(ge_stan_native_required_size(&surface, &native_size)
            == GE_STAN_COLLISION_OK);
        native_storage = malloc(native_size);
        assert(native_storage != NULL);
        assert(ge_stan_native_materialize(&surface, runway->level_scale,
            native_storage, native_size, &native) == GE_STAN_COLLISION_OK);
        assert(ge_original_stage_setup_load(&pack, runway, &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        assert(ge_original_stage_setup_bind_stan(&setup, &native)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        memset(&materializer_harness, 0, sizeof(materializer_harness));
        materializer_providers.capabilities =
            GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT;
        assert(ge_original_stage_prop_materialize_ready(&setup,
            &materializer_providers, &materializer_report));
        assert(materializer_report.constructed == materializer_report.ready
               && materializer_report.constructed
                    == materializer_harness.calls);
        assert(materializer_harness.ordinary_pads
                   + materializer_harness.bound_pads
               == materializer_harness.calls);
        assert(materializer_harness.calls == 14U
               && materializer_harness.ordinary_pads == 13U
               && materializer_harness.bound_pads == 1U);
        printf("runway default placement hash: %016llx (%zu records, "
               "%zu ordinary, %zu bound)\n",
               (unsigned long long)materializer_harness.placement_hash,
               materializer_harness.calls,
               materializer_harness.ordinary_pads,
               materializer_harness.bound_pads);
        assert(materializer_harness.placement_hash
               == UINT64_C(0xa5e7325d7435c6e9));
        ge_original_stage_setup_close(&setup);
    }
    for (int stage = 0; stage < GE_STAGE_COUNT; ++stage) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage);
        GeOriginalStageSetupStatus status = ge_original_stage_setup_load(
            &pack, descriptor, &setup);
        if (status != GE_ORIGINAL_STAGE_SETUP_OK) {
            fprintf(stderr, "%s setup relocation: %s\n", descriptor->key,
                    ge_original_stage_setup_status_name(status));
        }
        assert(status == GE_ORIGINAL_STAGE_SETUP_OK);
        if (descriptor->stage == GE_STAGE_CUBA) {
            const CreditsEntry *credits;
            size_t credits_count = 0U;
            credits = ge_original_stage_setup_credits(
                &setup, &credits_count);
            assert(setup.boundpad_count == 0U
                && setup.waypoint_count == 0U
                && setup.waygroup_count == 0U
                && setup.patrolpath_count == 0U);
            assert(credits != NULL && credits_count == 271U);
            assert(credits[0].TextId1 == 0x5010U
                && credits[0].TextId2 == 0x5011U
                && credits[0].Position1 == 220
                && credits[0].Alignment1 == 2U
                && credits[0].Position2 == -1
                && credits[0].Alignment2 == UINT16_MAX);
            assert(credits[credits_count].TextId1 == 0U
                && credits[credits_count].TextId2 == 0U);
        }
        assert(setup.prop_record_count != 0U
               && setup.prop_records[setup.prop_record_count - 1U].type
                   == PROPDEF_END);
        memset(&materializer_report, 0, sizeof(materializer_report));
        materializer_providers.capabilities = 0U;
        assert(ge_original_stage_prop_materialize_ready(&setup,
            &materializer_providers, &materializer_report));
        assert(materializer_report.records
            == materializer_report.control_only
             + materializer_report.missing_service
             + materializer_report.missing_model
             + materializer_report.unsupported_branch);
        printf("%s prop frontier: %zu records, %zu control, %zu default, "
               "%zu door, %zu guard, %zu item, %zu special\n",
               descriptor->key, materializer_report.records,
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL],
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_DEFAULT_OBJECT],
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_DOOR],
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_GUARD],
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_ITEM],
               materializer_report.service_counts[
                   GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT]);
        ge_original_stage_setup_close(&setup);
    }
    ge_asset_pack_close(&pack);
    free(collision);
    free(native_storage);
    puts("Facility packaged original setup/spawn/STAN relocation passed");
    return 0;
}
