#include "ge_asset_pack.h"
#include "ge_original_character_models.h"

#include <bondconstants.h>
#include <bondtypes.h>

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* The generated canonical metadata table references the separately retained
 * chrkalash header. Its provider is not exercised here, but its translation
 * unit retains this original-loader hook. */
void modelCalculateRwDataLen(ModelFileHeader *header)
{
    (void)header;
}

static int supported(uint16_t opcode)
{
    switch (opcode & 0xffU) {
    case MODELNODE_OPCODE_HEADER:
    case MODELNODE_OPCODE_GROUP:
    case MODELNODE_OPCODE_LOD:
    case MODELNODE_OPCODE_BSP:
    case MODELNODE_OPCODE_BBOX:
    case MODELNODE_OPCODE_SHADOW:
    case MODELNODE_OPCODE_SWITCH:
    case MODELNODE_OPCODE_HEAD:
    case MODELNODE_OPCODE_DLCOLLISION:
        return 1;
    default:
        return 0;
    }
}

static size_t audit_tree(ModelFileHeader *header, size_t *display_lists,
                         int *has_header, int *has_head)
{
    ModelNode *node = header->RootNode;
    size_t visited = 0U;
    while (node != NULL && visited++ < 256U) {
        assert(supported(node->Opcode));
        assert(node->Data != NULL);
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_HEADER)
            *has_header = 1;
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_HEAD)
            *has_head = 1;
        if ((node->Opcode & 0xffU) == MODELNODE_OPCODE_DLCOLLISION) {
            ModelRoData_DisplayList_CollisionRecord *dl =
                &node->Data->DisplayListCollisions;
            assert(dl->Primary != NULL && dl->Vertices != NULL
                   && dl->numVertices > 0 && dl->numCollisionVertices >= 0);
            ++*display_lists;
        }
        if (node->Child != NULL) node = node->Child;
        else {
            while (node != NULL && node->Next == NULL) node = node->Parent;
            if (node != NULL) node = node->Next;
        }
    }
    assert(node == NULL && visited > 0U);
    return visited;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    GeOriginalCharacterModelProvider *provider;
    GeOriginalCharacterModelStatus status;
    GeOriginalCharacterModelStats stats;
    size_t dependency_count;
    size_t index;
    size_t body_count = 0U, head_count = 0U, integrated_count = 0U;
    size_t nodes = 0U, parts = 0U;
    void *maximum_part_model = NULL;
    size_t maximum_part_count = 0U;

    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    dependency_count = ge_original_character_model_dependency_count();
    assert(dependency_count == 71U);
    provider = ge_original_character_model_provider_create(
        &pack, dependency_count, dependency_count + 38U, &status);
    assert(provider != NULL && status == GE_ORIGINAL_CHARACTER_MODEL_OK);

    for (index = 0U; index < dependency_count; ++index) {
        GeOriginalCharacterModelMetadata metadata;
        ModelFileHeader *header;
        Model *model;
        void *header_ptr = NULL;
        void *model_ptr = NULL;
        float scale = 0.0f, pov = 0.0f;
        size_t model_parts;
        size_t part_index;
        int has_header = 0, has_head = 0;

        assert(ge_original_character_model_dependency_metadata(
            index, &metadata));
        assert(metadata.model_id >= 0 && metadata.name != NULL
               && metadata.name[0] == 'C' && scale >= 0.0f);
        body_count += metadata.is_body_dependency != 0;
        head_count += metadata.is_head_dependency != 0;
        integrated_count += metadata.has_integrated_head != 0;
        if (!ge_original_character_model_load(provider, metadata.model_id)) {
            fprintf(stderr, "character %d %s: %s\n", metadata.model_id,
                    metadata.name, ge_original_character_model_status_name(
                        ge_original_character_model_last_status(provider)));
            assert(0);
        }
        assert(ge_original_character_model_resolve_instance(
            provider, metadata.model_id, &header_ptr, &model_ptr,
            &scale, &pov));
        header = header_ptr;
        model = model_ptr;
        assert(header != NULL && model != NULL && model->obj == header);
        assert(header->RootNode != NULL && header->numMatrices > 0
               && header->numRecords > 0 && header->Textures != NULL
               && model->render_pos != NULL && model->datas != NULL
               && model->rwdatalen == header->numRecords);
        assert(isfinite(scale) && scale > 0.0f && isfinite(pov) && pov > 0.0f);
        nodes += audit_tree(header, &parts, &has_header, &has_head);
        if (metadata.is_body_dependency) {
            assert(has_header);
            assert(has_head == !metadata.has_integrated_head);
        }
        model_parts = ge_original_character_model_scene_part_count(
            provider, metadata.model_id);
        assert(model_parts > 0U);
        for (part_index = 0U; part_index < model_parts; ++part_index) {
            GeOriginalCharacterModelScenePart part;
            assert(ge_original_character_model_scene_part(
                provider, metadata.model_id, part_index, &part));
            assert(part.blob != NULL && part.blob_size > 0U
                   && part.primary_offset < part.blob_size
                   && part.segment4_offset < part.blob_size);
        }
    }
    assert(body_count == 38U && head_count == 33U);
    assert(integrated_count > 0U);
    for (index = 0U; index < dependency_count; ++index) {
        GeOriginalCharacterModelMetadata metadata;
        GeOriginalCharacterModelPair pair;
        int expected_head;
        const float position[3] = {
            (float)index + 1.0f, 100.0f + (float)index,
            -(float)index - 2.0f
        };
        assert(ge_original_character_model_dependency_metadata(
            index, &metadata));
        if (!metadata.is_body_dependency) continue;
        expected_head = metadata.has_integrated_head ? -1
            : (metadata.model_id == BODY_Brosnan_Tuxedo
                ? BODY_Male_Pierce_Bond_Tuxedo : 42);
        assert(ge_original_character_model_resolve_pair(
            provider, metadata.model_id,
            expected_head, 0, &pair));
        assert(pair.body_id == metadata.model_id
               && pair.head_id == expected_head
               && pair.model_header != NULL && pair.model_instance != NULL
               && pair.matrix_count > 0U);
        {
            size_t bulk_count = 0U;
            GeOriginalCharacterModelScenePart *bulk_parts;
            size_t part_index;
            assert(ge_original_character_model_instance_scene_parts(
                provider, pair.model_instance, NULL, 0U, &bulk_count));
            assert(bulk_count
                == ge_original_character_model_instance_scene_part_count(
                    provider, pair.model_instance));
            bulk_parts = calloc(bulk_count, sizeof(*bulk_parts));
            assert(bulk_count == 0U || bulk_parts != NULL);
            assert(ge_original_character_model_instance_scene_parts(
                provider, pair.model_instance, bulk_parts, bulk_count,
                &bulk_count));
            for (part_index = 0U; part_index < bulk_count; ++part_index) {
                GeOriginalCharacterModelScenePart individual;
                assert(ge_original_character_model_instance_scene_part(
                    provider, pair.model_instance, part_index, &individual));
                assert(individual.blob == bulk_parts[part_index].blob
                    && individual.blob_size
                        == bulk_parts[part_index].blob_size
                    && individual.primary_offset
                        == bulk_parts[part_index].primary_offset
                    && individual.secondary_offset
                        == bulk_parts[part_index].secondary_offset
                    && individual.segment4_offset
                        == bulk_parts[part_index].segment4_offset);
            }
            if (bulk_count > maximum_part_count) {
                maximum_part_count = bulk_count;
                maximum_part_model = pair.model_instance;
            }
            free(bulk_parts);
        }
        assert(ge_original_character_model_instance_set_root(
            pair.model_instance, position, 0.25f));
        {
            size_t shadow_count=
                ge_original_character_model_instance_shadow_count(
                    provider,pair.model_instance);
            size_t shadow_index;
            assert(shadow_count>0U);
            for(shadow_index=0U;shadow_index<shadow_count;++shadow_index){
                GeOriginalCharacterModelShadow shadow;
                assert(ge_original_character_model_instance_shadow(
                    provider,pair.model_instance,shadow_index,&shadow));
                assert(isfinite(shadow.position[0])
                       &&isfinite(shadow.position[1])
                       &&isfinite(shadow.size[0])&&shadow.size[0]>=0.0f
                       &&isfinite(shadow.size[1])&&shadow.size[1]>=0.0f
                       &&isfinite(shadow.height_above_ground)
                       &&shadow.matrix_index>=0
                       &&(size_t)shadow.matrix_index<pair.matrix_count);
            }
        }
    }
    {
        const size_t iterations = 20000U;
        GeOriginalCharacterModelScenePart *bulk_parts;
        clock_t individual_start;
        clock_t individual_end;
        clock_t bulk_start;
        clock_t bulk_end;
        size_t part_index;

        assert(maximum_part_model != NULL && maximum_part_count != 0U);
        bulk_parts = calloc(maximum_part_count, sizeof(*bulk_parts));
        assert(bulk_parts != NULL);
        individual_start = clock();
        for (index = 0U; index < iterations; ++index)
            for (part_index = 0U; part_index < maximum_part_count;
                    ++part_index) {
                GeOriginalCharacterModelScenePart individual;
                assert(ge_original_character_model_instance_scene_part(
                    provider, maximum_part_model, part_index, &individual));
            }
        individual_end = clock();
        bulk_start = clock();
        for (index = 0U; index < iterations; ++index) {
            size_t bulk_count = 0U;
            assert(ge_original_character_model_instance_scene_parts(
                provider, maximum_part_model, bulk_parts,
                maximum_part_count, &bulk_count));
            assert(bulk_count == maximum_part_count);
        }
        bulk_end = clock();
        printf("character scene enumeration (%zu parts): individual %.3f us, "
               "bulk %.3f us\n", maximum_part_count,
               1000000.0 * (double)(individual_end - individual_start)
                    / ((double)CLOCKS_PER_SEC * (double)iterations),
               1000000.0 * (double)(bulk_end - bulk_start)
                    / ((double)CLOCKS_PER_SEC * (double)iterations));
        free(bulk_parts);
    }
    /* Cast closure now legitimately packages body id 1.  Exercise the
     * provider's invalid-id contract with an out-of-domain id instead of a
     * model whose availability depends on the staged gameplay/front-end set. */
    assert(!ge_original_character_model_load(provider, -1));
    assert(ge_original_character_model_last_status(provider)
           == GE_ORIGINAL_CHARACTER_MODEL_INVALID_ID);
    ge_original_character_model_get_stats(provider, &stats);
    assert(stats.loaded_models == dependency_count
           && stats.instantiated_models == dependency_count + body_count
           && stats.last_unsupported_opcode == 0U
           /* The cast closure intentionally expands the exact packaged model
            * set beyond the original Dam-only byte total.  Per-model hashes
            * and the generated manifest above remain the exactness oracle. */
           && stats.source_blob_bytes >= 758656U
           && stats.native_resource_bytes > stats.source_blob_bytes
           && stats.native_instance_bytes > 0U);
    printf("character provider: %zu exact models, %zu nodes, %zu display lists, "
           "%zu integrated-head records\n", dependency_count, nodes, parts,
           integrated_count);
    ge_original_character_model_provider_destroy(provider);
    ge_asset_pack_close(&pack);
    return 0;
}
