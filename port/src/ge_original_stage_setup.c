#include "ge_original_stage_setup.h"

#include "bondtypes.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#if defined(__GNUC__)
stagesetup g_CurrentSetup __attribute__((weak));
#else
extern stagesetup g_CurrentSetup;
#endif

static const GeOriginalStageSetupRuntime *ge_active_stage_setup;

enum {
    GE_SETUP_HEADER_WORDS = 10,
    GE_SETUP_HEADER_SIZE = 40,
    GE_SETUP_PAD_SIZE = 44,
    GE_SETUP_BOUNDPAD_SIZE = 68,
    GE_SETUP_INTRO_MAX_WORDS = 1024,
    GE_SETUP_TABLE_MAX_RECORDS = 1024,
    GE_SETUP_LIST_MAX_WORDS = 1024,
};

static uint32_t ge_setup_be32(const uint8_t *data)
{
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16)
        | ((uint32_t)data[2] << 8) | (uint32_t)data[3];
}

static uint16_t ge_setup_be16(const uint8_t *data)
{
    return (uint16_t)((uint16_t)data[0] << 8U | (uint16_t)data[1]);
}

static float ge_setup_float(const uint8_t *data)
{
    const uint32_t bits = ge_setup_be32(data);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static uint64_t ge_setup_fnv1a64(const uint8_t *data, size_t size)
{
    uint64_t value = UINT64_C(14695981039346656037);
    size_t index;
    for (index = 0U; index < size; ++index) {
        value ^= data[index];
        value *= UINT64_C(1099511628211);
    }
    return value;
}

static int ge_setup_offset(uint32_t offset, size_t size, size_t needed)
{
    return offset >= GE_SETUP_HEADER_SIZE && (size_t)offset <= size
        && needed <= size - (size_t)offset;
}

static int ge_setup_zero_record(const uint8_t *data, size_t size)
{
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (data[index] != 0U) return 0;
    }
    return 1;
}

static char *ge_setup_string(uint8_t *blob, size_t size, uint32_t offset)
{
    if (!ge_setup_offset(offset, size, 1U)
            || memchr(blob + offset, 0, size - offset) == NULL) return NULL;
    return (char *)(blob + offset);
}

static size_t ge_setup_table_count(const uint8_t *blob, size_t size,
                                   uint32_t offset, size_t stride)
{
    size_t count;
    for (count = 0U; count < GE_SETUP_TABLE_MAX_RECORDS; ++count) {
        if (!ge_setup_offset(offset, size, (count + 1U) * stride)) return SIZE_MAX;
        if (ge_setup_zero_record(blob + offset + count * stride, stride)) {
            return count;
        }
    }
    return SIZE_MAX;
}

static size_t ge_setup_zero_terminated_count_before(
    const uint8_t *blob, size_t size, uint32_t offset, size_t stride,
    uint32_t end_offset)
{
    size_t count;
    if (stride == 0U || offset >= end_offset || end_offset > size)
        return SIZE_MAX;
    for (count = 0U; count < GE_SETUP_TABLE_MAX_RECORDS; ++count) {
        const size_t needed = (count + 1U) * stride;
        if (needed > (size_t)(end_offset - offset)
                || !ge_setup_offset(offset, size, needed))
            return SIZE_MAX;
        if (ge_setup_zero_record(blob + offset + count * stride, stride))
            return count;
    }
    return SIZE_MAX;
}

static size_t ge_setup_waypoint_count(const uint8_t *blob, size_t size,
                                      uint32_t offset)
{
    size_t count;
    for (count = 0U; count < GE_SETUP_TABLE_MAX_RECORDS; ++count) {
        const uint8_t *record;
        if (!ge_setup_offset(offset, size, (count + 1U) * 16U)) {
            return SIZE_MAX;
        }
        record = blob + offset + count * 16U;
        /* The canonical waypoint terminator is {-1, NULL, 0, 0}; unlike the
         * other setup tables it is deliberately not an all-zero record. */
        if (ge_setup_be32(record) == UINT32_MAX
                && ge_setup_zero_record(record + 4U, 12U)) {
            return count;
        }
    }
    return SIZE_MAX;
}

static size_t ge_setup_prop_words(uint8_t type)
{
    switch (type) {
    case PROPDEF_NOTHING: case PROPDEF_DEBRIS: case PROPDEF_UNK16:
    case PROPDEF_OBJECTIVE_END:
    case PROPDEF_OBJECTIVE_NULL: case PROPDEF_OBJECTIVE_COPY_ITEM:
    case PROPDEF_END: return 1U;
    case PROPDEF_DOOR_SCALE: case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
    case PROPDEF_OBJECTIVE_COMPLETE_CONDITION:
    case PROPDEF_OBJECTIVE_FAIL_CONDITION:
    case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
    case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT: return 2U;
    case PROPDEF_LINK: case PROPDEF_GUARD_ATTRIBUTE: return 3U;
    case PROPDEF_SWITCH: case PROPDEF_TAG: case PROPDEF_OBJECTIVE_START:
    case PROPDEF_OBJECTIVE_PHOTOGRAPH: case PROPDEF_OBJECTIVE_ENTER_ROOM:
    case PROPDEF_WATCH_MENU_OBJECTIVE_TEXT: case PROPDEF_LOCK_DOOR: return 4U;
    case PROPDEF_SAFE_ITEM: case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT_IN_ROOM:
        return 5U;
    case PROPDEF_GUARD: case PROPDEF_CAMERAPOS: return 7U;
    case PROPDEF_RENAME: return 10U;
    case PROPDEF_PROP: case PROPDEF_ALARM: case PROPDEF_RACK:
    case PROPDEF_HAT: case PROPDEF_GAS_RELEASING: case PROPDEF_UNK41:
    case PROPDEF_GLASS: case PROPDEF_SAFE: return 32U;
    case PROPDEF_KEY: case PROPDEF_MAGAZINE: return 33U;
    case PROPDEF_COLLECTABLE: case PROPDEF_ARMOUR: return 34U;
    /* The authored US setup payload contains all five TintedGlass tail words;
     * its exact serialized width is 37, despite an old 0x24-word port helper. */
    case PROPDEF_TINTED_GLASS: return 37U;
    case PROPDEF_VEHICHLE: return 44U;
    case PROPDEF_AMMO: case PROPDEF_AIRCRAFT: return 45U;
    case PROPDEF_AUTOGUN: return 54U;
    case PROPDEF_TANK: return 56U;
    case PROPDEF_CCTV: return 59U;
    case PROPDEF_DOOR: case PROPDEF_MONITOR: return 64U;
    case PROPDEF_MULTI_MONITOR: return 149U;
    default: return 0U;
    }
}

static int ge_setup_prop_is_object(uint8_t type)
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

static GeOriginalStagePropRecord *ge_setup_prop_relative(
    GeOriginalStageSetupRuntime *runtime, size_t index, int32_t offset)
{
    const int64_t target = (int64_t)index + (int64_t)offset;
    return target >= 0 && (uint64_t)target < runtime->prop_record_count
        ? &runtime->prop_records[(size_t)target] : NULL;
}

static int ge_setup_prop_relation(GeOriginalStageSetupRuntime *runtime,
                                  size_t index, size_t slot, int32_t offset)
{
    GeOriginalStagePropRecord *record = &runtime->prop_records[index];
    GeOriginalStagePropRecord *target;
    if (slot >= 3U) return 0;
    target = ge_setup_prop_relative(runtime, index, offset);
    if (target == NULL) return 0;
    record->relation_offsets[slot] = offset;
    record->relations[slot] = target;
    if (record->relation_count <= slot) record->relation_count = slot + 1U;
    return 1;
}

static GeOriginalStagePropRecord *ge_setup_prop_tag(
    GeOriginalStageSetupRuntime *runtime, uint16_t tag_id)
{
    size_t index;
    for (index = 0U; index < runtime->prop_record_count; ++index) {
        GeOriginalStagePropRecord *record = &runtime->prop_records[index];
        if (record->type == PROPDEF_TAG && record->tag_id == tag_id) {
            return record;
        }
    }
    return NULL;
}

static int ge_setup_propdefs(GeOriginalStageSetupRuntime *runtime,
                             uint32_t offset, uint32_t end_offset)
{
    size_t cursor = 0U;
    size_t count = 0U;
    size_t index;
    GeOriginalStagePropRecord *objective = NULL;
    if ((offset & 3U) != 0U || (end_offset & 3U) != 0U
            || offset >= end_offset || end_offset > runtime->source_size) {
        return 0;
    }
    runtime->prop_word_count = (end_offset - offset) / 4U;
    runtime->propdefs_storage = calloc(runtime->prop_word_count,
                                       sizeof(*runtime->propdefs_storage));
    if (runtime->propdefs_storage == NULL) return 0;
    for (index = 0U; index < runtime->prop_word_count; ++index) {
        runtime->propdefs_storage[index] = ge_setup_be32(
            runtime->source_blob + offset + index * 4U);
    }
    while (cursor < runtime->prop_word_count) {
        const uint8_t type = (uint8_t)runtime->propdefs_storage[cursor];
        const size_t words = ge_setup_prop_words(type);
        if (words == 0U || words > runtime->prop_word_count - cursor) return 0;
        ++count;
        cursor += words;
        if (type == PROPDEF_END) break;
    }
    if (cursor != runtime->prop_word_count || count == 0U
            || (uint8_t)runtime->propdefs_storage[cursor - 1U]
                != PROPDEF_END) return 0;
    runtime->prop_records = calloc(count, sizeof(*runtime->prop_records));
    if (runtime->prop_records == NULL) return 0;
    runtime->prop_record_count = count;
    cursor = 0U;
    for (index = 0U; index < count; ++index) {
        GeOriginalStagePropRecord *record = &runtime->prop_records[index];
        const uint32_t word0 = runtime->propdefs_storage[cursor];
        record->words = runtime->propdefs_storage + cursor;
        record->source_offset = offset + cursor * 4U;
        record->type = (uint8_t)word0;
        record->word_count = ge_setup_prop_words(record->type);
        record->model_id = record->pad_id = record->chr_id = -1;
        record->ai_list_id = -1;
        if (ge_setup_prop_is_object(record->type)) {
            record->model_id = (int32_t)(record->words[1] >> 16U);
            record->pad_id = (int16_t)record->words[1];
        } else if (record->type == PROPDEF_GUARD) {
            record->chr_id = (int32_t)(record->words[1] >> 16U);
            record->pad_id = (uint16_t)record->words[1];
            record->model_id = (int32_t)(record->words[2] >> 16U);
            record->ai_list_id = (uint16_t)record->words[2];
        } else if (record->type == PROPDEF_TAG) {
            record->tag_id = (uint16_t)(record->words[1] >> 16U);
        }
        if (record->type == PROPDEF_OBJECTIVE_START) {
            objective = record;
        } else if (record->type == PROPDEF_OBJECTIVE_END) {
            record->objective = objective;
            objective = NULL;
        } else if (objective != NULL) {
            record->objective = objective;
        }
        cursor += record->word_count;
    }
    for (index = 0U; index < count; ++index) {
        GeOriginalStagePropRecord *record = &runtime->prop_records[index];
        switch (record->type) {
        case PROPDEF_TAG:
            if (!ge_setup_prop_relation(runtime, index, 0U,
                    (int16_t)record->words[1])) return 0;
            break;
        case PROPDEF_LINK:
            if (!ge_setup_prop_relation(runtime, index, 0U,
                    (int32_t)record->words[1])
                    || !ge_setup_prop_relation(runtime, index, 1U,
                        (int32_t)record->words[2])) return 0;
            break;
        case PROPDEF_SWITCH: case PROPDEF_LOCK_DOOR:
            if (!ge_setup_prop_relation(runtime, index, 0U,
                    (int32_t)record->words[1])
                    || !ge_setup_prop_relation(runtime, index, 1U,
                        (int32_t)record->words[2])) return 0;
            break;
        case PROPDEF_SAFE_ITEM:
            for (size_t slot = 0U; slot < 3U; ++slot) {
                if (!ge_setup_prop_relation(runtime, index, slot,
                        (int32_t)record->words[slot + 1U])) return 0;
            }
            break;
        case PROPDEF_RENAME:
            if (!ge_setup_prop_relation(runtime, index, 0U,
                    (int32_t)record->words[1])) return 0;
            break;
        case PROPDEF_DOOR:
            if ((int32_t)record->words[32] != 0
                    && !ge_setup_prop_relation(runtime, index, 0U,
                        (int32_t)record->words[32])) return 0;
            break;
        case PROPDEF_OBJECTIVE_DESTROY_OBJECT:
        case PROPDEF_OBJECTIVE_COLLECT_OBJECT:
        case PROPDEF_OBJECTIVE_DEPOSIT_OBJECT:
        case PROPDEF_OBJECTIVE_PHOTOGRAPH:
            record->relations[0] = ge_setup_prop_tag(
                runtime, (uint16_t)record->words[1]);
            record->relation_offsets[0] = (int32_t)(uint16_t)record->words[1];
            record->relation_count = record->relations[0] != NULL ? 1U : 0U;
            if (record->relations[0] == NULL) return 0;
            break;
        default: break;
        }
    }
    return objective == NULL;
}

static int *ge_setup_s32_list(GeOriginalStageSetupRuntime *runtime,
                              uint32_t offset)
{
    int *list;
    size_t count;
    if ((offset & 3U) != 0U || !ge_setup_offset(offset, runtime->source_size, 4U)
            || runtime->list_storage_count >= runtime->list_storage_capacity) {
        return NULL;
    }
    for (count = 0U; count < GE_SETUP_LIST_MAX_WORDS; ++count) {
        if (!ge_setup_offset(offset, runtime->source_size, (count + 1U) * 4U)) {
            return NULL;
        }
        if ((int32_t)ge_setup_be32(runtime->source_blob + offset + count * 4U)
                == -1) break;
    }
    if (count == GE_SETUP_LIST_MAX_WORDS) return NULL;
    list = malloc((count + 1U) * sizeof(*list));
    if (list == NULL) return NULL;
    for (size_t index = 0U; index <= count; ++index) {
        list[index] = (int)ge_setup_be32(
            runtime->source_blob + offset + index * 4U);
    }
    runtime->list_storage[runtime->list_storage_count++] = list;
    return list;
}

static int ge_setup_s32_list_values_in_range(const int *list, size_t limit)
{
    size_t index;
    if (list == NULL || limit == 0U) return 0;
    for (index = 0U; index < GE_SETUP_LIST_MAX_WORDS; ++index) {
        if (list[index] == -1) return 1;
        if (list[index] < 0 || (size_t)list[index] >= limit) return 0;
    }
    return 0;
}

static int ge_setup_intro_words(uint8_t *blob, size_t size, uint32_t offset,
                                size_t *word_count)
{
    size_t cursor = 0U;
    while (cursor < GE_SETUP_INTRO_MAX_WORDS
            && ge_setup_offset(offset, size, (cursor + 1U) * 4U)) {
        const uint32_t type = ge_setup_be32(blob + offset + cursor * 4U);
        size_t words;
        switch (type) {
        case INTROTYPE_END:
            *word_count = cursor + 1U;
            return 1;
        case INTROTYPE_SPAWN: words = 3U; break;
        case INTROTYPE_ITEM: words = 4U; break;
        case INTROTYPE_AMMO: words = 4U; break;
        case INTROTYPE_SWIRL: words = 8U; break;
        case INTROTYPE_ANIM: words = 2U; break;
        case INTROTYPE_CUFF: words = 2U; break;
        case INTROTYPE_CAMERA: words = 10U; break;
        case INTROTYPE_WATCH: words = 3U; break;
        case INTROTYPE_CREDITS: words = 2U; break;
        default: return 0;
        }
        if (words > GE_SETUP_INTRO_MAX_WORDS - cursor
                || !ge_setup_offset(offset, size, (cursor + words) * 4U)) {
            return 0;
        }
        cursor += words;
    }
    return 0;
}

static int ge_setup_relocate_credits(GeOriginalStageSetupRuntime *runtime)
{
    size_t cursor = 0U;
    while (cursor < runtime->intro_word_count) {
        const int32_t type = runtime->intro_storage[cursor];
        size_t words;
        if (type == INTROTYPE_END) return 1;
        switch (type) {
        case INTROTYPE_SPAWN: words = 3U; break;
        case INTROTYPE_ITEM: case INTROTYPE_AMMO: words = 4U; break;
        case INTROTYPE_SWIRL: words = 8U; break;
        case INTROTYPE_ANIM: case INTROTYPE_CUFF: words = 2U; break;
        case INTROTYPE_CAMERA: words = 10U; break;
        case INTROTYPE_WATCH: words = 3U; break;
        case INTROTYPE_CREDITS:
        {
            const uint32_t offset =
                (uint32_t)runtime->intro_storage[cursor + 1U];
            size_t count = 0U;
            size_t index;
            if (runtime->credits_storage != NULL) return 0;
            while (count < GE_SETUP_TABLE_MAX_RECORDS
                    && ge_setup_offset(offset, runtime->source_size,
                        (count + 1U) * 12U)) {
                const uint8_t *raw = runtime->source_blob + offset
                    + count * 12U;
                if (ge_setup_be16(raw) == 0U
                        && ge_setup_be16(raw + 2U) == 0U)
                    break;
                ++count;
            }
            if (count >= GE_SETUP_TABLE_MAX_RECORDS
                    || !ge_setup_offset(offset, runtime->source_size,
                        (count + 1U) * 12U))
                return 0;
            runtime->credits_storage = calloc(
                count + 1U, sizeof(*runtime->credits_storage));
            if (runtime->credits_storage == NULL) return -1;
            runtime->credits_count = count;
            for (index = 0U; index < count; ++index) {
                const uint8_t *raw = runtime->source_blob + offset
                    + index * 12U;
                CreditsEntry *entry = &runtime->credits_storage[index];
                entry->TextId1 = ge_setup_be16(raw);
                entry->TextId2 = ge_setup_be16(raw + 2U);
                entry->Position1 = (s16)ge_setup_be16(raw + 4U);
                entry->Alignment1 = ge_setup_be16(raw + 6U);
                entry->Position2 = (s16)ge_setup_be16(raw + 8U);
                entry->Alignment2 = ge_setup_be16(raw + 10U);
            }
            words = 2U;
            break;
        }
        default: return 0;
        }
        if (words > runtime->intro_word_count - cursor) return 0;
        cursor += words;
    }
    return 0;
}

void ge_original_stage_setup_close(GeOriginalStageSetupRuntime *runtime)
{
    if (runtime == NULL) return;
    if (ge_active_stage_setup == runtime) ge_active_stage_setup = NULL;
    free(runtime->prop_records);
    free(runtime->propdefs_storage);
    free(runtime->intro_storage);
    free(runtime->credits_storage);
    if (runtime->list_storage != NULL) {
        size_t index;
        for (index = 0U; index < runtime->list_storage_count; ++index) {
            free(runtime->list_storage[index]);
        }
    }
    free(runtime->list_storage);
    free(runtime->ailists_storage);
    free(runtime->patrolpaths_storage);
    free(runtime->waygroups_storage);
    free(runtime->waypoints_storage);
    free(runtime->boundpads_storage);
    free(runtime->pads_storage);
    free(runtime->setup);
    free(runtime->source_blob);
    memset(runtime, 0, sizeof(*runtime));
}

GeOriginalStageSetupStatus ge_original_stage_setup_load(
    GeAssetPack *pack, const GeStageAssetDescriptor *descriptor,
    GeOriginalStageSetupRuntime *runtime)
{
    GeOriginalStageSetupRuntime next;
    const GeAssetPackEntry *entry;
    uint32_t pointers[GE_SETUP_HEADER_WORDS];
    size_t pad_count;
    size_t boundpad_count;
    size_t intro_words;
    size_t waypoint_count;
    size_t waygroup_count;
    size_t patrolpath_count;
    size_t ailist_count;
    size_t index;

    if (pack == NULL || descriptor == NULL || runtime == NULL) {
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT;
    }
    if (!isfinite(descriptor->level_scale) || descriptor->level_scale <= 0.0f)
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT;
    memset(&next, 0, sizeof(next));
    entry = ge_asset_pack_find(pack, descriptor->setup_path);
    if (entry == NULL) return GE_ORIGINAL_STAGE_SETUP_ASSET_NOT_FOUND;
    if (entry->data_size != descriptor->expected_setup_size
            || entry->data_size < GE_SETUP_HEADER_SIZE
            || entry->data_size > SIZE_MAX) {
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    next.source_size = (size_t)entry->data_size;
    next.source_blob = malloc(next.source_size);
    if (next.source_blob == NULL) return GE_ORIGINAL_STAGE_SETUP_NO_MEMORY;
    if (ge_asset_pack_read(pack, descriptor->setup_path, next.source_blob,
            next.source_size, NULL) != GE_ASSET_PACK_OK) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    next.source_fnv1a64 = ge_setup_fnv1a64(next.source_blob, next.source_size);
    if (next.source_fnv1a64 != descriptor->expected_setup_fnv1a64) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    for (index = 0U; index < GE_SETUP_HEADER_WORDS; ++index) {
        pointers[index] = ge_setup_be32(next.source_blob + index * 4U);
    }
    if (!ge_setup_offset(pointers[6], next.source_size, GE_SETUP_PAD_SIZE)
            || !ge_setup_offset(pointers[7], next.source_size,
                               GE_SETUP_BOUNDPAD_SIZE)
            || !ge_setup_offset(pointers[3], next.source_size, 4U)
            || pointers[6] >= pointers[7] || pointers[7] >= pointers[3]
            || (pointers[7] - pointers[6]) % GE_SETUP_PAD_SIZE != 0U
            || pointers[3] >= pointers[2]
            || !ge_setup_intro_words(next.source_blob, next.source_size,
                                     pointers[2], &intro_words)) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    pad_count = (pointers[7] - pointers[6]) / GE_SETUP_PAD_SIZE;
    boundpad_count = ge_setup_zero_terminated_count_before(
        next.source_blob, next.source_size, pointers[7],
        GE_SETUP_BOUNDPAD_SIZE, pointers[3]);
    if (pad_count == 0U || boundpad_count == SIZE_MAX
            || !ge_setup_zero_record(next.source_blob + pointers[6]
                    + (pad_count - 1U) * GE_SETUP_PAD_SIZE,
                GE_SETUP_PAD_SIZE)) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    --pad_count;
    waypoint_count = ge_setup_waypoint_count(next.source_blob,
                                             next.source_size, pointers[0]);
    waygroup_count = ge_setup_table_count(next.source_blob, next.source_size,
                                          pointers[1], 12U);
    patrolpath_count = ge_setup_table_count(next.source_blob, next.source_size,
                                            pointers[4], 8U);
    ailist_count = ge_setup_table_count(next.source_blob, next.source_size,
                                        pointers[5], 8U);
    if (waypoint_count == SIZE_MAX || waygroup_count == SIZE_MAX
            || patrolpath_count == SIZE_MAX || ailist_count == SIZE_MAX) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    if (!ge_setup_propdefs(&next, pointers[3], pointers[2])) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
    }
    next.setup = calloc(1U, sizeof(*next.setup));
    /* Keep the original trailing plink==NULL records: canonical pad setup
     * walks both tables by sentinel rather than by a published count. */
    next.pads_storage = calloc(pad_count + 1U, sizeof(PadRecord));
    next.boundpads_storage = calloc(boundpad_count + 1U,
                                    sizeof(BoundPadRecord));
    next.intro_storage = calloc(intro_words, sizeof(*next.intro_storage));
    next.waypoints_storage = calloc(waypoint_count + 1U, sizeof(waypoint));
    next.waygroups_storage = calloc(waygroup_count + 1U, sizeof(waygroup));
    next.patrolpaths_storage = calloc(patrolpath_count + 1U,
                                      sizeof(PathRecord));
    next.ailists_storage = calloc(ailist_count + 1U, sizeof(AIListRecord));
    next.list_storage_capacity = waypoint_count + waygroup_count * 2U
        + patrolpath_count;
    next.list_storage = calloc(
                               next.list_storage_capacity != 0U
                                   ? next.list_storage_capacity : 1U,
                               sizeof(*next.list_storage));
    if (next.setup == NULL || next.pads_storage == NULL
            || next.boundpads_storage == NULL || next.intro_storage == NULL
            || next.waypoints_storage == NULL || next.waygroups_storage == NULL
            || next.patrolpaths_storage == NULL || next.ailists_storage == NULL
            || next.list_storage == NULL) {
        ge_original_stage_setup_close(&next);
        return GE_ORIGINAL_STAGE_SETUP_NO_MEMORY;
    }
    for (index = 0U; index < pad_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[6]
            + index * GE_SETUP_PAD_SIZE;
        PadRecord *pad = &((PadRecord *)next.pads_storage)[index];
        size_t axis;
        for (axis = 0U; axis < 3U; ++axis) {
            pad->pos.f[axis] = ge_setup_float(raw + axis * 4U);
            pad->up.f[axis] = ge_setup_float(raw + 12U + axis * 4U);
            pad->look.f[axis] = ge_setup_float(raw + 24U + axis * 4U);
            if (!isfinite(pad->pos.f[axis]) || !isfinite(pad->up.f[axis])
                    || !isfinite(pad->look.f[axis])) {
                ge_original_stage_setup_close(&next);
                return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
            }
        }
        pad->plink = ge_setup_string(next.source_blob, next.source_size,
                                     ge_setup_be32(raw + 36U));
        if (pad->plink == NULL || ge_setup_be32(raw + 40U) != 0U) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
        for (axis = 0U; axis < 3U; ++axis)
            pad->pos.f[axis] *= 1.0f / descriptor->level_scale;
    }
    for (index = 0U; index < boundpad_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[7]
            + index * GE_SETUP_BOUNDPAD_SIZE;
        BoundPadRecord *pad = &((BoundPadRecord *)next.boundpads_storage)[index];
        size_t axis;
        for (axis = 0U; axis < 3U; ++axis) {
            pad->pos.f[axis] = ge_setup_float(raw + axis * 4U);
            pad->up.f[axis] = ge_setup_float(raw + 12U + axis * 4U);
            pad->look.f[axis] = ge_setup_float(raw + 24U + axis * 4U);
        }
        pad->plink = ge_setup_string(next.source_blob, next.source_size,
                                     ge_setup_be32(raw + 36U));
        if (pad->plink == NULL || ge_setup_be32(raw + 40U) != 0U) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
        pad->bbox.xmin = ge_setup_float(raw + 44U);
        pad->bbox.xmax = ge_setup_float(raw + 48U);
        pad->bbox.ymin = ge_setup_float(raw + 52U);
        pad->bbox.ymax = ge_setup_float(raw + 56U);
        pad->bbox.zmin = ge_setup_float(raw + 60U);
        pad->bbox.zmax = ge_setup_float(raw + 64U);
        for (axis = 0U; axis < 3U; ++axis)
            pad->pos.f[axis] *= 1.0f / descriptor->level_scale;
        pad->bbox.xmin *= 1.0f / descriptor->level_scale;
        pad->bbox.xmax *= 1.0f / descriptor->level_scale;
        pad->bbox.ymin *= 1.0f / descriptor->level_scale;
        pad->bbox.ymax *= 1.0f / descriptor->level_scale;
        pad->bbox.zmin *= 1.0f / descriptor->level_scale;
        pad->bbox.zmax *= 1.0f / descriptor->level_scale;
    }
    for (index = 0U; index < intro_words; ++index) {
        next.intro_storage[index] = (int32_t)ge_setup_be32(
            next.source_blob + pointers[2] + index * 4U);
    }
    next.intro_word_count = intro_words;
    {
        const int credits_status = ge_setup_relocate_credits(&next);
        if (credits_status <= 0) {
            ge_original_stage_setup_close(&next);
            return credits_status < 0 ? GE_ORIGINAL_STAGE_SETUP_NO_MEMORY
                                      : GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
    }
    for (index = 0U; index < waypoint_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[0] + index * 16U;
        waypoint *record = &((waypoint *)next.waypoints_storage)[index];
        record->padID = (s32)ge_setup_be32(raw);
        record->neighbours = ge_setup_s32_list(&next, ge_setup_be32(raw + 4U));
        record->groupNum = (s32)ge_setup_be32(raw + 8U);
        record->dist = (s32)ge_setup_be32(raw + 12U);
        if (record->neighbours == NULL
                || !ge_setup_s32_list_values_in_range(
                    record->neighbours, waypoint_count)
                || record->padID < 0
                || (size_t)record->padID >= pad_count || record->groupNum < 0
                || (size_t)record->groupNum >= waygroup_count) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
    }
    /* The source table ends with {-1, NULL, 0, 0}. calloc supplies the final
     * three fields, but padID must retain its canonical negative sentinel or
     * get_ptrpreset_in_table_matching_tile walks into the following heap
     * allocation while searching for the end of the authored table. */
    ((waypoint *)next.waypoints_storage)[waypoint_count].padID = -1;
    for (index = 0U; index < waygroup_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[1] + index * 12U;
        waygroup *record = &((waygroup *)next.waygroups_storage)[index];
        record->neighbours = ge_setup_s32_list(&next, ge_setup_be32(raw));
        record->waypoints = ge_setup_s32_list(&next, ge_setup_be32(raw + 4U));
        record->dist = (s32)ge_setup_be32(raw + 8U);
        if (record->neighbours == NULL || record->waypoints == NULL
                || !ge_setup_s32_list_values_in_range(
                    record->neighbours, waygroup_count)
                || !ge_setup_s32_list_values_in_range(
                    record->waypoints, waypoint_count)) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
    }
    for (index = 0U; index < patrolpath_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[4] + index * 8U;
        PathRecord *record = &((PathRecord *)next.patrolpaths_storage)[index];
        record->waypoints = ge_setup_s32_list(&next, ge_setup_be32(raw));
        record->ID = raw[4];
        record->isLoop = raw[5];
        record->len = (u16)((u16)raw[6] << 8U | (u16)raw[7]);
        if (record->waypoints == NULL
                || !ge_setup_s32_list_values_in_range(
                    record->waypoints, waypoint_count)) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
    }
    for (index = 0U; index < ailist_count; ++index) {
        const uint8_t *raw = next.source_blob + pointers[5] + index * 8U;
        AIListRecord *record = &((AIListRecord *)next.ailists_storage)[index];
        const uint32_t ai_offset = ge_setup_be32(raw);
        if (!ge_setup_offset(ai_offset, next.source_size, 1U)) {
            ge_original_stage_setup_close(&next);
            return GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET;
        }
        record->ailist = (AIRecord *)(next.source_blob + ai_offset);
        record->ID = (int)ge_setup_be32(raw + 4U);
    }
    next.setup->pathwaypoints = next.waypoints_storage;
    next.setup->waypointgroups = next.waygroups_storage;
    next.setup->patrolpaths = next.patrolpaths_storage;
    next.setup->ailists = next.ailists_storage;
    next.setup->propDefs = (PropDefHeaderRecord *)next.propdefs_storage;
    next.setup->pads = next.pads_storage;
    next.setup->boundpads = next.boundpads_storage;
    next.setup->intro = next.intro_storage;
    next.descriptor = descriptor;
    next.pad_count = pad_count;
    next.boundpad_count = boundpad_count;
    next.waypoint_count = waypoint_count;
    next.waygroup_count = waygroup_count;
    next.patrolpath_count = patrolpath_count;
    next.ailist_count = ailist_count;
    next.relocated = GE_ORIGINAL_STAGE_SETUP_PADS
        | GE_ORIGINAL_STAGE_SETUP_BOUNDPADS | GE_ORIGINAL_STAGE_SETUP_INTRO
        | GE_ORIGINAL_STAGE_SETUP_PATHS | GE_ORIGINAL_STAGE_SETUP_AI
        | GE_ORIGINAL_STAGE_SETUP_PROPDEFS;
    next.loaded = 1U;
    *runtime = next;
    return GE_ORIGINAL_STAGE_SETUP_OK;
}

GeOriginalStageSetupStatus ge_original_stage_setup_bind_stan(
    GeOriginalStageSetupRuntime *runtime, const GeStanNativeMap *stan)
{
    size_t index;
    if (runtime == NULL || stan == NULL || runtime->loaded == 0U) {
        return GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT;
    }
    runtime->pad_stan_count = 0U;
    runtime->bound_pad_stan_count = 0U;
    for (index = 0U; index < runtime->pad_count; ++index) {
        PadRecord *pad = &((PadRecord *)runtime->pads_storage)[index];
        GeStanNativeTile *tile = NULL;
        if (pad->plink == NULL) {
            pad->stan = NULL;
            continue;
        }
        if (ge_original_stan_resolve_pad(stan, pad->plink,
                pad->pos.x, pad->pos.y, pad->pos.z, &tile) == 0) {
            pad->stan = NULL;
            if (pad->plink[0] != '\0'
                    && ge_original_stan_match_tile_name(stan, pad->plink)
                        == NULL) {
                return GE_ORIGINAL_STAGE_SETUP_STAN_UNRESOLVED;
            }
            continue;
        }
        pad->stan = (StandTile *)tile;
        ++runtime->pad_stan_count;
    }
    for (index = 0U; index < runtime->boundpad_count; ++index) {
        BoundPadRecord *pad = &((BoundPadRecord *)runtime->boundpads_storage)[index];
        GeStanNativeTile *tile = NULL;
        if (pad->plink == NULL) {
            pad->stan = NULL;
            continue;
        }
        if (ge_original_stan_resolve_pad(stan, pad->plink,
                pad->pos.x, pad->pos.y, pad->pos.z, &tile) == 0) {
            pad->stan = NULL;
            if (pad->plink[0] != '\0'
                    && ge_original_stan_match_tile_name(stan, pad->plink)
                        == NULL) {
                return GE_ORIGINAL_STAGE_SETUP_STAN_UNRESOLVED;
            }
            continue;
        }
        pad->stan = (StandTile *)tile;
        ++runtime->bound_pad_stan_count;
    }
    return GE_ORIGINAL_STAGE_SETUP_OK;
}

int ge_original_stage_setup_normal_spawn(
    GeOriginalStageSetupRuntime *runtime, GeOriginalStageSpawn *spawn)
{
    size_t cursor = 0U;
    if (runtime == NULL || spawn == NULL || runtime->loaded == 0U) return 0;
    while (cursor < runtime->intro_word_count) {
        const int32_t type = runtime->intro_storage[cursor];
        size_t words;
        if (type == INTROTYPE_END) break;
        switch (type) {
        case INTROTYPE_SPAWN:
            words = 3U;
            if (cursor + words <= runtime->intro_word_count
                    && runtime->intro_storage[cursor + 2U] == 0) {
                const int32_t pad_id = runtime->intro_storage[cursor + 1U];
                const PadRecord *pad;
                if (pad_id < 0 || (size_t)pad_id >= runtime->pad_count) return 0;
                pad = &((PadRecord *)runtime->pads_storage)[pad_id];
                spawn->pad_id = pad_id;
                memcpy(spawn->position, pad->pos.f, sizeof(spawn->position));
                memcpy(spawn->up, pad->up.f, sizeof(spawn->up));
                memcpy(spawn->look, pad->look.f, sizeof(spawn->look));
                spawn->plink = pad->plink;
                spawn->stan = pad->stan;
                return 1;
            }
            break;
        case INTROTYPE_ITEM: words = 4U; break;
        case INTROTYPE_AMMO: words = 4U; break;
        case INTROTYPE_SWIRL: words = 8U; break;
        case INTROTYPE_ANIM: words = 2U; break;
        case INTROTYPE_CUFF: words = 2U; break;
        case INTROTYPE_CAMERA: words = 10U; break;
        case INTROTYPE_WATCH: words = 3U; break;
        case INTROTYPE_CREDITS: words = 2U; break;
        default: return 0;
        }
        cursor += words;
    }
    return 0;
}

const CreditsEntry *ge_original_stage_setup_credits(
    const GeOriginalStageSetupRuntime *runtime, size_t *count)
{
    if (count != NULL) *count = 0U;
    if (runtime == NULL || runtime->loaded == 0U
            || runtime->credits_storage == NULL)
        return NULL;
    if (count != NULL) *count = runtime->credits_count;
    return runtime->credits_storage;
}

int ge_original_stage_setup_pad_placement(
    const GeOriginalStageSetupRuntime *runtime, int32_t pad_id,
    GeOriginalStagePadPlacement *placement)
{
    if (runtime == NULL || placement == NULL || runtime->loaded == 0U
            || pad_id < 0) return 0;
    memset(placement, 0, sizeof(*placement));
    placement->pad_id = pad_id;
    placement->room = -1;
    if (pad_id >= 10000) {
        const size_t index = (size_t)(pad_id - 10000);
        const BoundPadRecord *pad;
        if (index >= runtime->boundpad_count) return 0;
        pad = &((const BoundPadRecord *)runtime->boundpads_storage)[index];
        memcpy(placement->position, pad->pos.f, sizeof(placement->position));
        memcpy(placement->up, pad->up.f, sizeof(placement->up));
        memcpy(placement->look, pad->look.f, sizeof(placement->look));
        placement->bounds[0] = pad->bbox.xmin;
        placement->bounds[1] = pad->bbox.xmax;
        placement->bounds[2] = pad->bbox.ymin;
        placement->bounds[3] = pad->bbox.ymax;
        placement->bounds[4] = pad->bbox.zmin;
        placement->bounds[5] = pad->bbox.zmax;
        placement->plink = pad->plink;
        placement->stan = pad->stan;
        placement->is_bound_pad = 1U;
    } else {
        const size_t index = (size_t)pad_id;
        const PadRecord *pad;
        if (index >= runtime->pad_count) return 0;
        pad = &((const PadRecord *)runtime->pads_storage)[index];
        memcpy(placement->position, pad->pos.f, sizeof(placement->position));
        memcpy(placement->up, pad->up.f, sizeof(placement->up));
        memcpy(placement->look, pad->look.f, sizeof(placement->look));
        placement->plink = pad->plink;
        placement->stan = pad->stan;
    }
    if (placement->stan != NULL) {
        placement->room = (int16_t)((const StandTile *)placement->stan)->room;
        placement->has_stan = 1U;
    }
    return placement->plink != NULL;
}

stagesetup *ge_original_stage_setup_get(GeOriginalStageSetupRuntime *runtime)
{
    return runtime != NULL && runtime->loaded != 0U ? runtime->setup : NULL;
}

int ge_original_stage_setup_publish(
    const GeOriginalStageSetupRuntime *runtime)
{
    if (runtime == NULL || runtime->loaded == 0U || runtime->setup == NULL)
        return 0;
    g_CurrentSetup = *runtime->setup;
    ge_active_stage_setup = runtime;
    return 1;
}

uint32_t ge_original_stage_setup_publication_mask(
    const GeOriginalStageSetupRuntime *runtime)
{
    uint32_t mask = 0U;
    int paths_owned = 1;
    size_t index;
    if (runtime == NULL || runtime->loaded == 0U || runtime->setup == NULL)
        return 0U;
    if (g_CurrentSetup.pads == runtime->setup->pads)
        mask |= GE_ORIGINAL_STAGE_SETUP_PADS;
    if (g_CurrentSetup.boundpads == runtime->setup->boundpads)
        mask |= GE_ORIGINAL_STAGE_SETUP_BOUNDPADS;
    if (g_CurrentSetup.intro == runtime->setup->intro)
        mask |= GE_ORIGINAL_STAGE_SETUP_INTRO;
    for (index = 0U; index < runtime->waypoint_count && paths_owned; ++index) {
        const int *pointer =
            ((const waypoint *)runtime->waypoints_storage)[index].neighbours;
        size_t owned;
        paths_owned = 0;
        for (owned = 0U; owned < runtime->list_storage_count; ++owned) {
            if (pointer == runtime->list_storage[owned]) {
                paths_owned = 1;
                break;
            }
        }
        if (paths_owned && !ge_setup_s32_list_values_in_range(
                pointer, runtime->waypoint_count)) paths_owned = 0;
    }
    for (index = 0U; index < runtime->waygroup_count && paths_owned; ++index) {
        const waygroup *group =
            &((const waygroup *)runtime->waygroups_storage)[index];
        size_t owned;
        int neighbours_owned = 0;
        int waypoints_owned = 0;
        for (owned = 0U; owned < runtime->list_storage_count; ++owned) {
            if (group->neighbours == runtime->list_storage[owned])
                neighbours_owned = 1;
            if (group->waypoints == runtime->list_storage[owned])
                waypoints_owned = 1;
        }
        paths_owned = neighbours_owned && waypoints_owned
            && ge_setup_s32_list_values_in_range(
                group->neighbours, runtime->waygroup_count)
            && ge_setup_s32_list_values_in_range(
                group->waypoints, runtime->waypoint_count);
    }
    for (index = 0U; index < runtime->patrolpath_count && paths_owned;
            ++index) {
        const int *pointer =
            ((const PathRecord *)runtime->patrolpaths_storage)[index].waypoints;
        size_t owned;
        paths_owned = 0;
        for (owned = 0U; owned < runtime->list_storage_count; ++owned) {
            if (pointer == runtime->list_storage[owned]) {
                paths_owned = 1;
                break;
            }
        }
        if (paths_owned && !ge_setup_s32_list_values_in_range(
                pointer, runtime->waypoint_count)) paths_owned = 0;
    }
    if (paths_owned
            && g_CurrentSetup.pathwaypoints == runtime->setup->pathwaypoints
            && g_CurrentSetup.waypointgroups
                == runtime->setup->waypointgroups
            && g_CurrentSetup.patrolpaths == runtime->setup->patrolpaths)
        mask |= GE_ORIGINAL_STAGE_SETUP_PATHS;
    if (g_CurrentSetup.ailists == runtime->setup->ailists)
        mask |= GE_ORIGINAL_STAGE_SETUP_AI;
    if (g_CurrentSetup.propDefs == runtime->setup->propDefs)
        mask |= GE_ORIGINAL_STAGE_SETUP_PROPDEFS;
    return mask;
}

int ge_original_stage_setup_path_audit(
    const GeOriginalStageSetupRuntime *runtime,
    GeOriginalStagePathAudit *audit)
{
    GeOriginalStagePathAudit local;
    size_t index;
    memset(&local, 0, sizeof(local));
    local.first_invalid_waypoint = SIZE_MAX;
    if (runtime == NULL || runtime->loaded == 0U
            || runtime->waypoints_storage == NULL) {
        if (audit != NULL) *audit = local;
        return 0;
    }
    local.waypoint_count = runtime->waypoint_count;
    {
        const waypoint *terminator =
            &((const waypoint *)runtime->waypoints_storage)[
                runtime->waypoint_count];
        if (terminator->padID != -1 || terminator->neighbours != NULL
                || terminator->groupNum != 0 || terminator->dist != 0) {
            local.first_invalid_waypoint = runtime->waypoint_count;
            local.first_invalid_pad_id = terminator->padID;
            local.first_invalid_group = terminator->groupNum;
            if (audit != NULL) *audit = local;
            return 0;
        }
    }
    for (index = 0U; index < runtime->waypoint_count; ++index) {
        const waypoint *record =
            &((const waypoint *)runtime->waypoints_storage)[index];
        if (record->padID < 0 || (size_t)record->padID >= runtime->pad_count
                || record->groupNum < 0
                || (size_t)record->groupNum >= runtime->waygroup_count
                || !ge_setup_s32_list_values_in_range(
                    record->neighbours, runtime->waypoint_count)) {
            local.first_invalid_waypoint = index;
            local.first_invalid_pad_id = record->padID;
            local.first_invalid_group = record->groupNum;
            if (audit != NULL) *audit = local;
            return 0;
        }
        local.valid_waypoints++;
    }
    if (audit != NULL) *audit = local;
    return 1;
}

int ge_original_stage_setup_active_path_valid(void)
{
    return ge_original_stage_setup_path_audit(
        ge_active_stage_setup, NULL);
}

const GeOriginalStagePropRecord *ge_original_stage_setup_prop_record(
    const GeOriginalStageSetupRuntime *runtime, size_t index)
{
    return runtime != NULL && runtime->loaded != 0U
            && index < runtime->prop_record_count
        ? &runtime->prop_records[index] : NULL;
}

size_t ge_original_stage_setup_prop_type_count(
    const GeOriginalStageSetupRuntime *runtime, uint8_t type)
{
    size_t count = 0U;
    size_t index;
    if (runtime == NULL || runtime->loaded == 0U) return 0U;
    for (index = 0U; index < runtime->prop_record_count; ++index) {
        if (runtime->prop_records[index].type == type) ++count;
    }
    return count;
}

const char *ge_original_stage_setup_status_name(
    GeOriginalStageSetupStatus status)
{
    switch (status) {
    case GE_ORIGINAL_STAGE_SETUP_OK: return "ok";
    case GE_ORIGINAL_STAGE_SETUP_INVALID_ARGUMENT: return "invalid argument";
    case GE_ORIGINAL_STAGE_SETUP_ASSET_NOT_FOUND: return "asset not found";
    case GE_ORIGINAL_STAGE_SETUP_INVALID_ASSET: return "invalid asset";
    case GE_ORIGINAL_STAGE_SETUP_NO_MEMORY: return "no memory";
    case GE_ORIGINAL_STAGE_SETUP_STAN_UNRESOLVED: return "STAN unresolved";
    default: return "unknown";
    }
}
