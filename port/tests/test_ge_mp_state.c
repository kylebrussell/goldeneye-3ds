#include "ge_mp_state.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Fixture {
    GeMpRegistryEntry storage[4];
    GeMpRegistry registry;
    uint32_t keys[4], free_order[2];
    uint64_t ids[2], active[2], room[2], child[2];
    uint8_t u8, boolean, owners[2];
    uint16_t u16;
    uint32_t u32, slot;
    uint64_t u64, ref;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    float f32, positions[2][3];
    GeMpFieldSchema global_schema[16], actor_schema[2];
    GeMpRecordSchema record_schema[2];
    GeMpStateSchema schema;
    GeMpStateField globals[16], actors[2][2];
    GeMpStateRecord records[3];
    GeMpCanonicalSnapshot snapshot;
} Fixture;

static void field(GeMpFieldSchema *schema, GeMpStateField *value,
    uint16_t id, uint8_t type, uint32_t count, const void *data)
{
    *schema = (GeMpFieldSchema){id, type, 0, count, count};
    *value = (GeMpStateField){id, type, count, data};
}

static void fixture_init(Fixture *f, int poison)
{
    unsigned i;
    const void *values[12];
    memset(f, poison, sizeof(*f));
    assert(ge_mp_registry_init(&f->registry, f->storage, 4) == GE_MP_STATE_OK);
    for (i = 0; i < 4; ++i) f->keys[i] = ge_mp_registry_key(1, 2, (uint16_t)(i + 1U));
    assert(ge_mp_registry_define(&f->registry, f->keys[2]) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&f->registry, f->keys[0]) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&f->registry, f->keys[3]) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&f->registry, f->keys[1]) == GE_MP_STATE_OK);
    for (i = 0; i < 2; ++i) {
        assert(ge_mp_registry_acquire(&f->registry, f->keys[i], &f->ids[i]) == GE_MP_STATE_OK);
        f->active[i] = f->room[i] = f->child[i] = f->ids[i];
        f->free_order[i] = f->keys[i + 2U];
        f->owners[i] = (uint8_t)i;
        f->positions[i][0] = (float)i + 1.0f;
        f->positions[i][1] = -2.0f;
        f->positions[i][2] = 0.0f;
    }
    f->u8 = 0xab; f->u16 = 0xabcd; f->u32 = 0x89abcdefU;
    f->u64 = UINT64_C(0xfedcba9876543210);
    f->i8 = INT8_MIN; f->i16 = INT16_MIN; f->i32 = INT32_MIN; f->i64 = INT64_MIN;
    f->f32 = -0.25f; f->boolean = 1; f->ref = f->ids[1]; f->slot = f->keys[3];
    field(&f->global_schema[0], &f->globals[0], 1, GE_MP_STATE_ACTOR_REF, 2, f->active);
    field(&f->global_schema[1], &f->globals[1], 2, GE_MP_STATE_SLOT_REF, 2, f->free_order);
    field(&f->global_schema[2], &f->globals[2], 3, GE_MP_STATE_ACTOR_REF, 2, f->room);
    field(&f->global_schema[3], &f->globals[3], 4, GE_MP_STATE_ACTOR_REF, 2, f->child);
    values[0] = &f->u8; values[1] = &f->u16; values[2] = &f->u32; values[3] = &f->u64;
    values[4] = &f->i8; values[5] = &f->i16; values[6] = &f->i32; values[7] = &f->i64;
    values[8] = &f->f32; values[9] = &f->boolean; values[10] = &f->ref; values[11] = &f->slot;
    for (i = 0; i < 12; ++i)
        field(&f->global_schema[i + 4U], &f->globals[i + 4U],
            (uint16_t)(256U + i), (uint8_t)(i + 1U), 1, values[i]);
    f->global_schema[14].flags = f->global_schema[15].flags = GE_MP_FIELD_NULLABLE;
    for (i = 0; i < 2; ++i) {
        field(&f->actor_schema[0], &f->actors[i][0], 5, GE_MP_STATE_U8, 1, &f->owners[i]);
        field(&f->actor_schema[1], &f->actors[i][1], 256, GE_MP_STATE_F32, 3, f->positions[i]);
        f->records[i + 1U] = (GeMpStateRecord){f->ids[i], 1, f->actors[i], 2};
    }
    f->record_schema[0] = (GeMpRecordSchema){0, GE_MP_SYSTEM_SESSION | GE_MP_SYSTEM_RNG,
        f->global_schema, 16};
    f->record_schema[1] = (GeMpRecordSchema){1, GE_MP_SYSTEM_PLAYERS, f->actor_schema, 2};
    f->schema = (GeMpStateSchema){7, GE_MP_SYSTEM_SESSION | GE_MP_SYSTEM_PLAYERS |
        GE_MP_SYSTEM_RNG, 1, f->record_schema, 2};
    f->records[0] = (GeMpStateRecord){0, 0, f->globals, 16};
    f->snapshot = (GeMpCanonicalSnapshot){.session = 0x12345678U,
        .tick = UINT64_C(0x0102030405060708), .enabled_systems = f->schema.registered_systems,
        .schema = &f->schema, .registry = &f->registry, .records = f->records, .record_count = 3,
        .coverage = {f->schema.registered_systems, 0, 0}};
    for (i = 0; i < 32; ++i) {
        f->snapshot.build_sha256[i] = (uint8_t)i;
        f->snapshot.content_sha256[i] = (uint8_t)(i + 32U);
        f->snapshot.rules_sha256[i] = (uint8_t)(i + 64U);
    }
}

static void expect_failure(Fixture *f, GeMpStateStatus expected)
{
    uint8_t output[2048], digest[32], before[2048];
    size_t size = 12345;
    GeMpCoverageReport report;
    memset(output, 0x5a, sizeof(output)); memcpy(before, output, sizeof(output));
    memset(digest, 0x5a, sizeof(digest));
    assert(ge_mp_state_measure(&f->snapshot, &size, &report) == expected && size == 12345);
    assert(ge_mp_state_encode(&f->snapshot, output, sizeof(output), &size, &report) == expected);
    assert(size == 12345 && memcmp(output, before, sizeof(output)) == 0);
    assert(ge_mp_state_sha256(&f->snapshot, digest, &report) == expected);
    assert(memcmp(digest, before, sizeof(digest)) == 0);
}

static void hex_digest(const char *input, size_t size, const char *expected)
{
    uint8_t digest[32];
    char hex[65];
    unsigned i;
    assert(ge_mp_sha256(input, size, digest) == GE_MP_STATE_OK);
    for (i = 0; i < 32; ++i) (void)snprintf(hex + 2U * i, 3, "%02x", digest[i]);
    assert(strcmp(hex, expected) == 0);
}

static void test_sha(void)
{
    char *million = malloc(1000000);
    uint8_t digest[32], before[32];
    assert(million != NULL);
    hex_digest(NULL, 0, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    hex_digest("abc", 3, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    hex_digest("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56,
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    memset(million, 'a', 1000000);
    hex_digest(million, 1000000, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    free(million);
    memset(digest, 0xa7, sizeof(digest)); memcpy(before, digest, sizeof(digest));
    assert(ge_mp_sha256(NULL, 1, digest) == GE_MP_STATE_INVALID);
    assert(memcmp(digest, before, sizeof(digest)) == 0);
    assert(ge_mp_sha256("a", 1, NULL) == GE_MP_STATE_INVALID);
}

static void test_registry(void)
{
    GeMpRegistryEntry entries[32];
    GeMpRegistry registry;
    uint64_t current[32] = {0}, retired[32] = {0}, id = UINT64_MAX;
    uint32_t generations[32] = {0}, random = 0x6a12fedcU;
    unsigned i;
    size_t at;
    assert(ge_mp_registry_init(&registry, entries, 32) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry, 1) == GE_MP_STATE_INVALID);
    for (i = 32; i > 0; --i)
        assert(ge_mp_registry_define(&registry, ge_mp_registry_key(2, 255, (uint16_t)(i - 1U))) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry, ge_mp_registry_key(2, 255, 0)) == GE_MP_STATE_INVALID);
    assert(ge_mp_registry_define(&registry, ge_mp_registry_key(2, 255, 32)) == GE_MP_STATE_CAPACITY);
    for (i = 0; i < 20000; ++i) {
        unsigned slot;
        uint32_t key;
        random = random * 1664525U + 1013904223U;
        slot = (random >> 16) % 32U;
        key = ge_mp_registry_key(2, 255, (uint16_t)slot);
        if (current[slot]) {
            assert(ge_mp_registry_acquire(&registry, key, &id) == GE_MP_STATE_INVALID);
            assert(id == UINT64_MAX);
            assert(ge_mp_registry_resolve(&registry, current[slot], &at) == GE_MP_STATE_OK && at == slot);
            assert(ge_mp_registry_release(&registry, current[slot]) == GE_MP_STATE_OK);
            retired[slot] = current[slot]; current[slot] = 0;
            assert(ge_mp_registry_release(&registry, retired[slot]) == GE_MP_STATE_STALE);
        } else {
            assert(ge_mp_registry_acquire(&registry, key, &current[slot]) == GE_MP_STATE_OK);
            assert((uint32_t)current[slot] == ++generations[slot]);
            assert((uint32_t)(current[slot] >> 32) == key);
        }
        if (retired[slot]) assert(ge_mp_registry_resolve(&registry, retired[slot], NULL) == GE_MP_STATE_STALE);
    }
    entries[0].live = 0; entries[0].generation = UINT32_MAX;
    assert(ge_mp_registry_acquire(&registry, entries[0].key, &id) == GE_MP_STATE_EXHAUSTED);
    assert(id == UINT64_MAX && entries[0].generation == UINT32_MAX && !entries[0].live);
    entries[1].key = entries[0].key;
    assert(ge_mp_registry_acquire(&registry, entries[0].key, &id) == GE_MP_STATE_INVALID);
    assert(ge_mp_registry_init(&registry, NULL, 0) == GE_MP_STATE_OK);
    assert(ge_mp_registry_define(&registry, ge_mp_registry_key(1, 0, 0)) == GE_MP_STATE_CAPACITY);
    assert(ge_mp_registry_init(&registry, entries, GE_MP_STATE_MAX_RECORDS + 1U) == GE_MP_STATE_CAPACITY);
}

static void test_coverage_and_references(void)
{
    Fixture f;
    GeMpCoverageReport report;
    uint8_t digest[32];
    uint64_t next;
    unsigned i;
    fixture_init(&f, 0);
    assert(ge_mp_state_sha256(&f.snapshot, digest, &report) == GE_MP_STATE_OK);
    assert(report.record_index == SIZE_MAX && report.field_index == SIZE_MAX);
    f.schema.complete = 0; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    f.schema.complete = 1;
    for (i = 0; i < 64; ++i) {
        uint64_t bit = UINT64_C(1) << i;
        f.snapshot.enabled_systems |= bit;
        if (!(f.schema.registered_systems & bit)) expect_failure(&f, GE_MP_STATE_INCOMPLETE);
        f.snapshot.enabled_systems = f.schema.registered_systems;
    }
    f.snapshot.coverage.enumerated_systems &= ~GE_MP_SYSTEM_RNG;
    expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    assert(ge_mp_state_sha256(&f.snapshot, digest, &report) == GE_MP_STATE_INCOMPLETE);
    assert(report.missing_systems == GE_MP_SYSTEM_RNG);
    f.snapshot.coverage.enumerated_systems = f.schema.registered_systems;
    f.snapshot.coverage.unclassified_fields = 1; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    f.snapshot.coverage.unclassified_fields = 0;
    f.snapshot.coverage.unresolved_references = 1; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    f.snapshot.coverage.unresolved_references = 0;
    f.record_schema[0].systems = GE_MP_SYSTEM_SESSION; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    f.records[2].field_count = 1; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    f.globals[15].id = 999; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    f.snapshot.record_count = 2; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    assert(ge_mp_registry_acquire(&f.registry, f.keys[2], &next) == GE_MP_STATE_OK);
    expect_failure(&f, GE_MP_STATE_INCOMPLETE); /* live actor omitted even when unreferenced */
    fixture_init(&f, 0);
    f.ref = ((uint64_t)ge_mp_registry_key(1, 3, 1) << 32) | 1U;
    expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    f.ref = f.ids[1] + 1U; expect_failure(&f, GE_MP_STATE_STALE);
    f.ref = 0; f.slot = 0;
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK);
    f.global_schema[14].flags = 0; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.slot = ge_mp_registry_key(1, 0, 99); expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    assert(ge_mp_registry_release(&f.registry, f.ids[1]) == GE_MP_STATE_OK);
    expect_failure(&f, GE_MP_STATE_STALE);
    assert(ge_mp_registry_acquire(&f.registry, f.keys[1], &next) == GE_MP_STATE_OK);
    f.records[2].actor_id = next; expect_failure(&f, GE_MP_STATE_STALE); /* old forward links */
    fixture_init(&f, 0);
    f.records[2].actor_id = f.records[1].actor_id; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.records[1].kind = 2; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.registry.entries[1].key = f.registry.entries[0].key; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.global_schema[0].flags = GE_MP_FIELD_UNIQUE;
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK);
    f.active[1] = f.active[0]; expect_failure(&f, GE_MP_STATE_INVALID);
    f.global_schema[0].flags = 0; /* repetition may be meaningful outside membership lists */
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK);
    fixture_init(&f, 0);
    f.global_schema[1].flags = GE_MP_FIELD_UNIQUE;
    f.free_order[1] = f.free_order[0]; expect_failure(&f, GE_MP_STATE_INVALID);
    for (i = 0; i < 3; ++i) {
        fixture_init(&f, 0);
        memset(i == 0 ? f.snapshot.build_sha256 : i == 1 ? f.snapshot.content_sha256
            : f.snapshot.rules_sha256, 0, 32);
        expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    }
}

static void test_limits(void)
{
    Fixture f;
    uint8_t *values = calloc(GE_MP_STATE_MAX_VALUES, 1);
    GeMpRegistryEntry *entries = calloc(GE_MP_STATE_MAX_RECORDS, sizeof(*entries));
    GeMpStateRecord *records = calloc(GE_MP_STATE_MAX_RECORDS, sizeof(*records));
    GeMpCoverageReport report;
    size_t size, overhead;
    unsigned i;
    assert(values && entries && records);
    fixture_init(&f, 0);
    for (i = 0; i < 16; ++i) {
        field(&f.global_schema[i], &f.globals[i], (uint16_t)(256U + i), GE_MP_STATE_U8, 0, values);
        f.global_schema[i].max_count = GE_MP_STATE_MAX_VALUES;
    }
    assert(ge_mp_state_measure(&f.snapshot, &overhead, NULL) == GE_MP_STATE_OK);
    for (i = 0; i < 16; ++i) f.globals[i].count = GE_MP_STATE_MAX_VALUES;
    expect_failure(&f, GE_MP_STATE_CAPACITY); /* payload fits; framing pushes past 16 MiB */
    f.globals[15].count -= (uint32_t)overhead;
    assert(ge_mp_state_measure(&f.snapshot, &size, &report) == GE_MP_STATE_OK);
    assert(size == GE_MP_STATE_MAX_BYTES && report.encoded_size == size);
    ++f.globals[15].count; expect_failure(&f, GE_MP_STATE_CAPACITY);
    f.globals[0].count = GE_MP_STATE_MAX_VALUES + 1U; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.record_schema[0].field_count = f.record_schema[1].field_count = 0;
    f.registry = (GeMpRegistry){entries, GE_MP_STATE_MAX_RECORDS - 1U, GE_MP_STATE_MAX_RECORDS};
    f.snapshot.records = records; f.snapshot.record_count = GE_MP_STATE_MAX_RECORDS;
    records[0] = (GeMpStateRecord){0, 0, NULL, 0};
    for (i = 0; i < GE_MP_STATE_MAX_RECORDS - 1U; ++i) {
        entries[i] = (GeMpRegistryEntry){ge_mp_registry_key(1, 255, (uint16_t)i), 1, 1};
        records[i + 1U] = (GeMpStateRecord){((uint64_t)entries[i].key << 32) | 1U, 1, NULL, 0};
    }
    assert(ge_mp_state_measure(&f.snapshot, &size, NULL) == GE_MP_STATE_OK);
    entries[GE_MP_STATE_MAX_RECORDS - 1U] = (GeMpRegistryEntry){ge_mp_registry_key(1, 255,
        GE_MP_STATE_MAX_RECORDS - 1U), 1, 1};
    ++f.registry.count; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    free(values); free(entries); free(records);
}

static void test_values_and_atomicity(void)
{
    Fixture f;
    uint8_t base[32], digest[32], output[2048], before[2048], unaligned[9];
    uint32_t nonfinite[] = {0x7f800000U, 0xff800000U, 0x7fc00000U, 0x7f800001U, 0xffffffffU};
    size_t required, written = 765;
    unsigned i;
    fixture_init(&f, 0);
    assert(ge_mp_state_measure(&f.snapshot, &required, NULL) == GE_MP_STATE_OK);
    memset(output, 0x82, sizeof(output)); memcpy(before, output, sizeof(output));
    assert(ge_mp_state_encode(&f.snapshot, output, required - 1U, &written, NULL) == GE_MP_STATE_CAPACITY);
    assert(written == 765 && memcmp(output, before, sizeof(output)) == 0);
    assert(ge_mp_state_encode(&f.snapshot, output, required, &written, NULL) == GE_MP_STATE_OK && written == required);
    assert(output[required] == 0x82);
    for (i = 0; i < sizeof(nonfinite) / sizeof(nonfinite[0]); ++i) {
        memcpy(&f.f32, &nonfinite[i], 4); expect_failure(&f, GE_MP_STATE_INVALID);
    }
    f.f32 = 0.0f;
    assert(ge_mp_state_sha256(&f.snapshot, base, NULL) == GE_MP_STATE_OK);
    { uint32_t negative_zero = 0x80000000U; memcpy(&f.f32, &negative_zero, 4); }
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK && memcmp(base, digest, 32));
    { uint32_t subnormal = 1U; memcpy(&f.f32, &subnormal, 4); }
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK);
    f.boolean = 2; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    assert(ge_mp_state_sha256(&f.snapshot, base, NULL) == GE_MP_STATE_OK);
    memcpy(unaligned + 1, &f.u64, 8); f.globals[7].values = unaligned + 1;
    assert(ge_mp_state_sha256(&f.snapshot, digest, NULL) == GE_MP_STATE_OK && !memcmp(base, digest, 32));
    f.globals[7].values = NULL; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.globals[7].type = GE_MP_STATE_I64; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.globals[7].count = UINT32_MAX; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.global_schema[7].max_count = UINT32_MAX; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.global_schema[4].flags = GE_MP_FIELD_NULLABLE; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.globals[7].id = f.globals[6].id; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.global_schema[7].id = f.global_schema[6].id; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.record_schema[1].kind = 0; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.snapshot.record_count = GE_MP_STATE_MAX_RECORDS + 1U; expect_failure(&f, GE_MP_STATE_CAPACITY);
    fixture_init(&f, 0);
    f.registry.count = 5; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.snapshot.session = 0; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.schema.version = 0; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.record_schema[0].field_count = GE_MP_STATE_MAX_FIELDS + 1U; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.schema.record_count = 257; expect_failure(&f, GE_MP_STATE_INVALID);
    fixture_init(&f, 0);
    f.schema.record_count = 1; expect_failure(&f, GE_MP_STATE_INCOMPLETE);
    fixture_init(&f, 0);
    f.record_schema[0].systems |= GE_MP_SYSTEM_PLAYERS;
    f.schema.record_count = 1; expect_failure(&f, GE_MP_STATE_INCOMPLETE); /* undeclared actor kind */
}

static void test_canonical(const char *path)
{
    Fixture a, b;
    uint8_t bytes[2048], other[2048], digest[32], plain[32];
    size_t size, other_size;
    unsigned i;
    fixture_init(&a, 0x19); fixture_init(&b, 0xe7);
    assert(ge_mp_state_encode(&a.snapshot, bytes, sizeof(bytes), &size, NULL) == GE_MP_STATE_OK);
    assert(ge_mp_state_encode(&b.snapshot, other, sizeof(other), &other_size, NULL) == GE_MP_STATE_OK);
    assert(size == other_size && !memcmp(bytes, other, size)); /* addresses and poisoned padding excluded */
    assert(ge_mp_state_sha256(&a.snapshot, digest, NULL) == GE_MP_STATE_OK);
    assert(ge_mp_sha256(bytes, size, plain) == GE_MP_STATE_OK && !memcmp(digest, plain, 32));
    if (path != NULL) {
        FILE *file = fopen(path, "wb");
        assert(file != NULL && fwrite(bytes, 1, size, file) == size);
        assert(fwrite(digest, 1, 32, file) == 32 && fclose(file) == 0);
    }
    for (i = 0; i < 4; ++i) {
        fixture_init(&b, 0);
        if (i == 1) {
            uint32_t temp = b.free_order[0]; b.free_order[0] = b.free_order[1]; b.free_order[1] = temp;
        } else {
            uint64_t *order = i == 0 ? b.active : i == 2 ? b.room : b.child;
            uint64_t temp = order[0]; order[0] = order[1]; order[1] = temp;
        }
        assert(ge_mp_state_sha256(&b.snapshot, plain, NULL) == GE_MP_STATE_OK && memcmp(digest, plain, 32));
    }
    fixture_init(&b, 0);
    b.owners[0] = 3; /* transfer ownership without changing immutable creation namespace */
    assert(b.records[1].actor_id == a.records[1].actor_id);
    assert(ge_mp_state_sha256(&b.snapshot, plain, NULL) == GE_MP_STATE_OK && memcmp(digest, plain, 32));
    fixture_init(&b, 0);
    { uint64_t temporary;
      assert(ge_mp_registry_acquire(&b.registry, b.keys[2], &temporary) == GE_MP_STATE_OK);
      assert(ge_mp_registry_release(&b.registry, temporary) == GE_MP_STATE_OK); }
    assert(ge_mp_state_sha256(&b.snapshot, plain, NULL) == GE_MP_STATE_OK && memcmp(digest, plain, 32));
    for (i = 0; i < 5; ++i) {
        fixture_init(&b, 0);
        if (i == 0) ++b.snapshot.tick;
        if (i == 1) ++b.snapshot.session;
        if (i == 2) ++b.snapshot.build_sha256[0];
        if (i == 3) ++b.snapshot.rules_sha256[0];
        if (i == 4) ++b.schema.version;
        assert(ge_mp_state_sha256(&b.snapshot, plain, NULL) == GE_MP_STATE_OK && memcmp(digest, plain, 32));
    }
}

int main(int argc, char **argv)
{
    test_sha(); test_registry(); test_coverage_and_references();
    test_values_and_atomicity(); test_limits(); test_canonical(argc > 1 ? argv[1] : NULL);
    puts("mp state: SHA-256 vectors, canonical types/order, coverage/ref rejection and 20,000 registry transitions passed");
    return 0;
}
