#include "ge_asset_pack.h"
#include "ge_original_pitem_models.h"
#include "ge_original_stage_prop_materializer.h"
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
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus model_status;
    size_t stage_index;
    size_t records = 0U, model_instances = 0U;
    uint8_t model_seen[341] = {0};
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    models = ge_original_pitem_model_provider_create(
        &pack, 341U, 1U, &model_status);
    assert(models != NULL && model_status == GE_ORIGINAL_PITEM_MODEL_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup = {0};
        size_t command_index;
        assert(descriptor != NULL
               && ge_original_stage_setup_load(&pack, descriptor, &setup)
                    == GE_ORIGINAL_STAGE_SETUP_OK);
        for (command_index = 0U;
                command_index < setup.prop_record_count; ++command_index) {
            const GeOriginalStagePropRecord *record =
                &setup.prop_records[command_index];
            GeOriginalStagePropConstructionRequest request;
            GeOriginalStageSecurityModelAudit audit;
            void *model_header = NULL, *model = NULL, *definition;
            float scale = 0.0f;
            size_t definition_size;
            if (record->type != PROPDEF_CCTV
                    && record->type != PROPDEF_AUTOGUN) continue;
            assert(record->model_id >= 0 && record->model_id < 341
                   && ge_original_stage_prop_construction_request(
                        &setup, command_index, &request));
            definition_size =
                ge_original_stage_prop_native_definition_size(&request);
            definition = malloc(definition_size);
            assert(definition != NULL
                   && ge_original_stage_prop_native_definition_init(
                        &request, definition, definition_size)
                   && ge_original_pitem_model_resolve_instance(
                        models, record->model_id, &model_header, &model,
                        &scale));
            ((ObjectRecord *)definition)->model = model;
            assert(model_header == ((Model *)model)->obj && scale > 0.0f
                   && ge_original_stage_security_model_audit(
                        definition, &audit)
                   && audit.render_relations_ready);
            if (record->type == PROPDEF_CCTV) {
                assert((audit.switch_data_mask & UINT32_C(1)) != 0U);
            } else {
                assert((audit.switch_data_mask
                    & ((UINT32_C(1) << 1) | (UINT32_C(1) << 2)))
                    == ((UINT32_C(1) << 1) | (UINT32_C(1) << 2)));
            }
            model_seen[record->model_id] = 1U;
            ++records;
            assert(ge_original_pitem_model_release_instance(models, model));
            free(definition);
        }
        ge_original_stage_setup_close(&setup);
    }
    for (stage_index = 0U; stage_index < sizeof(model_seen); ++stage_index)
        model_instances += model_seen[stage_index] != 0U;
    assert(records == 56U && model_instances > 0U);
    printf("security authored model relations: %lu record / %lu unique model\n",
        (unsigned long)records, (unsigned long)model_instances);
    ge_original_pitem_model_provider_destroy(models);
    ge_asset_pack_close(&pack);
    return 0;
}
