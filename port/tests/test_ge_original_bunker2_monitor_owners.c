#include "ge_original_stage_monitor.h"
#include "ge_original_default_object_internal.h"
#include "ge_original_stage_prop_materializer.h"
#include "ge_original_stage_setup.h"
#include "ge_stage_assets.h"
#include "bondconstants.h"
#include "bondtypes.h"
#include "game/chrai.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Embedment g_Embedments[EMBEDMENT_ARR_MAX];

void modelSetScale(Model *model, f32 scale) { model->scale = scale; }

Embedment *embedmentAllocate(void)
{
    size_t index;
    for (index = 0U; index < EMBEDMENT_ARR_MAX; ++index)
        if ((g_Embedments[index].flags & EMBEDMENTFLAG_FREE) != 0) {
            g_Embedments[index].flags = 0;
            g_Embedments[index].projectile = NULL;
            return &g_Embedments[index];
        }
    return NULL;
}

void embedmentFree(Embedment *embedment)
{
    embedment->flags |= EMBEDMENTFLAG_FREE;
}

void ge_original_stage_guard_actor_reparent_prop(
    PropRecord *child, PropRecord *owner)
{
    child->parent = owner;
    if (owner->child != NULL) owner->child->next = child;
    child->prev = owner->child;
    child->next = NULL;
    child->stan = NULL;
    owner->child = child;
}

void chrpropDetach(PropRecord *prop)
{
    if (prop->parent != NULL) {
        if (prop->parent->child == prop) prop->parent->child = prop->prev;
        if (prop->prev != NULL) prop->prev->next = prop->next;
        if (prop->next != NULL) prop->next->prev = prop->prev;
        prop->parent = NULL;
        prop->prev = NULL;
        prop->next = NULL;
    }
}

PropRecord *ge_original_objInitPreallocatedSlice(
    ObjectRecord *object, ModelFileHeader *header, PropRecord *prop,
    Model *model, f32 pitem_scale, void *collision_data)
{
    (void)collision_data;
    if (object == NULL || header == NULL || prop == NULL || model == NULL)
        return NULL;
    object->model = model;
    object->prop = prop;
    object->projectile = NULL;
    model->obj = header;
    model->scale = pitem_scale;
    model->chr = NULL;
    prop->type = PROP_TYPE_OBJ;
    prop->obj = object;
    prop->stan = NULL;
    return prop;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    const GeStageAssetDescriptor *descriptor;
    GeOriginalStageSetupRuntime setup;
    size_t command;
    size_t owned = 0U;
    GeOriginalPitemModelProvider *models;
    GeOriginalPitemModelStatus model_status;
    void *rack_header = NULL;
    void *rack_model = NULL;
    float rack_scale = 0.0f;
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    descriptor = ge_stage_asset_descriptor_by_key("bunker2");
    assert(descriptor != NULL);
    assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
        == GE_ORIGINAL_STAGE_SETUP_OK);
    models = ge_original_pitem_model_provider_create(
        &pack, 341U, 12U, &model_status);
    assert(models != NULL && model_status == GE_ORIGINAL_PITEM_MODEL_OK
        && ge_original_pitem_model_resolve_instance(
            models, 76, &rack_header, &rack_model, &rack_scale));
    assert(((ModelFileHeader *)rack_header)->numSwitches >= 4);
    for (command = 0U; command < 4U; ++command)
        assert(ge_original_pitem_model_instance_switch_node(
            models, rack_model, command) != NULL);
    assert(ge_original_pitem_model_release_instance(models, rack_model));
    for (command = 0U; command < setup.prop_record_count; ++command) {
        const GeOriginalStagePropRecord *record =
            ge_original_stage_setup_prop_record(&setup, command);
        GeOriginalStagePropConstructionRequest request;
        void *definition;
        size_t definition_size;
        int32_t owner_command;
        int embedded;
        const GeOriginalStagePropRecord *owner;
        if (record == NULL || record->type != PROPDEF_MONITOR
                || !ge_original_stage_prop_construction_request(
                    &setup, command, &request)) continue;
        definition_size =
            ge_original_stage_prop_native_definition_size(&request);
        definition = calloc(1U, definition_size);
        assert(definition != NULL
            && ge_original_stage_prop_native_definition_init(
                &request, definition, definition_size));
        if (!ge_original_stage_monitor_owner_command_exact(
                &request, definition, &owner_command, &embedded)) {
            free(definition);
            continue;
        }
        assert(owner_command >= 0
            && (size_t)owner_command < setup.prop_record_count);
        owner = ge_original_stage_setup_prop_record(
            &setup, (size_t)owner_command);
        assert(owner != NULL);
        printf("monitor=%zu model=%d pad=%d part=%u offset=%d embedded=%d owner=%d "
               "owner_type=%u owner_model=%d owner_pad=%d "
               "owner_flags=%08x\n",
            command, record->model_id, record->pad_id,
            ((MonitorObjRecord *)definition)->OwnerPart,
            ((MonitorObjRecord *)definition)->OwnerOffset, embedded,
            owner_command, owner->type, owner->model_id, owner->pad_id,
            owner->words[2]);
        ++owned;
        free(definition);
    }
    assert(owned == 8U);
    memset(g_Embedments, 0, sizeof(g_Embedments));
    for (command = 0U; command < EMBEDMENT_ARR_MAX; ++command)
        g_Embedments[command].flags = EMBEDMENTFLAG_FREE;
    for (command = 0U; command < 2U; ++command) {
        const size_t owner_command = command == 0U ? 155U : 160U;
        const size_t first_child = command == 0U ? 156U : 161U;
        GeOriginalStagePropConstructionRequest owner_request;
        void *owner_definition;
        size_t owner_size;
        void *owner_header = NULL;
        void *owner_model = NULL;
        float owner_scale = 0.0f;
        PropRecord owner_prop;
        size_t child;
        assert(ge_original_stage_prop_construction_request(
            &setup, owner_command, &owner_request));
        owner_size = ge_original_stage_prop_native_definition_size(
            &owner_request);
        owner_definition = calloc(1U, owner_size);
        memset(&owner_prop, 0, sizeof(owner_prop));
        assert(owner_definition != NULL);
        assert(ge_original_stage_prop_native_definition_init(
            &owner_request, owner_definition, owner_size));
        ((ObjectRecord *)owner_definition)->prop = &owner_prop;
        owner_prop.obj = owner_definition;
        assert(ge_original_pitem_model_resolve_instance(
            models, owner_request.model_id, &owner_header,
            &owner_model, &owner_scale));
        assert(ge_original_objInitPreallocatedSlice(
            owner_definition, owner_header, &owner_prop,
            owner_model, owner_scale, NULL) != NULL);
        ((Model *)owner_model)->scale *=
            (float)((ObjectRecord *)owner_definition)->extrascale / 256.0f;
        for (child = first_child; child < first_child + 4U; ++child) {
            GeOriginalStagePropConstructionRequest child_request;
            void *child_definition;
            void *child_model = NULL;
            PropRecord child_prop;
            size_t child_size;
            memset(&child_prop, 0, sizeof(child_prop));
            assert(ge_original_stage_prop_construction_request(
                &setup, child, &child_request));
            assert(!child_request.placement_resolved);
            child_size = ge_original_stage_prop_native_definition_size(
                &child_request);
            child_definition = calloc(1U, child_size);
            assert(child_definition != NULL
                && ge_original_stage_prop_native_definition_init(
                    &child_request, child_definition, child_size));
            assert(!ge_original_stage_prop_native_bind_prop(
                &child_request, child_definition, &child_prop,
                sizeof(child_prop)));
            assert(ge_original_stage_monitor_bind_owned_prop_exact(
                    &child_request, child_definition, &child_prop,
                    sizeof(child_prop))
                && ge_original_stage_monitor_construct_owned_exact(
                    &child_request, child_definition, child_size,
                    &child_prop, sizeof(child_prop), models,
                    owner_definition, &owner_prop, NULL, 1, 1,
                    &child_model));
            assert(child_prop.parent == &owner_prop && child_model != NULL
                && ((ObjectRecord *)child_definition)->embedment != NULL);
            ge_original_stage_monitor_release_owned_exact(
                child_definition, models);
            free(child_definition);
        }
        assert(owner_prop.child == NULL);
        assert(ge_original_pitem_model_release_instance(
            models, owner_model));
        free(owner_definition);
    }
    ge_original_pitem_model_provider_destroy(models);
    ge_original_stage_setup_close(&setup);
    ge_asset_pack_close(&pack);
    puts("Bunker2 eight authored monitor owner edges inventoried");
    return 0;
}
