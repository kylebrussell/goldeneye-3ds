#include "ge_original_stage_monitor.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"
#include "bondconstants.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int model_available(void *context, int32_t model_id)
{
    (void)context;
    return model_id >= 0;
}

static int construct_special(
    void *context, const GeOriginalStagePropConstructionRequest *request)
{
    (void)context;
    (void)request;
    return 1;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    const GeStageAssetDescriptor *descriptor;
    GeOriginalStageSetupRuntime setup;
    GeOriginalStagePropMaterializerProviders providers = {
        .capabilities = GE_ORIGINAL_STAGE_PROP_CAP_MONITOR
            | GE_ORIGINAL_STAGE_PROP_CAP_DEFAULT_OBJECT,
        .model_available = model_available,
        .construct_default_object = construct_special,
        .construct_special_object = construct_special,
    };
    const size_t child_commands[2] = {142U, 143U};
    size_t index;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    descriptor = ge_stage_asset_descriptor_by_key("caverns");
    assert(descriptor != NULL);
    assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
        == GE_ORIGINAL_STAGE_SETUP_OK);
    assert(setup.prop_record_count > 144U);

    for (index = 0U; index < 2U; ++index) {
        const size_t command = child_commands[index];
        const GeOriginalStagePropRecord *record =
            ge_original_stage_setup_prop_record(&setup, command);
        GeOriginalStagePropConstructionRequest request;
        GeOriginalStagePropClassification classification;
        void *definition;
        size_t definition_size;
        int32_t owner_command = -1;
        int embedded = -1;

        assert(record != NULL && record->type == PROPDEF_MONITOR);
        assert(record->pad_id == (int32_t)(144U - command));
        assert((record->words[2] & PROPFLAG_INSIDEANOTHEROBJ) != 0U);
        assert(ge_original_stage_prop_construction_request(
            &setup, command, &request));
        definition_size = ge_original_stage_prop_native_definition_size(
            &request);
        definition = calloc(1U, definition_size);
        assert(definition != NULL);
        assert(ge_original_stage_prop_native_definition_init(
            &request, definition, definition_size));
        assert(ge_original_stage_monitor_owner_command_exact(
            &request, definition, &owner_command, &embedded));
        assert(owner_command == 144 && embedded == 0);
        classification = ge_original_stage_prop_classify(record, &providers);
        assert(classification.blocker == GE_ORIGINAL_STAGE_PROP_READY);
        free(definition);
    }
    for (index = 144U; index <= 146U; ++index) {
        const GeOriginalStagePropRecord *owner =
            ge_original_stage_setup_prop_record(&setup, index);
        GeOriginalStagePropClassification classification;
        assert(owner != NULL && owner->type == PROPDEF_PROP
            && owner->pad_id == 1
            && (owner->words[2] & PROPFLAG_INSIDEANOTHEROBJ) != 0U);
        classification = ge_original_stage_prop_classify(owner, &providers);
        assert(classification.blocker == GE_ORIGINAL_STAGE_PROP_READY);
    }
    {
        const GeOriginalStagePropRecord *root =
            ge_original_stage_setup_prop_record(&setup, 147U);
        assert(root != NULL && root->type == PROPDEF_PROP
            && (root->words[2] & PROPFLAG_INSIDEANOTHEROBJ) == 0U);
    }

    ge_original_stage_setup_close(&setup);
    ge_asset_pack_close(&pack);
    puts("Caverns owner graph 142/143 -> 144 -> 145 -> 146 -> 147 passed");
    return 0;
}
