#include "ge_asset_pack.h"
#include "ge_original_stage_setup.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_special_objects.h"
#include "ge_original_stage_safe_runtime.h"
#include "ge_stage_assets.h"
#include "ge_stan_collision.h"
#include "ge_stan_native.h"

#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StageStanHarness {
    uint8_t *blob;
    void *storage;
    GeStanCollisionSurface surface;
    GeStanNativeMap native;
} StageStanHarness;

typedef struct TintedGlassHarness {
    const GeOriginalStagePropConstructionRequest *request;
    size_t portal_calls;
    size_t construct_calls;
    size_t place_calls;
    int32_t next_portal;
} TintedGlassHarness;

typedef struct MiscHarness {
    const GeOriginalStagePropConstructionRequest *request;
    PropRecord prop;
    int model_token;
    int ai_token;
    unsigned sequence;
    size_t constructions;
    size_t switch_calls;
    size_t projectile_calls;
    size_t floor_calls;
} MiscHarness;

typedef struct SafeLinkHarness {
    void **definitions;
    size_t count;
    size_t registrations;
    GeOriginalStageSafeRuntime runtime;
} SafeLinkHarness;

typedef struct DefaultMaterializerHarness {
    size_t root_callbacks;
    size_t embedded_callbacks;
    size_t unplaced_embedded_callbacks;
} DefaultMaterializerHarness;

static void bind_stage_stan(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeOriginalStageSetupRuntime *setup, StageStanHarness *stan)
{
    const GeAssetPackEntry *entry = ge_asset_pack_find(
        pack, descriptor->collision_path);
    size_t native_size;
    size_t index;
    memset(stan, 0, sizeof(*stan));
    assert(entry != NULL && entry->data_size > 0U
           && entry->data_size <= SIZE_MAX);
    stan->blob = malloc((size_t)entry->data_size);
    assert(stan->blob != NULL
           && ge_asset_pack_read(pack, descriptor->collision_path,
               stan->blob, (size_t)entry->data_size, NULL)
                == GE_ASSET_PACK_OK
           && ge_stan_collision_open(
               stan->blob, (size_t)entry->data_size, &stan->surface)
                == GE_STAN_COLLISION_OK
           && ge_stan_native_required_size(&stan->surface, &native_size)
                == GE_STAN_COLLISION_OK);
    stan->storage = malloc(native_size);
    assert(stan->storage != NULL
           && ge_stan_native_materialize(&stan->surface,
               descriptor->level_scale, stan->storage, native_size,
               &stan->native) == GE_STAN_COLLISION_OK);
    setup->pad_stan_count = setup->bound_pad_stan_count = 0U;
    for (index = 0U; index < setup->pad_count; ++index) {
        PadRecord *pad = &((PadRecord *)setup->pads_storage)[index];
        pad->stan = (StandTile *)ge_original_stan_match_tile_name(
            &stan->native, pad->plink);
        setup->pad_stan_count += pad->stan != NULL;
    }
    for (index = 0U; index < setup->boundpad_count; ++index) {
        BoundPadRecord *pad =
            &((BoundPadRecord *)setup->boundpads_storage)[index];
        pad->stan = (StandTile *)ge_original_stan_match_tile_name(
            &stan->native, pad->plink);
        setup->bound_pad_stan_count += pad->stan != NULL;
    }
}

static void close_stage_stan(StageStanHarness *stan)
{
    free(stan->storage);
    free(stan->blob);
    memset(stan, 0, sizeof(*stan));
}

static int model_available(void *context, int32_t model_id)
{
    (void)context;
    return model_id >= 0 && model_id < 341;
}

static int classify_special(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    (void)context;
    (void)request;
    return 1;
}

static int materialize_default(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    DefaultMaterializerHarness *harness = context;
    const uint32_t owner_flags = request != NULL && request->record != NULL
        ? request->record->words[2]
            & (PROPFLAG_ASSIGNEDTOCHR | PROPFLAG_INSIDEANOTHEROBJ)
        : 0U;
    assert(harness != NULL && request != NULL && request->record != NULL
           && (request->record->type == PROPDEF_PROP
               || request->record->type == PROPDEF_GLASS));
    if (owner_flags == PROPFLAG_INSIDEANOTHEROBJ) {
        ++harness->embedded_callbacks;
        if (!request->placement_resolved || request->placement.has_stan == 0U)
            ++harness->unplaced_embedded_callbacks;
    } else {
        assert(owner_flags == 0U && request->placement_resolved
               && request->placement.has_stan != 0U);
        ++harness->root_callbacks;
    }
    return 1;
}

static int32_t find_portal(
    void *context, const float point_a[3], const float point_b[3])
{
    TintedGlassHarness *harness = context;
    float length_squared = 0.0f;
    size_t axis;
    for (axis = 0U; axis < 3U; ++axis) {
        const float difference = point_b[axis] - point_a[axis];
        assert(isfinite(point_a[axis]) && isfinite(point_b[axis]));
        length_squared += difference * difference;
    }
    assert(length_squared > 0.0f);
    ++harness->portal_calls;
    return harness->next_portal++;
}

static int construct_standard(
    void *context, void *definition, int32_t command_index)
{
    TintedGlassHarness *harness = context;
    TintedGlassRecord *glass = definition;
    assert(harness->request != NULL
           && command_index == (int32_t)harness->request->command_index
           && glass->type == PROPDEF_TINTED_GLASS
           && glass->obj == harness->request->model_id
           && glass->pad == harness->request->pad_id);
    ++harness->construct_calls;
    return 1;
}

static int place_standard(void *context, void *definition)
{
    TintedGlassHarness *harness = context;
    assert(definition != NULL);
    ++harness->place_calls;
    return 1;
}

static int misc_construct_standard(
    void *context, void *definition, int32_t command_index)
{
    MiscHarness *harness = context;
    ObjectRecord *object = definition;
    assert(harness->request != NULL
           && command_index == (int32_t)harness->request->command_index
           && object->obj == harness->request->model_id
           && object->pad == harness->request->pad_id
           && harness->sequence == 0U);
    memset(&harness->prop, 0, sizeof(harness->prop));
    harness->prop.obj = object;
    harness->prop.stan = (StandTile *)harness->request->placement.stan;
    memcpy(harness->prop.pos.f, harness->request->placement.position,
           sizeof(harness->prop.pos.f));
    object->prop = &harness->prop;
    object->model = (Model *)(void *)&harness->model_token;
    memset(&object->mtx, 0, sizeof(object->mtx));
    object->mtx.m[0][0] = object->mtx.m[1][1]
        = object->mtx.m[2][2] = object->mtx.m[3][3] = 1.0f;
    harness->sequence = 1U;
    ++harness->constructions;
    return 1;
}

static int misc_place_standard(void *context, void *definition)
{
    MiscHarness *harness = context;
    assert(definition != NULL && harness->sequence == 1U);
    harness->sequence = 2U;
    return 1;
}

static int misc_update_room(void *context, void *definition)
{
    MiscHarness *harness = context;
    assert(definition != NULL && harness->sequence == 2U);
    harness->sequence = 3U;
    return 1;
}

static int misc_activate(void *context, void *prop)
{
    MiscHarness *harness = context;
    assert(prop == &harness->prop && harness->sequence == 3U);
    harness->sequence = 4U;
    return 1;
}

static int misc_enable(void *context, void *prop)
{
    MiscHarness *harness = context;
    assert(prop == &harness->prop && harness->sequence == 4U);
    harness->sequence = 5U;
    return 1;
}

static int misc_resolve_ai(void *context, int32_t list_id, void **resolved)
{
    MiscHarness *harness = context;
    assert(list_id >= 0 && resolved != NULL && harness->sequence == 5U);
    *resolved = &harness->ai_token;
    return 1;
}

static int misc_set_switch(
    void *context, void *model, uint32_t switch_index, int enabled)
{
    MiscHarness *harness = context;
    assert(model == &harness->model_token && switch_index == 5U
           && (enabled == 0 || enabled == 1) && harness->sequence == 5U);
    ++harness->switch_calls;
    return 1;
}

static int misc_load_projectiles(void *context)
{
    MiscHarness *harness = context;
    assert(harness->sequence == 0U);
    ++harness->projectile_calls;
    return 1;
}

static int misc_floor_y(
    void *context, void *stan, float x, float z, float *floor_y)
{
    MiscHarness *harness = context;
    assert(stan == harness->request->placement.stan
           && isfinite(x) && isfinite(z) && floor_y != NULL
           && harness->sequence == 5U);
    *floor_y = 17.0f;
    ++harness->floor_calls;
    return 1;
}

static void *safe_find_definition(void *context, size_t command_index)
{
    SafeLinkHarness *harness = context;
    return command_index < harness->count
        ? harness->definitions[command_index] : NULL;
}

static int safe_register_relation(void *context, void *relation)
{
    SafeLinkHarness *harness = context;
    assert(relation != NULL);
    if (!ge_original_stage_safe_runtime_register_relation(
            &harness->runtime, relation)) return 0;
    ++harness->registrations;
    return 1;
}

static uint32_t float_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    return bits;
}

static void assert_misc_native(
    const GeOriginalStagePropRecord *record, const void *definition)
{
    const ObjectRecord *object = definition;
    assert(object->type == record->type && object->obj == record->model_id
           && object->pad == record->pad_id && object->flags == record->words[2]
           && object->flags2 == record->words[3]);
    if (record->type == PROPDEF_VEHICHLE) {
        const VehichleRecord *vehicle = definition;
        assert((uintptr_t)vehicle->ailist == record->words[32]
               && vehicle->aioffset == (uint16_t)(record->words[33] >> 16U)
               && vehicle->aireturnlist == (int16_t)record->words[33]
               && float_bits(vehicle->speed) == record->words[34]
               && float_bits(vehicle->roty) == record->words[40]
               && (uintptr_t)vehicle->path == record->words[41]
               && vehicle->nextstep == (int32_t)record->words[42]
               && (uintptr_t)vehicle->Sound == record->words[43]);
    } else if (record->type == PROPDEF_AIRCRAFT) {
        const AircraftRecord *aircraft = definition;
        assert((uintptr_t)aircraft->ailist == record->words[32]
               && aircraft->aioffset == (uint16_t)(record->words[33] >> 16U)
               && aircraft->aireturnlist == (int16_t)record->words[33]
               && float_bits(aircraft->rotoryrot) == record->words[34]
               && float_bits(aircraft->yrot) == record->words[41]
               && aircraft->nextstep == (int32_t)record->words[42]
               && (uintptr_t)aircraft->path == record->words[43]
               && (uintptr_t)aircraft->Sound == record->words[44]);
    } else if (record->type == PROPDEF_TANK) {
        const TankRecord *tank = definition;
        size_t word;
        assert((uintptr_t)tank->collision == record->words[32]
               && tank->unkA4 == (int32_t)record->words[41]
               && tank->unkD8 == (int32_t)record->words[54]
               && float_bits(tank->tank_orientation_angle)
                    == record->words[55]);
        for (word = 0U; word < 8U; ++word)
            assert(float_bits(tank->rect.f[word]) == record->words[33U+word]);
    } else if (record->type == PROPDEF_SAFE) {
        const SafeRecord *safe = definition;
        assert(float_bits(safe->normal.f[0]) == 0U
               && float_bits(safe->normal.f[1]) == 0U
               && float_bits(safe->normal.f[2]) == 0U);
    }
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalStageSpecialAudit audit;
    size_t stage_index;
    size_t type_index;
    size_t constructed_tinted = 0U;
    size_t portal_tinted = 0U;
    size_t tinted_stages = 0U;
    size_t monitor_standard = 0U;
    size_t monitor_embedded = 0U;
    size_t monitor_inside = 0U;
    size_t misc_constructed = 0U;
    size_t misc_owned = 0U;
    size_t safe_links = 0U;
    size_t special_materializer_ready = 0U;
    size_t special_second_pass = 0U;
    size_t special_unsupported = 0U;
    size_t ordinary_unsupported = 0U;
    size_t ordinary_negative = 0U;
    size_t ordinary_assigned = 0U;
    size_t ordinary_inside = 0U;
    size_t ordinary_unsupported_by_stage[GE_STAGE_COUNT] = {0};
    size_t embedded_callbacks_by_stage[GE_STAGE_COUNT] = {0};
    size_t unplaced_embedded_callbacks = 0U;
    size_t monitor_images[52] = {0};
    size_t multi_monitor_images[52] = {0};
    static const size_t expected_totals[GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT] = {
        7U, 15U, 48U, 222U, 55U, 4U, 41U, 12U, 50U,
        19U, 9U, 6U, 0U, 5U, 2U, 184U, 6U,
    };
    assert(argc == 2);
    assert(strcmp(ge_original_stage_misc_status_name(
        GE_ORIGINAL_STAGE_MISC_OK), "ok") == 0);
    assert(ge_original_stage_misc_runtime_dependencies(PROPDEF_ALARM)
                == (GE_ORIGINAL_STAGE_MISC_DEP_DEFAULT_OBJECT
                    | GE_ORIGINAL_STAGE_MISC_DEP_PROP_PUBLICATION
                    | GE_ORIGINAL_STAGE_MISC_DEP_ALARM_INTERACTION)
           && ge_original_stage_misc_runtime_dependencies(PROPDEF_SAFE_ITEM)
                == GE_ORIGINAL_STAGE_MISC_DEP_SAFE_RELATION
           && ge_original_stage_misc_runtime_dependencies(PROPDEF_END) == 0U);
    memset(&audit, 0, sizeof(audit));
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup;
        StageStanHarness stan;
        TintedGlassHarness harness = {0};
        GeOriginalStageTintedGlassProviders tinted_providers = {
            .context = &harness,
            .find_portal = find_portal,
            .construct_standard = construct_standard,
            .place_standard = place_standard,
        };
        GeOriginalStagePropMaterializerProviders classify = {
            .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_TINTED_GLASS,
            .model_available = model_available,
            .construct_special_object = classify_special,
        };
        void **safe_definitions;
        SafeObjectRecord *safe_relations;
        SafeLinkHarness safe_harness;
        DefaultMaterializerHarness default_harness = {0};
        GeOriginalStagePropMaterializerReport default_report;
        GeOriginalStagePropMaterializerProviders default_materializer = {
            .context = &default_harness,
            .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT,
            .model_available = model_available,
            .construct_default_object = materialize_default,
        };
        size_t record_index;
        size_t stage_tinted = 0U;
        assert(descriptor != NULL);
        assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
               == GE_ORIGINAL_STAGE_SETUP_OK);
        safe_definitions = calloc(
            setup.prop_record_count, sizeof(*safe_definitions));
        safe_relations = calloc(
            setup.prop_record_count, sizeof(*safe_relations));
        assert(safe_definitions != NULL && safe_relations != NULL);
        safe_harness.definitions = safe_definitions;
        safe_harness.count = setup.prop_record_count;
        safe_harness.registrations = 0U;
        ge_original_stage_safe_runtime_bind(&safe_harness.runtime);
        bind_stage_stan(&pack, descriptor, &setup, &stan);
        assert(ge_original_stage_prop_materialize_ready(
            &setup, &default_materializer, &default_report));
        assert(default_report.constructed == default_report.ready
               && default_report.failed == 0U);
        embedded_callbacks_by_stage[stage_index] =
            default_harness.embedded_callbacks;
        unplaced_embedded_callbacks +=
            default_harness.unplaced_embedded_callbacks;
        assert(ge_original_stage_special_audit_add(
            &audit, descriptor->stage, &setup));
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[record_index];
            GeOriginalStagePropMaterializerProviders full_special = {
                .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_TINTED_GLASS
                    | GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT
                    | GE_ORIGINAL_STAGE_PROP_CAP_MONITOR
                    | GE_ORIGINAL_STAGE_PROP_CAP_CCTV
                    | GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN
                    | GE_ORIGINAL_STAGE_PROP_CAP_SUPPLY
                    | GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT
                    | GE_ORIGINAL_STAGE_PROP_CAP_SAFE
                    | GE_ORIGINAL_STAGE_PROP_CAP_ALARM
                    | GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING,
                .model_available = model_available,
                .construct_default_object = classify_special,
                .construct_special_object = classify_special,
            };
            GeOriginalStagePropConstructionRequest request;
            TintedGlassRecord *glass;
            size_t definition_size;
            size_t portals_before;
            int32_t raw_opacity;
            uint32_t final_opacity_bits;
            if ((record->type == PROPDEF_PROP
                        || record->type == PROPDEF_GLASS)
                    && ge_original_stage_prop_classify(
                        record, &full_special).blocker
                        == GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH) {
                ++ordinary_unsupported;
                ++ordinary_unsupported_by_stage[stage_index];
                if (record->pad_id < 0) ++ordinary_negative;
                if ((record->words[2] & PROPFLAG_ASSIGNEDTOCHR) != 0U)
                    ++ordinary_assigned;
                if ((record->words[2] & PROPFLAG_INSIDEANOTHEROBJ) != 0U)
                    ++ordinary_inside;
                if (getenv("GE_STAGE_SPECIAL_VERBOSE") != NULL)
                    printf("ordinary frontier %s[%zu] type=%u model=%d "
                           "pad=%d flags=%08x flags2=%08x\n",
                           descriptor->key, record_index,
                           (unsigned)record->type, record->model_id,
                           record->pad_id, record->words[2],
                           record->words[3]);
            }
            if (ge_original_stage_special_type_index(record->type) >= 0) {
                const GeOriginalStagePropClassification classification =
                    ge_original_stage_prop_classify(record, &full_special);
                if (record->type == PROPDEF_SAFE_ITEM) {
                    assert(classification.service
                               == GE_ORIGINAL_STAGE_PROP_SERVICE_CONTROL
                           && classification.blocker
                               == GE_ORIGINAL_STAGE_PROP_CONTROL_ONLY);
                    ++special_second_pass;
                } else if (classification.blocker
                            == GE_ORIGINAL_STAGE_PROP_READY) {
                    ++special_materializer_ready;
                } else {
                    ++special_unsupported;
                }
            }
            if (record->type == PROPDEF_MONITOR) {
                const int32_t image = (int32_t)record->words[63];
                assert(record->word_count == 64U && image >= 0 && image < 52);
                ++monitor_images[image];
                if (record->pad_id < 0
                        && (record->words[2]
                            & PROPFLAG_INSIDEANOTHEROBJ) == 0U)
                    ++monitor_embedded;
                else if ((record->words[2]
                            & PROPFLAG_INSIDEANOTHEROBJ) != 0U)
                    ++monitor_inside;
                else
                    ++monitor_standard;
            } else if (record->type == PROPDEF_MULTI_MONITOR) {
                const uint32_t packed = record->words[148];
                size_t slot;
                assert(record->word_count == 149U);
                for (slot = 0U; slot < 4U; ++slot) {
                    const uint8_t image = (uint8_t)(packed >> (24U-slot*8U));
                    assert(image < 52U);
                    ++multi_monitor_images[image];
                }
            }
            if (record->type != PROPDEF_TINTED_GLASS) continue;
            assert(ge_original_stage_prop_classify(record, &classify).blocker
                   == GE_ORIGINAL_STAGE_PROP_READY
                   && ge_original_stage_prop_construction_request(
                        &setup, record_index, &request)
                   && request.placement_resolved
                   && request.placement.stan != NULL);
            definition_size =
                ge_original_stage_prop_native_definition_size(&request);
            assert(definition_size == sizeof(*glass));
            glass = malloc(definition_size);
            assert(glass != NULL);
            harness.request = &request;
            harness.next_portal = 1000 + (int32_t)record_index;
            portals_before = harness.portal_calls;
            assert(ge_original_stage_tinted_glass_construct(
                &request, glass, definition_size, &tinted_providers)
                == GE_ORIGINAL_STAGE_TINTED_GLASS_OK);
            assert(glass->TintDist == (int32_t)record->words[32]
                   && glass->CullDist == (int32_t)record->words[33]
                   && glass->calculatedopacity == (int32_t)record->words[34]);
            memcpy(&raw_opacity, &record->words[36], sizeof(raw_opacity));
            if ((glass->flags & PROPFLAG_GLASS_HASPORTAL) != 0U
                    && glass->pad >= 10000) {
                assert(harness.portal_calls == portals_before + 1U
                       && glass->portalnum
                            == 1000 + (int32_t)record_index
                       && glass->unk90
                            == (float)raw_opacity / 65535.0f);
                ++portal_tinted;
            } else {
                assert(harness.portal_calls == portals_before);
                memcpy(&final_opacity_bits, &glass->unk90,
                       sizeof(final_opacity_bits));
                assert(final_opacity_bits == record->words[36]);
            }
            free(glass);
            ++stage_tinted;
            ++constructed_tinted;
        }
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[record_index];
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStageMiscProviders providers;
            GeOriginalStageMiscInstance instance;
            GeOriginalStagePropMaterializerProviders materializer = {
                .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_MISC_OBJECT,
                .model_available = model_available,
                .construct_special_object = classify_special,
            };
            MiscHarness misc = {0};
            size_t definition_size;
            void *definition;
            if (record->type != PROPDEF_ALARM && record->type != PROPDEF_RACK
                    && record->type != PROPDEF_GAS_RELEASING
                    && record->type != PROPDEF_VEHICHLE
                    && record->type != PROPDEF_AIRCRAFT
                    && record->type != PROPDEF_SAFE
                    && record->type != PROPDEF_TANK) continue;
            assert(ge_original_stage_prop_construction_request(
                &setup, record_index, &request));
            definition_size =
                ge_original_stage_prop_native_definition_size(&request);
            assert(definition_size != 0U);
            definition = malloc(definition_size);
            assert(definition != NULL
                   && ge_original_stage_prop_native_definition_init(
                        &request, definition, definition_size));
            assert_misc_native(record, definition);
            free(definition);
            if (record->pad_id < 0
                    || (record->words[2]
                        & (PROPFLAG_INSIDEANOTHEROBJ
                           | PROPFLAG_ASSIGNEDTOCHR)) != 0U) {
                assert(ge_original_stage_prop_classify(
                    record, &materializer).blocker
                       == GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH);
                ++misc_owned;
                continue;
            }
            assert(ge_original_stage_prop_classify(
                record, &materializer).blocker
                   == GE_ORIGINAL_STAGE_PROP_READY);
            if (record->type == PROPDEF_GAS_RELEASING) {
                GeOriginalStagePropMaterializerProviders gas_only = {
                    .capabilities =
                        GE_ORIGINAL_STAGE_PROP_CAP_GAS_RELEASING,
                    .model_available = model_available,
                    .construct_special_object = classify_special,
                };
                assert(ge_original_stage_prop_classify(
                    record, &gas_only).blocker
                       == GE_ORIGINAL_STAGE_PROP_READY);
            }
            if (!request.placement_resolved || request.placement.stan == NULL)
                continue;
            definition = malloc(definition_size);
            assert(definition != NULL);
            misc.request = &request;
            memset(&providers, 0, sizeof(providers));
            providers.context = &misc;
            providers.construct_standard = misc_construct_standard;
            providers.place_standard = misc_place_standard;
            providers.update_room_position = misc_update_room;
            providers.activate_prop = misc_activate;
            providers.enable_prop = misc_enable;
            providers.resolve_ai_list = misc_resolve_ai;
            providers.set_model_switch = misc_set_switch;
            providers.load_tank_projectiles = misc_load_projectiles;
            providers.get_floor_y = misc_floor_y;
            assert(ge_original_stage_misc_construct_exact(
                &request, definition, definition_size, &providers, &instance)
                   == GE_ORIGINAL_STAGE_MISC_OK
                   && instance.constructed && instance.activated
                   && instance.runtime_dependencies
                        == ge_original_stage_misc_runtime_dependencies(
                            record->type)
                   && misc.sequence == 5U);
            if (record->type == PROPDEF_VEHICHLE) {
                VehichleRecord *vehicle = definition;
                assert(vehicle->ailist == (AIRecord *)&misc.ai_token
                       && vehicle->speed == 0.0f
                       && vehicle->speedtime60 == -1.0f
                       && vehicle->aireturnlist == -1
                       && misc.switch_calls == 1U);
            } else if (record->type == PROPDEF_AIRCRAFT) {
                AircraftRecord *aircraft = definition;
                assert(aircraft->ailist == (AIRecord *)&misc.ai_token
                       && aircraft->rotaryspeed == 0.0f
                       && aircraft->speedtime60 == -1.0f
                       && aircraft->rotaryspeedtime == -1.0f
                       && aircraft->aireturnlist == -1);
            } else if (record->type == PROPDEF_TANK) {
                TankRecord *tank = definition;
                assert(misc.projectile_calls == 1U
                       && misc.floor_calls == 1U
                       && tank->stan_y == 17.0f
                       && tank->unkD0 == 17.0f / 0.17000002f
                       && tank->turret_vertical_angle == 0.0f
                       && tank->turret_orientation_angle == 0.0f);
            }
            free(definition);
            ++misc_constructed;
        }
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[record_index];
            if (record->type == PROPDEF_SAFE) {
                SafeRecord *safe = calloc(1U, sizeof(*safe));
                assert(safe != NULL);
                safe->type = PROPDEF_SAFE;
                safe->prop = calloc(1U, sizeof(*safe->prop));
                assert(safe->prop != NULL);
                safe_definitions[record_index] = safe;
            } else if (record->type == PROPDEF_DOOR) {
                DoorRecord *door = calloc(1U, sizeof(*door));
                assert(door != NULL);
                door->type = PROPDEF_DOOR;
                door->prop = calloc(1U, sizeof(*door->prop));
                assert(door->prop != NULL);
                safe_definitions[record_index] = door;
            } else if (record->type == PROPDEF_PROP
                    || record->type == PROPDEF_KEY
                    || record->type == PROPDEF_COLLECTABLE) {
                ObjectRecord *item = calloc(1U, sizeof(*item));
                assert(item != NULL);
                item->type = record->type;
                item->prop = calloc(1U, sizeof(*item->prop));
                assert(item->prop != NULL);
                safe_definitions[record_index] = item;
            }
        }
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[record_index];
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStageSafeItemProviders providers = {
                .context = &safe_harness,
                .find_definition = safe_find_definition,
                .register_relation = safe_register_relation,
            };
            SafeObjectRecord *relation = &safe_relations[record_index];
            if (record->type != PROPDEF_SAFE_ITEM) continue;
            assert(ge_original_stage_prop_construction_request(
                &setup, record_index, &request)
                   && ge_original_stage_safe_item_link_exact(
                    &request, relation, sizeof(*relation), &providers)
                        == GE_ORIGINAL_STAGE_MISC_OK
                   && relation->item != NULL && relation->safe != NULL
                   && relation->door != NULL
                   && (relation->item->flags2 & PROPFLAG2_LINKEDTOSAFE) != 0U
                   && (relation->door->flags2 & PROPFLAG2_LINKEDTOSAFE) != 0U);
            ++safe_links;
        }
        /* Test again only after the whole authored second pass so stages with
         * multiple links (Bunker 2 has three) traverse the exact reverse-order
         * canonical list rather than only its newest head. */
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            SafeObjectRecord *relation = &safe_relations[record_index];
            if (setup.prop_records[record_index].type != PROPDEF_SAFE_ITEM)
                continue;
            relation->door->openPosition = 0.5f;
            assert(!ge_original_stage_safe_runtime_can_pickup(
                &safe_harness.runtime, relation->item));
            relation->door->openPosition = 0.50001f;
            assert(ge_original_stage_safe_runtime_can_pickup(
                &safe_harness.runtime, relation->item));
        }
        assert(safe_harness.registrations
                == ge_original_stage_setup_prop_type_count(
                    &setup, PROPDEF_SAFE_ITEM)
               && safe_harness.runtime.relation_count
                    == safe_harness.registrations
               && safe_harness.runtime.pickup_tests
                    == safe_harness.registrations * 2U
               && safe_harness.runtime.blocked_pickups
                    == safe_harness.registrations);
        ge_original_stage_safe_runtime_close(&safe_harness.runtime);
        for (record_index = 0U; record_index < setup.prop_record_count;
                ++record_index) {
            ObjectRecord *object = safe_definitions[record_index];
            if (object != NULL) free(object->prop);
            free(object);
        }
        free(safe_definitions);
        free(safe_relations);
        assert(harness.construct_calls == stage_tinted
               && harness.place_calls == stage_tinted);
        if (stage_tinted != 0U) ++tinted_stages;
        close_stage_stan(&stan);
        ge_original_stage_setup_close(&setup);
    }
    for (type_index = 0U;
            type_index < GE_ORIGINAL_STAGE_SPECIAL_TYPE_COUNT;
            ++type_index) {
        const GeOriginalStageSpecialTypeAudit *entry =
            &audit.types[type_index];
        assert(entry->total == expected_totals[type_index]);
        printf("%-16s %3zu record / %2zu stage",
               ge_original_stage_special_type_name(entry->type),
               entry->total, entry->stage_count);
        for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
            if (entry->by_stage[stage_index] != 0U)
                printf(" %s=%zu",
                       ge_stage_asset_descriptor((GeStageId)stage_index)->key,
                       entry->by_stage[stage_index]);
        }
        putchar('\n');
    }
    assert(constructed_tinted == 184U && tinted_stages == 7U
           && portal_tinted > 0U);
    printf("tinted-glass exact constructor: %zu record / %zu stage / "
           "%zu portal ray\n", constructed_tinted, tinted_stages,
           portal_tinted);
    printf("monitor setup branches: %zu standard / %zu embedded / %zu inside; "
           "image IDs", monitor_standard, monitor_embedded, monitor_inside);
    for (type_index = 0U; type_index < 52U; ++type_index)
        if (monitor_images[type_index] != 0U
                || multi_monitor_images[type_index] != 0U)
            printf(" %zu=%zu+%zu", type_index, monitor_images[type_index],
                   multi_monitor_images[type_index]);
    putchar('\n');
    assert(special_materializer_ready == 679U
           && special_second_pass == 6U && special_unsupported == 0U);
    printf("special authored census: %zu exact records; %zu constructor-ready; "
           "%zu canonical second-pass; %zu unsupported\n",
           audit.total, special_materializer_ready, special_second_pass,
           special_unsupported);
    assert(embedded_callbacks_by_stage[GE_STAGE_SURFACE] == 12U
           && embedded_callbacks_by_stage[GE_STAGE_SURFACE2] == 1U
           && embedded_callbacks_by_stage[GE_STAGE_CAVERNS] == 3U
           && unplaced_embedded_callbacks == 13U);
    printf("ordinary embedded materializer callbacks: surface1=%zu "
           "surface2=%zu caverns=%zu; %zu bypassed root placement gate\n",
           embedded_callbacks_by_stage[GE_STAGE_SURFACE],
           embedded_callbacks_by_stage[GE_STAGE_SURFACE2],
           embedded_callbacks_by_stage[GE_STAGE_CAVERNS],
           unplaced_embedded_callbacks);
    printf("ordinary owned/negative frontier: %zu records (%zu negative, "
           "%zu assigned, %zu inside)", ordinary_unsupported,
           ordinary_negative, ordinary_assigned, ordinary_inside);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index)
        if (ordinary_unsupported_by_stage[stage_index] != 0U)
            printf(" %s=%zu",
                   ge_stage_asset_descriptor((GeStageId)stage_index)->key,
                   ordinary_unsupported_by_stage[stage_index]);
    putchar('\n');
    assert(safe_links == 6U && misc_constructed > 0U);
    printf("misc constructors: %zu positive-pad live / %zu owner-blocked; "
           "%zu safe relation\n", misc_constructed, misc_owned, safe_links);
    ge_asset_pack_close(&pack);
    return 0;
}
