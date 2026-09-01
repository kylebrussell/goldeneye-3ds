#include "ge_asset_pack.h"
#include "ge_original_stage_objectives.h"
#include "ge_stage_assets.h"

#include "bondconstants.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ObjectiveProviderHarness {
    const GeOriginalStageSetupRuntime *setup;
    size_t calls;
    size_t unavailable_command;
} ObjectiveProviderHarness;

static int is_object_type(uint8_t type)
{
    switch (type) {
    case PROPDEF_DOOR: case PROPDEF_PROP: case PROPDEF_KEY:
    case PROPDEF_ALARM: case PROPDEF_CCTV: case PROPDEF_MAGAZINE:
    case PROPDEF_COLLECTABLE: case PROPDEF_MONITOR:
    case PROPDEF_MULTI_MONITOR: case PROPDEF_RACK: case PROPDEF_AUTOGUN:
    case PROPDEF_HAT: case PROPDEF_AMMO: case PROPDEF_ARMOUR:
    case PROPDEF_GAS_RELEASING: case PROPDEF_VEHICHLE:
    case PROPDEF_AIRCRAFT: case PROPDEF_UNK41: case PROPDEF_GLASS:
    case PROPDEF_SAFE: case PROPDEF_TANK: case PROPDEF_TINTED_GLASS:
        return 1;
    default: return 0;
    }
}

static int is_criterion(uint8_t type)
{
    return type >= PROPDEF_OBJECTIVE_DESTROY_OBJECT
        && type <= PROPDEF_OBJECTIVE_COPY_ITEM;
}

static int uses_tag(uint8_t type)
{
    return type == PROPDEF_OBJECTIVE_DESTROY_OBJECT
        || type == PROPDEF_OBJECTIVE_COLLECT_OBJECT
        || type == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT
        || type == PROPDEF_OBJECTIVE_PHOTOGRAPH;
}

static void *provide_object(void *opaque, size_t command_index,
                            const GeOriginalStagePropRecord *record)
{
    ObjectiveProviderHarness *harness = opaque;
    assert(harness != NULL && harness->setup != NULL);
    assert(command_index < harness->setup->prop_record_count);
    assert(record == &harness->setup->prop_records[command_index]);
    assert(is_object_type(record->type));
    ++harness->calls;
    if (command_index == harness->unavailable_command) return NULL;
    /* An opaque, stable native address is sufficient for registry ownership
     * tests; the actual runtime provider returns its ObjectRecord definition. */
    return (void *)record;
}

static uint64_t hash_words(const GeOriginalStageSetupRuntime *setup)
{
    const uint8_t *bytes = (const uint8_t *)setup->propdefs_storage;
    const size_t size = setup->prop_word_count * sizeof(uint32_t);
    uint64_t hash = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < size; ++index) {
        hash ^= bytes[index];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static void audit_stage(const GeOriginalStageSetupRuntime *setup,
                        const GeOriginalStageObjectiveRegistry *registry,
                        size_t *tag_total, size_t *text_total,
                        size_t *objective_total, size_t *criterion_total,
                        size_t *enter_total, size_t *deposit_total,
                        size_t *photo_total, size_t *tag_use_total)
{
    size_t expected_tags = 0U, expected_texts = 0U;
    size_t expected_objectives = 0U, expected_criteria = 0U;
    size_t expected_enter = 0U, expected_deposit = 0U, expected_photo = 0U;
    size_t expected_tag_uses = 0U, index;
    size_t expected_blocked_tags = 0U;
    size_t expected_blocked_criteria = 0U;
    int inside_objective = 0;
    int32_t prior_tag = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    int32_t prior_text = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    int32_t prior_enter = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    int32_t prior_deposit = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    int32_t prior_photo = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    int32_t expected_objective_count = GE_ORIGINAL_STAGE_OBJECTIVE_NO_INDEX;
    size_t objective_cursor = 0U, criterion_cursor = 0U;

    for (index = 0U; index < setup->prop_record_count; ++index) {
        const GeOriginalStagePropRecord *record = &setup->prop_records[index];
        if (record->type == PROPDEF_TAG) {
            const GeOriginalStageTagEntry *tag =
                &registry->tags[expected_tags];
            const size_t target = (size_t)(record->relations[0]
                - setup->prop_records);
            assert(tag->record == record);
            assert(tag->command_index == index);
            assert(tag->target_command_index == target);
            assert(tag->tag_id == record->tag_id);
            assert(tag->next_tag == prior_tag);
            if (is_object_type(setup->prop_records[target].type)) {
                assert(tag->tagged_object
                    == (void *)&setup->prop_records[target]);
                assert(tag->blocker == GE_ORIGINAL_STAGE_OBJECTIVE_READY);
            } else {
                assert(tag->tagged_object == NULL);
                assert(tag->blocker
                    == GE_ORIGINAL_STAGE_OBJECTIVE_TARGET_NOT_OBJECT);
                ++expected_blocked_tags;
            }
            prior_tag = (int32_t)expected_tags++;
        } else if (record->type == PROPDEF_WATCH_MENU_OBJECTIVE_TEXT) {
            const GeOriginalStageObjectiveTextEntry *text =
                &registry->texts[expected_texts];
            assert(text->record == record && text->command_index == index);
            assert(text->menu == record->words[1]);
            assert(text->reserved == (uint16_t)(record->words[2] >> 16U));
            assert(text->text_id == (uint16_t)record->words[2]);
            assert(text->next_text == prior_text);
            prior_text = (int32_t)expected_texts++;
        } else if (record->type == PROPDEF_OBJECTIVE_START) {
            const GeOriginalStageObjectiveEntry *objective =
                &registry->objectives[objective_cursor];
            assert(!inside_objective);
            inside_objective = 1;
            assert(objective->record == record);
            assert(objective->command_index == index);
            assert(objective->menu == record->words[1]);
            assert(objective->reserved
                == (uint16_t)(record->words[2] >> 16U));
            assert(objective->text_id == (uint16_t)record->words[2]);
            assert(objective->unknown_c
                == (uint16_t)(record->words[3] >> 16U));
            assert(objective->unknown_e
                == (uint8_t)(record->words[3] >> 8U));
            assert(objective->difficulty == (int8_t)record->words[3]);
            assert(objective->first_criterion == criterion_cursor);
            assert(record->words[1] < GE_ORIGINAL_STAGE_OBJECTIVE_MAX);
            assert(registry->objective_by_menu[record->words[1]]
                == (int32_t)objective_cursor);
            if (expected_objective_count < (int32_t)record->words[1])
                expected_objective_count = (int32_t)record->words[1];
            ++objective_cursor;
            ++expected_objectives;
        } else if (record->type == PROPDEF_OBJECTIVE_END) {
            const GeOriginalStageObjectiveEntry *objective =
                &registry->objectives[objective_cursor - 1U];
            assert(inside_objective);
            assert(record->objective == objective->record);
            assert(objective->end_command_index == index);
            assert(objective->criterion_count
                == criterion_cursor - objective->first_criterion);
            inside_objective = 0;
        } else if (inside_objective && is_criterion(record->type)) {
            const GeOriginalStageObjectiveCriterion *criterion =
                &registry->criteria[criterion_cursor];
            size_t word;
            assert(criterion->record == record);
            assert(criterion->command_index == index);
            assert(criterion->objective_index == objective_cursor - 1U);
            assert(criterion->type == record->type);
            for (word = 0U; word < 4U; ++word)
                assert(criterion->words[word] == (word + 1U
                    < record->word_count ? record->words[word + 1U] : 0U));
            if (record->type == PROPDEF_OBJECTIVE_ENTER_ROOM) {
                assert(criterion->value_a == (int32_t)record->words[1]);
                assert(criterion->status == (int32_t)record->words[2]);
                assert(criterion->value_b == (int32_t)record->words[3]);
                assert(criterion->next_same_type == prior_enter);
                prior_enter = (int32_t)criterion_cursor;
                ++expected_enter;
            } else if (record->type
                    == PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM) {
                assert(criterion->value_a == (int32_t)record->words[1]);
                assert(criterion->value_b == (int32_t)record->words[2]);
                assert(criterion->status == (int32_t)record->words[3]);
                assert(criterion->next_same_type == prior_deposit);
                prior_deposit = (int32_t)criterion_cursor;
                ++expected_deposit;
            } else if (record->type == PROPDEF_OBJECTIVE_PHOTOGRAPH) {
                assert(criterion->value_a == (int32_t)record->words[1]);
                assert(criterion->status == (int32_t)record->words[2]);
                assert(criterion->next_same_type == prior_photo);
                prior_photo = (int32_t)criterion_cursor;
                ++expected_photo;
            }
            if (uses_tag(record->type)) {
                const GeOriginalStageTagEntry *tag;
                assert(criterion->tag_index >= 0);
                tag = &registry->tags[criterion->tag_index];
                assert(tag->tag_id == (uint16_t)record->words[1]);
                assert(criterion->tagged_object == tag->tagged_object);
                assert(criterion->blocker == tag->blocker);
                if (tag->blocker != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
                    ++expected_blocked_criteria;
                ++expected_tag_uses;
            }
            ++criterion_cursor;
            ++expected_criteria;
        }
    }
    assert(!inside_objective);
    assert(registry->tag_count == expected_tags);
    assert(registry->text_count == expected_texts);
    assert(registry->objective_entry_count == expected_objectives);
    assert(registry->criterion_count == expected_criteria);
    assert(registry->tag_head == prior_tag);
    assert(registry->text_head == prior_text);
    assert(registry->enter_room_head == prior_enter);
    assert(registry->deposit_room_head == prior_deposit);
    assert(registry->photograph_head == prior_photo);
    assert(registry->objective_count == expected_objective_count);
    assert(registry->blocked_tag_count == expected_blocked_tags);
    assert(registry->blocked_criterion_count == expected_blocked_criteria);
    *tag_total += expected_tags;
    *text_total += expected_texts;
    *objective_total += expected_objectives;
    *criterion_total += expected_criteria;
    *enter_total += expected_enter;
    *deposit_total += expected_deposit;
    *photo_total += expected_photo;
    *tag_use_total += expected_tag_uses;
}

int main(int argc, char **argv)
{
    GeAssetPack pack;
    size_t stage_index;
    size_t tag_total = 0U, text_total = 0U, objective_total = 0U;
    size_t criterion_total = 0U, enter_total = 0U, deposit_total = 0U;
    size_t photo_total = 0U, tag_use_total = 0U;
    size_t non_object_tag_total = 0U;
    assert(argc == 2);
    assert(ge_asset_pack_open(&pack, argv[1]) == GE_ASSET_PACK_OK);
    for (stage_index = 0U; stage_index < GE_STAGE_COUNT; ++stage_index) {
        const GeStageAssetDescriptor *descriptor =
            ge_stage_asset_descriptor((GeStageId)stage_index);
        GeOriginalStageSetupRuntime setup = {0};
        GeOriginalStageObjectiveRegistry registry = {0};
        GeOriginalStageObjectiveProviders providers;
        ObjectiveProviderHarness harness;
        uint64_t words_before;
        assert(descriptor != NULL);
        assert(ge_original_stage_setup_load(&pack, descriptor, &setup)
            == GE_ORIGINAL_STAGE_SETUP_OK);
        words_before = hash_words(&setup);
        memset(&harness, 0, sizeof(harness));
        harness.setup = &setup;
        harness.unavailable_command = SIZE_MAX;
        providers.context = &harness;
        providers.object_definition_by_command = provide_object;
        assert(ge_original_stage_objectives_build(
            &registry, &setup, &providers) == GE_ORIGINAL_STAGE_OBJECTIVE_OK);
        assert(registry.initialized);
        if (getenv("GE_STAGE_OBJECTIVE_VERBOSE") != NULL) {
            size_t tag_index;
            for (tag_index = 0U; tag_index < registry.tag_count;
                    ++tag_index) {
                const GeOriginalStageTagEntry *tag = &registry.tags[tag_index];
                const GeOriginalStagePropRecord *target =
                    &setup.prop_records[tag->target_command_index];
                printf("%s tag %u command %zu -> %zu type %u model %d "
                       "pad %d flags %08x\n", descriptor->key,
                       (unsigned)tag->tag_id, tag->command_index,
                       tag->target_command_index, (unsigned)target->type,
                       target->model_id, target->pad_id,
                       (unsigned)target->words[2]);
            }
        }
        audit_stage(&setup, &registry, &tag_total, &text_total,
            &objective_total, &criterion_total, &enter_total, &deposit_total,
            &photo_total, &tag_use_total);
        non_object_tag_total += registry.blocked_tag_count;
        assert(harness.calls
            == registry.tag_count - registry.blocked_tag_count);
        assert(words_before == hash_words(&setup));
        if (registry.tag_count != 0U) {
            GeOriginalStageObjectiveRegistry blocked = {0};
            GeOriginalStageObjectiveRegistry unbound = {0};
            size_t unavailable = SIZE_MAX;
            size_t expected_blocked_criteria = 0U;
            size_t expected_tag_criteria = 0U;
            size_t index;
            assert(ge_original_stage_objectives_build(
                &unbound, &setup, NULL) == GE_ORIGINAL_STAGE_OBJECTIVE_OK);
            assert(unbound.blocked_tag_count == unbound.tag_count);
            for (index = 0U; index < unbound.criterion_count; ++index) {
                if (unbound.criteria[index].tag_index >= 0) {
                    assert(unbound.criteria[index].tagged_object == NULL);
                    assert(unbound.criteria[index].blocker
                        != GE_ORIGINAL_STAGE_OBJECTIVE_READY);
                    ++expected_tag_criteria;
                }
            }
            assert(unbound.blocked_criterion_count == expected_tag_criteria);
            ge_original_stage_objectives_close(&unbound);
            for (index = 0U; index < registry.tag_count; ++index) {
                if (registry.tags[index].blocker
                        == GE_ORIGINAL_STAGE_OBJECTIVE_READY) {
                    unavailable = registry.tags[index].target_command_index;
                    break;
                }
            }
            if (unavailable == SIZE_MAX) goto no_live_tag;
            harness.calls = 0U;
            harness.unavailable_command = unavailable;
            assert(ge_original_stage_objectives_build(
                &blocked, &setup, &providers)
                    == GE_ORIGINAL_STAGE_OBJECTIVE_OK);
            assert(blocked.blocked_tag_count >= 1U);
            for (index = 0U; index < blocked.tag_count; ++index) {
                if (blocked.tags[index].target_command_index == unavailable) {
                    assert(blocked.tags[index].tagged_object == NULL);
                    assert(blocked.tags[index].blocker
                        == GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE);
                }
            }
            for (index = 0U; index < blocked.criterion_count; ++index) {
                if (blocked.criteria[index].blocker
                        != GE_ORIGINAL_STAGE_OBJECTIVE_READY)
                    ++expected_blocked_criteria;
                if (blocked.criteria[index].tag_index >= 0
                        && blocked.tags[blocked.criteria[index].tag_index]
                            .target_command_index == unavailable) {
                    assert(blocked.criteria[index].blocker
                        == GE_ORIGINAL_STAGE_OBJECTIVE_LIVE_OBJECT_UNAVAILABLE);
                }
            }
            assert(blocked.blocked_criterion_count
                == expected_blocked_criteria);
            ge_original_stage_objectives_close(&blocked);
no_live_tag:;
        }
        printf("%s: %lu tags, %lu objectives, %lu criteria, %lu/%lu blockers\n",
            descriptor->key, (unsigned long)registry.tag_count,
            (unsigned long)registry.objective_entry_count,
            (unsigned long)registry.criterion_count,
            (unsigned long)registry.blocked_tag_count,
            (unsigned long)registry.blocked_criterion_count);
        ge_original_stage_objectives_close(&registry);
        ge_original_stage_setup_close(&setup);
    }
    ge_asset_pack_close(&pack);
    printf("Campaign objective registry: %lu tags, %lu texts, %lu objectives, "
           "%lu criteria (%lu enter, %lu deposit, %lu photograph, "
           "%lu tag relations)\n",
        (unsigned long)tag_total, (unsigned long)text_total,
        (unsigned long)objective_total, (unsigned long)criterion_total,
        (unsigned long)enter_total, (unsigned long)deposit_total,
        (unsigned long)photo_total, (unsigned long)tag_use_total);
    assert(tag_total == 398U);
    assert(text_total == 105U);
    assert(objective_total == 80U);
    assert(criterion_total == 214U);
    assert(enter_total == 5U);
    assert(deposit_total == 4U);
    assert(photo_total == 2U);
    assert(tag_use_total == 111U);
    assert(non_object_tag_total == 31U);
    return 0;
}
