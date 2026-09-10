#include "ge_mp_state.h"

#include <float.h>
#include <limits.h>
#include <string.h>

_Static_assert(CHAR_BIT == 8 && sizeof(size_t) >= 4 && sizeof(float) == 4 && FLT_RADIX == 2 &&
    FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128 && FLT_MIN_EXP == -125,
    "The canonical codec requires IEEE-754 binary32 and octet bytes");

/* SHA-256 compression adapted from this port's ge_original_model62.c.
 * FIPS 180-4 constants/operations; streaming finalization shares that algorithm. */
typedef struct GeSha256 {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t block[64];
    size_t used;
} GeSha256;

static uint32_t rotr32(uint32_t value, unsigned shift)
{
    return (value >> shift) | (value << (32U - shift));
}

static uint32_t read_be32(const uint8_t *source)
{
    return ((uint32_t)source[0] << 24) | ((uint32_t)source[1] << 16)
        | ((uint32_t)source[2] << 8) | (uint32_t)source[3];
}

static void sha256_transform(GeSha256 *hash, const uint8_t block[64])
{
    static const uint32_t constants[64] = {
        0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,
        0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
        0xd807aa98U,0x12835b01U,0x243185beU,0x550c7dc3U,
        0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
        0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,
        0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
        0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,
        0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
        0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,
        0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
        0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,
        0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
        0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,
        0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
        0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,
        0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
    };
    uint32_t words[64];
    uint32_t a, b, c, d, e, f, g, h;
    size_t index;

    for (index = 0; index < 16U; ++index)
        words[index] = read_be32(block + index * 4U);
    for (; index < 64U; ++index) {
        uint32_t s0 = rotr32(words[index - 15U], 7U)
            ^ rotr32(words[index - 15U], 18U) ^ (words[index - 15U] >> 3);
        uint32_t s1 = rotr32(words[index - 2U], 17U)
            ^ rotr32(words[index - 2U], 19U) ^ (words[index - 2U] >> 10);
        words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }
    a=hash->state[0]; b=hash->state[1]; c=hash->state[2]; d=hash->state[3];
    e=hash->state[4]; f=hash->state[5]; g=hash->state[6]; h=hash->state[7];
    for (index = 0; index < 64U; ++index) {
        uint32_t sum1 = rotr32(e,6U)^rotr32(e,11U)^rotr32(e,25U);
        uint32_t choice = (e & f) ^ (~e & g);
        uint32_t temp1 = h + sum1 + choice + constants[index] + words[index];
        uint32_t sum0 = rotr32(a,2U)^rotr32(a,13U)^rotr32(a,22U);
        uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = sum0 + majority;
        h=g; g=f; f=e; e=d+temp1; d=c; c=b; b=a; a=temp1+temp2;
    }
    hash->state[0]+=a; hash->state[1]+=b; hash->state[2]+=c;
    hash->state[3]+=d; hash->state[4]+=e; hash->state[5]+=f;
    hash->state[6]+=g; hash->state[7]+=h;
}

static void sha256_update(GeSha256 *hash, const uint8_t *data, size_t size)
{
    while (size != 0U) {
        size_t available = 64U - hash->used;
        size_t count = size < available ? size : available;
        memcpy(hash->block + hash->used, data, count);
        hash->used += count;
        hash->bit_count += (uint64_t)count * 8U;
        data += count;
        size -= count;
        if (hash->used == 64U) {
            sha256_transform(hash, hash->block);
            hash->used = 0U;
        }
    }
}

static GeSha256 sha256_init(void)
{
    const GeSha256 hash = {{0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,
        0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U},0,{0},0};
    return hash;
}

static void sha256_final(GeSha256 *hash, uint8_t result[32])
{
    uint64_t bits = hash->bit_count;
    size_t index;
    hash->block[hash->used++] = 0x80U;
    if (hash->used > 56U) {
        memset(hash->block + hash->used, 0, 64U - hash->used);
        sha256_transform(hash, hash->block);
        hash->used = 0U;
    }
    memset(hash->block + hash->used, 0, 56U - hash->used);
    for (index = 0U; index < 8U; ++index)
        hash->block[63U - index] = (uint8_t)(bits >> (index * 8U));
    sha256_transform(hash, hash->block);
    for (index = 0U; index < 8U; ++index) {
        result[index*4U]=(uint8_t)(hash->state[index]>>24);
        result[index*4U+1U]=(uint8_t)(hash->state[index]>>16);
        result[index*4U+2U]=(uint8_t)(hash->state[index]>>8);
        result[index*4U+3U]=(uint8_t)hash->state[index];
    }
}

GeMpStateStatus ge_mp_sha256(const void *bytes, size_t length, uint8_t digest[32])
{
    GeSha256 hash = sha256_init();
    if (digest == NULL || (length != 0U && bytes == NULL))
        return GE_MP_STATE_INVALID;
#if SIZE_MAX > UINT64_MAX / 8U
    if (length > UINT64_MAX / 8U) return GE_MP_STATE_CAPACITY;
#endif
    sha256_update(&hash, bytes, length);
    sha256_final(&hash, digest);
    return GE_MP_STATE_OK;
}

static int valid_key(uint32_t key) { return (key >> 24) != 0U; }

static GeMpStateStatus registry_valid(const GeMpRegistry *registry)
{
    size_t i;
    if (registry == NULL || registry->count > registry->capacity ||
        registry->capacity > GE_MP_STATE_MAX_RECORDS ||
        (registry->capacity != 0U && registry->entries == NULL))
        return GE_MP_STATE_INVALID;
    for (i = 0; i < registry->count; ++i) {
        const GeMpRegistryEntry *entry = &registry->entries[i];
        if (!valid_key(entry->key) || entry->live > 1U ||
            (entry->live && entry->generation == 0U) ||
            (i != 0U && registry->entries[i - 1U].key >= entry->key))
            return GE_MP_STATE_INVALID;
    }
    return GE_MP_STATE_OK;
}

static size_t registry_find(const GeMpRegistry *registry, uint32_t key)
{
    size_t lo = 0, hi = registry->count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2U;
        if (registry->entries[mid].key < key) lo = mid + 1U;
        else hi = mid;
    }
    return lo;
}

static GeMpStateStatus resolve_valid(const GeMpRegistry *registry,
    uint64_t id, size_t *index)
{
    uint32_t key = (uint32_t)(id >> 32), generation = (uint32_t)id;
    size_t at;
    if (!valid_key(key) || generation == 0U) return GE_MP_STATE_INVALID;
    at = registry_find(registry, key);
    if (at == registry->count || registry->entries[at].key != key)
        return GE_MP_STATE_INCOMPLETE;
    if (!registry->entries[at].live || registry->entries[at].generation != generation)
        return GE_MP_STATE_STALE;
    if (index != NULL) *index = at;
    return GE_MP_STATE_OK;
}

uint32_t ge_mp_registry_key(uint8_t kind, uint8_t creation_namespace, uint16_t slot)
{
    return ((uint32_t)kind << 24) | ((uint32_t)creation_namespace << 16) | slot;
}

GeMpStateStatus ge_mp_registry_init(GeMpRegistry *registry,
    GeMpRegistryEntry *storage, size_t capacity)
{
    if (registry == NULL || (capacity != 0U && storage == NULL))
        return GE_MP_STATE_INVALID;
    if (capacity > GE_MP_STATE_MAX_RECORDS) return GE_MP_STATE_CAPACITY;
    registry->entries = storage;
    registry->count = 0;
    registry->capacity = capacity;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_registry_define(GeMpRegistry *registry, uint32_t key)
{
    size_t at;
    if (registry_valid(registry) != GE_MP_STATE_OK || !valid_key(key))
        return GE_MP_STATE_INVALID;
    at = registry_find(registry, key);
    if (at != registry->count && registry->entries[at].key == key)
        return GE_MP_STATE_INVALID;
    if (registry->count == registry->capacity) return GE_MP_STATE_CAPACITY;
    memmove(&registry->entries[at + 1U], &registry->entries[at],
        (registry->count - at) * sizeof(*registry->entries));
    registry->entries[at].key = key;
    registry->entries[at].generation = 0;
    registry->entries[at].live = 0;
    ++registry->count;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_registry_acquire(GeMpRegistry *registry, uint32_t key,
    uint64_t *actor_id)
{
    size_t at;
    GeMpRegistryEntry *entry;
    if (actor_id == NULL || registry_valid(registry) != GE_MP_STATE_OK || !valid_key(key))
        return GE_MP_STATE_INVALID;
    at = registry_find(registry, key);
    if (at == registry->count || registry->entries[at].key != key)
        return GE_MP_STATE_INCOMPLETE;
    entry = &registry->entries[at];
    if (entry->live) return GE_MP_STATE_INVALID;
    if (entry->generation == UINT32_MAX) return GE_MP_STATE_EXHAUSTED;
    ++entry->generation;
    entry->live = 1;
    *actor_id = ((uint64_t)key << 32) | entry->generation;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_registry_resolve(const GeMpRegistry *registry,
    uint64_t actor_id, size_t *entry_index)
{
    if (registry_valid(registry) != GE_MP_STATE_OK) return GE_MP_STATE_INVALID;
    return resolve_valid(registry, actor_id, entry_index);
}

GeMpStateStatus ge_mp_registry_release(GeMpRegistry *registry, uint64_t actor_id)
{
    size_t at;
    GeMpStateStatus status = ge_mp_registry_resolve(registry, actor_id, &at);
    if (status == GE_MP_STATE_OK) registry->entries[at].live = 0;
    return status;
}

static size_t type_width(uint8_t type)
{
    switch (type) {
    case GE_MP_STATE_U8: case GE_MP_STATE_I8: case GE_MP_STATE_BOOL: return 1;
    case GE_MP_STATE_U16: case GE_MP_STATE_I16: return 2;
    case GE_MP_STATE_U32: case GE_MP_STATE_I32: case GE_MP_STATE_F32:
    case GE_MP_STATE_SLOT_REF: return 4;
    case GE_MP_STATE_U64: case GE_MP_STATE_I64: case GE_MP_STATE_ACTOR_REF: return 8;
    default: return 0;
    }
}

/* memcpy permits unaligned arrays and never reads native struct padding.
 * Signed values are converted numerically to the canonical two's complement. */
static uint64_t value_bits(uint8_t type, const uint8_t *source)
{
    switch (type) {
    case GE_MP_STATE_I8: { int8_t x; memcpy(&x, source, 1); return (uint8_t)x; }
    case GE_MP_STATE_I16: { int16_t x; memcpy(&x, source, 2); return (uint16_t)x; }
    case GE_MP_STATE_I32: { int32_t x; memcpy(&x, source, 4); return (uint32_t)x; }
    case GE_MP_STATE_I64: { int64_t x; memcpy(&x, source, 8); return (uint64_t)x; }
    default:
        switch (type_width(type)) {
        case 1: return *source;
        case 2: { uint16_t x; memcpy(&x, source, 2); return x; }
        case 4: { uint32_t x; memcpy(&x, source, 4); return x; }
        default: { uint64_t x; memcpy(&x, source, 8); return x; }
        }
    }
}

static const GeMpRecordSchema *record_schema(const GeMpStateSchema *schema, uint8_t kind)
{
    size_t lo = 0, hi = schema->record_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2U;
        if (schema->records[mid].kind < kind) lo = mid + 1U;
        else hi = mid;
    }
    return lo < schema->record_count && schema->records[lo].kind == kind
        ? &schema->records[lo] : NULL;
}

static int record_present(const GeMpCanonicalSnapshot *snapshot, uint64_t id)
{
    size_t lo = 0, hi = snapshot->record_count;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2U;
        if (snapshot->records[mid].actor_id < id) lo = mid + 1U;
        else hi = mid;
    }
    return lo < snapshot->record_count && snapshot->records[lo].actor_id == id;
}

static void reset_report(GeMpCoverageReport *report)
{
    memset(report, 0, sizeof(*report));
    report->record_index = report->field_index = SIZE_MAX;
}

static int identity_present(const uint8_t identity[32])
{
    uint8_t bits = 0;
    size_t i;
    for (i = 0; i < 32; ++i) bits |= identity[i];
    return bits != 0;
}

static GeMpStateStatus validate(const GeMpCanonicalSnapshot *snapshot,
    GeMpCoverageReport *report)
{
    const GeMpStateSchema *schema;
    uint64_t declared_systems = 0;
    uint8_t seen[GE_MP_STATE_MAX_RECORDS];
    size_t i, j, n, bytes = 0;
    if (snapshot == NULL || snapshot->session == 0U || snapshot->schema == NULL ||
        (snapshot->enabled_systems & GE_MP_SYSTEM_SESSION) == 0U ||
        registry_valid(snapshot->registry) != GE_MP_STATE_OK)
        return GE_MP_STATE_INVALID;
    schema = snapshot->schema;
    if (schema->version == 0U || schema->complete > 1U || schema->record_count == 0U ||
        schema->record_count > 256U || schema->records == NULL || schema->records[0].kind != 0U)
        return GE_MP_STATE_INVALID;
    report->missing_systems = snapshot->enabled_systems &
        ~(schema->registered_systems & snapshot->coverage.enumerated_systems);
    report->unclassified_fields = snapshot->coverage.unclassified_fields;
    report->unresolved_references = snapshot->coverage.unresolved_references;
    if (!schema->complete || report->missing_systems || report->unclassified_fields ||
        report->unresolved_references) return GE_MP_STATE_INCOMPLETE;
    if (!identity_present(snapshot->build_sha256) || !identity_present(snapshot->content_sha256) ||
        !identity_present(snapshot->rules_sha256)) return GE_MP_STATE_INCOMPLETE;
    for (i = 0; i < schema->record_count; ++i) {
        const GeMpRecordSchema *rs = &schema->records[i];
        if ((i && schema->records[i - 1U].kind >= rs->kind) || !rs->systems ||
            (rs->systems & ~schema->registered_systems) || rs->field_count > GE_MP_STATE_MAX_FIELDS ||
            (rs->field_count && rs->fields == NULL)) return GE_MP_STATE_INVALID;
        declared_systems |= rs->systems;
        for (j = 0; j < rs->field_count; ++j) {
            const GeMpFieldSchema *fs = &rs->fields[j];
            if (!fs->id || (j && rs->fields[j - 1U].id >= fs->id) || !type_width(fs->type) ||
                fs->min_count > fs->max_count || fs->max_count > GE_MP_STATE_MAX_VALUES ||
                (fs->flags & ~(GE_MP_FIELD_NULLABLE | GE_MP_FIELD_UNIQUE)) || (fs->flags &&
                fs->type != GE_MP_STATE_ACTOR_REF && fs->type != GE_MP_STATE_SLOT_REF))
                return GE_MP_STATE_INVALID;
        }
    }
    report->missing_systems = snapshot->enabled_systems & ~declared_systems;
    if (report->missing_systems) return GE_MP_STATE_INCOMPLETE;
    /* Free slots retain canonical lifetime history and can be SLOT_REF
     * targets. Their kind needs coverage even before an actor is acquired. */
    for (i = 0; i < snapshot->registry->count; ++i) {
        uint8_t kind = (uint8_t)(snapshot->registry->entries[i].key >> 24);
        if (record_schema(schema, kind) == NULL) {
            ++report->unclassified_fields;
            return GE_MP_STATE_INCOMPLETE;
        }
    }
    if (!snapshot->record_count) return GE_MP_STATE_INCOMPLETE;
    if (snapshot->record_count > GE_MP_STATE_MAX_RECORDS) return GE_MP_STATE_CAPACITY;
    if (snapshot->records == NULL) return GE_MP_STATE_INVALID;
    if (snapshot->records[0].actor_id != 0U) return GE_MP_STATE_INCOMPLETE;
    /* Pass one: shape, ranges and scalar values. Only then follow references. */
    for (i = 0; i < snapshot->record_count; ++i) {
        const GeMpStateRecord *record = &snapshot->records[i];
        const GeMpRecordSchema *rs = record_schema(schema, record->kind);
        report->record_index = i;
        report->field_index = SIZE_MAX;
        if ((i && snapshot->records[i - 1U].actor_id >= record->actor_id) ||
            record->kind != (uint8_t)(record->actor_id >> 56)) return GE_MP_STATE_INVALID;
        if (rs == NULL || record->field_count != rs->field_count) {
            ++report->unclassified_fields;
            return GE_MP_STATE_INCOMPLETE;
        }
        if (i) {
            GeMpStateStatus status = resolve_valid(snapshot->registry, record->actor_id, NULL);
            if (status != GE_MP_STATE_OK) {
                ++report->unresolved_references;
                return status;
            }
        }
        if (record->field_count && record->fields == NULL) return GE_MP_STATE_INVALID;
        for (j = 0; j < record->field_count; ++j) {
            const GeMpStateField *field = &record->fields[j];
            const GeMpFieldSchema *fs = &rs->fields[j];
            size_t width = type_width(field->type);
            report->field_index = j;
            if (j && record->fields[j - 1U].id >= field->id) return GE_MP_STATE_INVALID;
            if (field->id != fs->id) {
                ++report->unclassified_fields;
                return GE_MP_STATE_INCOMPLETE;
            }
            if (field->type != fs->type || field->count < fs->min_count ||
                field->count > fs->max_count || (field->count && field->values == NULL))
                return GE_MP_STATE_INVALID;
            if (field->count > (GE_MP_STATE_MAX_BYTES - bytes) / width)
                return GE_MP_STATE_CAPACITY;
            bytes += (size_t)field->count * width;
            if (field->type == GE_MP_STATE_F32 || field->type == GE_MP_STATE_BOOL) {
                for (n = 0; n < field->count; ++n) {
                    uint64_t bits = value_bits(field->type, (const uint8_t *)field->values + n * width);
                    if ((field->type == GE_MP_STATE_BOOL && bits > 1U) ||
                        (field->type == GE_MP_STATE_F32 && (bits & 0x7f800000U) == 0x7f800000U))
                        return GE_MP_STATE_INVALID;
                }
            }
        }
    }
    for (i = 0; i < snapshot->record_count; ++i) {
        const GeMpStateRecord *record = &snapshot->records[i];
        const GeMpRecordSchema *rs = record_schema(schema, record->kind);
        report->record_index = i;
        report->field_index = SIZE_MAX;
        for (j = 0; j < record->field_count; ++j) {
            const GeMpStateField *field = &record->fields[j];
            size_t width = type_width(field->type);
            if (field->type != GE_MP_STATE_ACTOR_REF && field->type != GE_MP_STATE_SLOT_REF) continue;
            report->field_index = j;
            if (rs->fields[j].flags & GE_MP_FIELD_UNIQUE) memset(seen, 0, sizeof(seen));
            for (n = 0; n < field->count; ++n) {
                uint64_t id = value_bits(field->type, (const uint8_t *)field->values + n * width);
                GeMpStateStatus status;
                size_t at;
                if (!id && (rs->fields[j].flags & GE_MP_FIELD_NULLABLE)) continue;
                if (field->type == GE_MP_STATE_ACTOR_REF) {
                    status = resolve_valid(snapshot->registry, id, &at);
                    if (status == GE_MP_STATE_OK && !record_present(snapshot, id))
                        status = GE_MP_STATE_INCOMPLETE;
                } else {
                    at = registry_find(snapshot->registry, (uint32_t)id);
                    status = !valid_key((uint32_t)id) ? GE_MP_STATE_INVALID :
                        at == snapshot->registry->count || snapshot->registry->entries[at].key != id
                        ? GE_MP_STATE_INCOMPLETE : GE_MP_STATE_OK;
                }
                if (status != GE_MP_STATE_OK) {
                    ++report->unresolved_references;
                    return status;
                }
                if (rs->fields[j].flags & GE_MP_FIELD_UNIQUE) {
                    if (seen[at]) return GE_MP_STATE_INVALID;
                    seen[at] = 1;
                }
            }
        }
    }
    report->record_index = report->field_index = SIZE_MAX;
    for (i = 0; i < snapshot->registry->count; ++i) {
        const GeMpRegistryEntry *entry = &snapshot->registry->entries[i];
        uint64_t id = ((uint64_t)entry->key << 32) | entry->generation;
        if (entry->live && !record_present(snapshot, id)) {
            ++report->unresolved_references;
            return GE_MP_STATE_INCOMPLETE;
        }
    }
    return GE_MP_STATE_OK;
}

typedef struct GeMpSink {
    uint8_t *data;
    GeSha256 *hash;
    size_t used;
} GeMpSink;

static void emit(GeMpSink *sink, const uint8_t *bytes, size_t count)
{
    if (sink->data != NULL) memcpy(sink->data + sink->used, bytes, count);
    if (sink->hash != NULL) sha256_update(sink->hash, bytes, count);
    sink->used += count;
}

static void emit_number(GeMpSink *sink, uint64_t value, size_t width)
{
    uint8_t bytes[8];
    size_t i;
    for (i = 0; i < width; ++i) bytes[width - 1U - i] = (uint8_t)(value >> (i * 8U));
    emit(sink, bytes, width);
}

/* The validated maximum shape fits size_t on all supported (32+ bit) hosts.
 * Measurement omits value reads; emission streams without heap allocation. */
static void encode_valid(const GeMpCanonicalSnapshot *snapshot, GeMpSink *sink)
{
    const GeMpStateSchema *schema = snapshot->schema;
    size_t i, j, n;
    emit(sink, (const uint8_t *)"GEMS", 4);
    emit_number(sink, GE_MP_STATE_WIRE_VERSION, 2);
    emit_number(sink, schema->version, 4);
    emit_number(sink, snapshot->session, 4);
    emit_number(sink, snapshot->tick, 8);
    emit_number(sink, snapshot->enabled_systems, 8);
    emit(sink, snapshot->build_sha256, 32);
    emit(sink, snapshot->content_sha256, 32);
    emit(sink, snapshot->rules_sha256, 32);
    emit_number(sink, schema->registered_systems, 8);
    emit_number(sink, schema->record_count, 4);
    for (i = 0; i < schema->record_count; ++i) {
        const GeMpRecordSchema *rs = &schema->records[i];
        emit_number(sink, rs->kind, 1);
        emit_number(sink, rs->systems, 8);
        emit_number(sink, rs->field_count, 4);
        for (j = 0; j < rs->field_count; ++j) {
            const GeMpFieldSchema *fs = &rs->fields[j];
            emit_number(sink, fs->id, 2);
            emit_number(sink, fs->type, 1);
            emit_number(sink, fs->flags, 1);
            emit_number(sink, fs->min_count, 4);
            emit_number(sink, fs->max_count, 4);
        }
    }
    emit_number(sink, snapshot->registry->count, 4);
    for (i = 0; i < snapshot->registry->count; ++i) {
        const GeMpRegistryEntry *entry = &snapshot->registry->entries[i];
        emit_number(sink, entry->key, 4);
        emit_number(sink, entry->generation, 4);
        emit_number(sink, entry->live, 1);
    }
    emit_number(sink, snapshot->record_count, 4);
    for (i = 0; i < snapshot->record_count; ++i) {
        const GeMpStateRecord *record = &snapshot->records[i];
        emit_number(sink, record->actor_id, 8);
        emit_number(sink, record->kind, 1);
        emit_number(sink, record->field_count, 4);
        for (j = 0; j < record->field_count; ++j) {
            const GeMpStateField *field = &record->fields[j];
            size_t width = type_width(field->type);
            emit_number(sink, field->id, 2);
            emit_number(sink, field->type, 1);
            emit_number(sink, field->count, 4);
            if (sink->data == NULL && sink->hash == NULL) sink->used += width * field->count;
            else for (n = 0; n < field->count; ++n)
                emit_number(sink, value_bits(field->type,
                    (const uint8_t *)field->values + n * width), width);
        }
    }
}

GeMpStateStatus ge_mp_state_measure(const GeMpCanonicalSnapshot *snapshot,
    size_t *required_bytes, GeMpCoverageReport *report)
{
    GeMpCoverageReport local;
    GeMpSink sink = {NULL, NULL, 0};
    GeMpStateStatus status;
    if (report == NULL) report = &local;
    reset_report(report);
    if (required_bytes == NULL) return GE_MP_STATE_INVALID;
    status = validate(snapshot, report);
    if (status != GE_MP_STATE_OK) return status;
    encode_valid(snapshot, &sink);
    if (sink.used > GE_MP_STATE_MAX_BYTES) return GE_MP_STATE_CAPACITY;
    report->encoded_size = *required_bytes = sink.used;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_state_encode(const GeMpCanonicalSnapshot *snapshot,
    uint8_t *destination, size_t capacity, size_t *written, GeMpCoverageReport *report)
{
    size_t required;
    GeMpSink sink = {destination, NULL, 0};
    GeMpStateStatus status;
    if (destination == NULL || written == NULL) {
        if (report != NULL) reset_report(report);
        return GE_MP_STATE_INVALID;
    }
    status = ge_mp_state_measure(snapshot, &required, report);
    if (status != GE_MP_STATE_OK) return status;
    if (capacity < required) return GE_MP_STATE_CAPACITY;
    encode_valid(snapshot, &sink);
    *written = sink.used;
    return GE_MP_STATE_OK;
}

GeMpStateStatus ge_mp_state_sha256(const GeMpCanonicalSnapshot *snapshot,
    uint8_t digest[32], GeMpCoverageReport *report)
{
    size_t required;
    GeSha256 hash = sha256_init();
    GeMpSink sink = {NULL, &hash, 0};
    GeMpStateStatus status;
    if (digest == NULL) {
        if (report != NULL) reset_report(report);
        return GE_MP_STATE_INVALID;
    }
    status = ge_mp_state_measure(snapshot, &required, report);
    if (status != GE_MP_STATE_OK) return status;
    encode_valid(snapshot, &sink);
    sha256_final(&hash, digest);
    return GE_MP_STATE_OK;
}
