#include "ge_asset_pack.h"
#include "ge_original_stage_autogun_lifecycle.h"
#include "ge_original_stage_security.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"

#include <ultra64.h>
#ifdef MAXFLOAT
#undef MAXFLOAT
#endif
#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct SecurityHarness {
    const GeOriginalStagePropConstructionRequest *request;
    PropRecord prop;
    Model model;
    ModelFileHeader header;
    ModelNode relations[8];
    ModelNode *switches[8];
    coord3d relation_points[8];
    void *beam;
    size_t construct_calls;
    size_t place_calls;
    size_t allocation_calls;
    unsigned sequence;
    int omit_relation;
} SecurityHarness;

static GeOriginalStageSecurityStatus construct_security_instance(
    const GeOriginalStagePropConstructionRequest *request,
    void *definition,size_t definition_size,
    const GeOriginalStageSecurityProviders *providers,
    GeOriginalStageSecurityInstance *instance)
{
    return request!=NULL&&request->record!=NULL
            &&request->record->type==PROPDEF_AUTOGUN
        ?ge_original_stage_autogun_lifecycle_construct(
            request,definition,definition_size,providers,instance)
        :ge_original_stage_security_construct(
            request,definition,definition_size,providers,instance);
}

static int model_available(void *opaque, int32_t model_id)
{
    (void)opaque;
    return model_id >= 0;
}

static int construct_special_object(
    void *opaque, const GeOriginalStagePropConstructionRequest *request)
{
    (void)opaque;
    return request != NULL;
}

static int construct_standard(void *opaque, void *definition,
                              int32_t command_index)
{
    SecurityHarness *harness = opaque;
    ObjectRecord *object = definition;
    assert(harness != NULL && harness->request != NULL);
    assert(command_index == (int32_t)harness->request->command_index);
    memset(&harness->prop, 0, sizeof(harness->prop));
    memset(&harness->model, 0, sizeof(harness->model));
    memset(&harness->header, 0, sizeof(harness->header));
    memset(harness->relations, 0, sizeof(harness->relations));
    harness->model.obj = &harness->header;
    harness->model.scale = 1.25f;
    for (size_t index = 0U; index < 8U; ++index) {
        harness->relation_points[index].x = 1.0f + (float)index;
        harness->relation_points[index].y = 2.0f + (float)index;
        harness->relation_points[index].z = 3.0f + (float)index;
        harness->relations[index].Data =
            (union ModelRoData *)&harness->relation_points[index];
        harness->switches[index] = &harness->relations[index];
    }
    harness->header.Switches = harness->switches;
    harness->header.numSwitches = 8;
    harness->header.numMatrices = 8;
    if (harness->request->record->type == PROPDEF_CCTV
            && harness->omit_relation) harness->switches[0] = NULL;
    object->model = &harness->model;
    object->prop = &harness->prop;
    object->mtx.m[0][0] = object->mtx.m[1][1] =
        object->mtx.m[2][2] = object->mtx.m[3][3] = 1.0f;
    harness->prop.obj = object;
    harness->prop.type = PROP_TYPE_OBJ;
    memcpy(harness->prop.pos.f, harness->request->placement.position,
        sizeof(harness->prop.pos.f));
    harness->prop.stan = harness->request->placement.stan;
    ++harness->construct_calls;
    assert(harness->sequence == 0U);
    harness->sequence = 1U;
    return 1;
}

static int place_standard(void *opaque, void *definition)
{
    SecurityHarness *harness = opaque;
    ObjectRecord *object = definition;
    assert(object != NULL && object->prop == &harness->prop
        && object->model == &harness->model && harness->sequence == 1U);
    ++harness->place_calls;
    harness->sequence = 2U;
    return 1;
}

static int update_room_position(void *opaque, void *definition)
{
    SecurityHarness *harness = opaque;
    assert(definition != NULL && harness->sequence == 2U);
    harness->sequence = 3U;
    return 1;
}

static int activate_prop(void *opaque, void *prop)
{
    SecurityHarness *harness = opaque;
    assert(prop == &harness->prop && harness->sequence == 3U);
    harness->sequence = 4U;
    return 1;
}

static int enable_prop(void *opaque, void *prop)
{
    SecurityHarness *harness = opaque;
    assert(prop == &harness->prop && harness->sequence == 4U);
    harness->sequence = 5U;
    return 1;
}

static void *allocate_stage(void *opaque, size_t size_bytes)
{
    SecurityHarness *harness = opaque;
    assert(size_bytes == 0x30U && harness->beam == NULL);
    harness->beam = malloc(size_bytes);
    ++harness->allocation_calls;
    return harness->beam;
}

static float fixed_angle(uint32_t word)
{
    return ((float)(int32_t)word * M_TAU_F) / 65536.0f;
}

static void validate_instance(
    const GeOriginalStagePropConstructionRequest *request,
    const GeOriginalStageSecurityInstance *instance,
    const void *definition, const SecurityHarness *harness)
{
    assert(instance->request == request && instance->definition == definition);
    assert(instance->type == request->record->type);
    assert(instance->constructed && instance->runtime_ready);
    assert(instance->missing_runtime_capabilities == 0U);
    assert(instance->prop == &harness->prop
        && instance->model == &harness->model);
    {
        GeOriginalStageSecurityModelAudit model_audit;
        assert(ge_original_stage_security_model_audit(
            definition, &model_audit)
               && model_audit.render_relations_ready
               && model_audit.num_switches == 8
               && model_audit.num_matrices == 8);
    }
    if (request->record->type == PROPDEF_CCTV) {
        const CCTVRecord *cctv = definition;
        assert(instance->required_runtime_capabilities
            == GE_ORIGINAL_STAGE_SECURITY_CCTV_REQUIRED);
        assert(cctv->cctv_lookpad == (int32_t)request->record->words[32]);
        if (cctv->cctv_lookpad >= 0) {
            assert(cctv->convert_to_f32 == 1);
            assert(fabsf(cctv->unkCC
                    - fixed_angle(request->record->words[51])) < 0.00001f);
            assert(fabsf(cctv->unkD0
                    - fixed_angle(request->record->words[52])) < 0.00001f);
            assert(fabsf(cctv->unkDC
                    - fixed_angle(request->record->words[55])) < 0.00001f);
            assert(cctv->unkE8
                == (float)(int32_t)request->record->words[58]);
            assert(cctv->unkD4 == 0 && cctv->unkD8 == 0.0f
                && cctv->unkC8 == cctv->unkCC && cctv->timer == 0
                && fabsf(cctv->unkC4 - atan2f(1.0f, 3.0f)) < 0.00001f);
            for (size_t row = 0U; row < 4U; ++row)
                for (size_t column = 0U; column < 4U; ++column)
                    assert(isfinite(cctv->unk84.m[row][column]));
        }
    } else {
        const AutogunRecord *autogun = definition;
        assert(instance->required_runtime_capabilities
            == GE_ORIGINAL_STAGE_SECURITY_AUTOGUN_REQUIRED);
        assert(autogun->padID == (int32_t)request->record->words[32]);
        assert(fabsf(autogun->speed
                - fixed_angle(request->record->words[41])) < 0.00001f);
        assert(fabsf(autogun->aimdist
                - ((float)(int32_t)request->record->words[42]
                    * 100.0f) / 65536.0f) < 0.00001f);
        assert(fabsf(autogun->unk88
                - fixed_angle(request->record->words[34])) < 0.00001f);
        assert(fabsf(autogun->unk8C
                - fixed_angle(request->record->words[35])) < 0.00001f);
        assert(autogun->unkAC == 0 && autogun->unkB8 == -1
            && autogun->unkBC == -1 && autogun->unkC0 == -1
            && autogun->unkC4 == NULL && autogun->unkC8 == NULL
            && autogun->unk90 == 0.0f && autogun->unk94 == 0.0f
            && autogun->unk9C == 0.0f && autogun->unkA0 == 0.0f
            && autogun->unkB0 == 0.0f && autogun->unkB4 == 0.0f
            && autogun->is_active == 0 && autogun->unkD4 == 0.0f);
        assert(autogun->beam == harness->beam
            && instance->beam == harness->beam
            && *(const int8_t *)autogun->beam == -1);
        if (autogun->padID >= 0)
            assert(isfinite(autogun->rot_related)
                && isfinite(autogun->unk98));
    }
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    size_t stage_index;
    size_t campaign_cctv = 0U, campaign_autogun = 0U;
    size_t cctv_stages = 0U, autogun_stages = 0U;
    size_t cctv_distinct_tail_pad = 0U;
    int tested_cctv_relation_blocker = 0;
    int tested_autogun_beam_blocker = 0;
    int tested_activation_blocker = 0;
    assert(argc == 2);
    assert(strcmp(ge_original_stage_security_status_name(
        GE_ORIGINAL_STAGE_SECURITY_OK), "ok") == 0);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup = {0};
        size_t command_index, stage_cctv = 0U, stage_autogun = 0U;
        assert(descriptor != NULL);
        assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        for (command_index = 0U;
                command_index < setup.prop_record_count; ++command_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[command_index];
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStageSecurityProviders providers;
            GeOriginalStageSecurityInstance instance;
            SecurityHarness harness = {0};
            void *definition;
            size_t definition_size;
            union {
                CCTVRecord cctv;
                AutogunRecord autogun;
            } blocked_definition, blocked_copy;
            uint32_t required;
            if (record->type != PROPDEF_CCTV
                    && record->type != PROPDEF_AUTOGUN) continue;
            assert(ge_original_stage_prop_construction_request(
                &setup, command_index, &request));
            definition_size =
                ge_original_stage_prop_native_definition_size(&request);
            assert(definition_size == (record->type == PROPDEF_CCTV
                ? sizeof(CCTVRecord) : sizeof(AutogunRecord)));
            required = ge_original_stage_security_required_capabilities(
                record->type);
            {
                GeOriginalStageSecurityDependencyAudit dependency_audit;
                assert(ge_original_stage_security_dependency_audit(
                    record->type, &dependency_audit)
                       && dependency_audit.all_capabilities == required
                       && dependency_audit.tick_capabilities != 0U
                       && dependency_audit.render_capabilities != 0U
                       && dependency_audit.damage_capabilities != 0U);
                if (record->type == PROPDEF_AUTOGUN)
                    assert(dependency_audit.cleanup_capabilities
                        == GE_ORIGINAL_STAGE_SECURITY_SOUND_LIFECYCLE);
            }
            {
                GeOriginalStagePropMaterializerProviders materializer = {0};
                GeOriginalStagePropClassification classification;
                materializer.capabilities = record->type == PROPDEF_CCTV
                    ? GE_ORIGINAL_STAGE_PROP_CAP_CCTV
                    : GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN;
                materializer.model_available = model_available;
                materializer.construct_special_object =
                    construct_special_object;
                classification = ge_original_stage_prop_classify(
                    record, &materializer);
                assert(classification.service
                    == GE_ORIGINAL_STAGE_PROP_SERVICE_SPECIAL_OBJECT);
                assert(classification.blocker
                    == GE_ORIGINAL_STAGE_PROP_READY);
                materializer.capabilities = record->type == PROPDEF_CCTV
                    ? GE_ORIGINAL_STAGE_PROP_CAP_AUTOGUN
                    : GE_ORIGINAL_STAGE_PROP_CAP_CCTV;
                classification = ge_original_stage_prop_classify(
                    record, &materializer);
                assert(classification.blocker
                    == GE_ORIGINAL_STAGE_PROP_UNSUPPORTED_BRANCH);
                materializer.capabilities =
                    GE_ORIGINAL_STAGE_PROP_CAP_SECURITY;
                classification = ge_original_stage_prop_classify(
                    record, &materializer);
                assert(classification.blocker
                    == GE_ORIGINAL_STAGE_PROP_READY);
            }
            harness.request = &request;
            providers.context = &harness;
            providers.runtime_capabilities = required;
            providers.construct_standard = construct_standard;
            providers.place_standard = place_standard;
            providers.update_room_position = update_room_position;
            providers.activate_prop = activate_prop;
            providers.enable_prop = enable_prop;
            providers.allocate_stage = allocate_stage;
            memset(&blocked_definition, 0xa5, sizeof(blocked_definition));
            memcpy(&blocked_copy, &blocked_definition, sizeof(blocked_copy));
            providers.runtime_capabilities = required
                & ~GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK;
            assert(construct_security_instance(&request,
                &blocked_definition, definition_size, &providers, &instance)
                    == GE_ORIGINAL_STAGE_SECURITY_MISSING_RUNTIME_DEPENDENCY);
            assert(instance.missing_runtime_capabilities
                == GE_ORIGINAL_STAGE_SECURITY_ACTIVE_PROP_TICK);
            assert(memcmp(&blocked_definition, &blocked_copy,
                sizeof(blocked_definition)) == 0);
            providers.runtime_capabilities = required;
            if (!tested_activation_blocker) {
                GeOriginalStageSecurityProviders blocked_providers = providers;
                blocked_providers.enable_prop = NULL;
                memset(&blocked_definition, 0xa5, sizeof(blocked_definition));
                memcpy(&blocked_copy, &blocked_definition,
                       sizeof(blocked_copy));
                assert(construct_security_instance(&request,
                    &blocked_definition, definition_size,
                    &blocked_providers, &instance)
                       == GE_ORIGINAL_STAGE_SECURITY_ACTIVATION_DEPENDENCY_UNAVAILABLE
                       && memcmp(&blocked_definition, &blocked_copy,
                            sizeof(blocked_definition)) == 0);
                tested_activation_blocker = 1;
            }
            definition = malloc(definition_size);
            assert(definition != NULL);
            assert(construct_security_instance(&request,
                definition, definition_size, &providers, &instance)
                    == GE_ORIGINAL_STAGE_SECURITY_OK);
            assert(harness.construct_calls == 1U
                && harness.place_calls == 1U && harness.sequence == 5U);
            validate_instance(
                &request, &instance, definition, &harness);
            if (record->type == PROPDEF_CCTV) {
                cctv_distinct_tail_pad +=
                    request.pad_id != (int32_t)record->words[32];
                if (!tested_cctv_relation_blocker) {
                    SecurityHarness blocked = {0};
                    GeOriginalStageSecurityProviders blocked_providers =
                        providers;
                    GeOriginalStageSecurityInstance blocked_instance;
                    void *blocked_definition = malloc(definition_size);
                    assert(blocked_definition != NULL);
                    blocked.request = &request;
                    blocked.omit_relation = 1;
                    blocked_providers.context = &blocked;
                    assert(construct_security_instance(&request,
                        blocked_definition, definition_size,
                        &blocked_providers, &blocked_instance)
                            == GE_ORIGINAL_STAGE_SECURITY_MODEL_RELATION_UNAVAILABLE);
                    assert(blocked.construct_calls == 1U
                        && blocked.place_calls == 1U);
                    {
                        GeOriginalStageSecurityModelAudit model_audit;
                        assert(ge_original_stage_security_model_audit(
                            blocked_definition, &model_audit)
                               && !model_audit.render_relations_ready);
                    }
                    free(blocked_definition);
                    tested_cctv_relation_blocker = 1;
                }
                ++stage_cctv;
                ++campaign_cctv;
            } else {
                if (!tested_autogun_beam_blocker) {
                    SecurityHarness blocked = {0};
                    GeOriginalStageSecurityProviders blocked_providers =
                        providers;
                    GeOriginalStageSecurityInstance blocked_instance;
                    void *blocked_definition = malloc(definition_size);
                    assert(blocked_definition != NULL);
                    blocked.request = &request;
                    blocked_providers.context = &blocked;
                    blocked_providers.allocate_stage = NULL;
                    assert(construct_security_instance(&request,
                        blocked_definition, definition_size,
                        &blocked_providers, &blocked_instance)
                            == GE_ORIGINAL_STAGE_SECURITY_BEAM_ALLOCATION_UNAVAILABLE);
                    assert(blocked.construct_calls == 1U
                        && blocked.place_calls == 1U
                        && blocked.allocation_calls == 0U);
                    free(blocked_definition);
                    tested_autogun_beam_blocker = 1;
                }
                assert(harness.allocation_calls == 1U);
                ++stage_autogun;
                ++campaign_autogun;
            }
            free(harness.beam);
            free(definition);
        }
        cctv_stages += stage_cctv != 0U;
        autogun_stages += stage_autogun != 0U;
        printf("%s: %lu CCTV, %lu autoguns\n", descriptor->key,
            (unsigned long)stage_cctv, (unsigned long)stage_autogun);
        ge_original_stage_setup_close(&setup);
    }
    assert(campaign_cctv == 15U && campaign_autogun == 41U);
    assert(cctv_distinct_tail_pad > 0U);
    assert(tested_cctv_relation_blocker && tested_autogun_beam_blocker
           && tested_activation_blocker);
    printf("Campaign security runtime: %lu CCTV across %lu stages, "
           "%lu autoguns across %lu stages; %lu CCTV common/tail pad split\n",
        (unsigned long)campaign_cctv, (unsigned long)cctv_stages,
        (unsigned long)campaign_autogun, (unsigned long)autogun_stages,
        (unsigned long)cctv_distinct_tail_pad);
    ge_asset_pack_close(&pack);
    return 0;
}
