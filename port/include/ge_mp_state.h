#ifndef GE_MP_STATE_H
#define GE_MP_STATE_H

#include <stddef.h>
#include <stdint.h>

#define GE_MP_STATE_WIRE_VERSION 1U
#define GE_MP_STATE_DIGEST_SIZE 32U
#define GE_MP_STATE_MAX_RECORDS 4096U
#define GE_MP_STATE_MAX_FIELDS 1024U
#define GE_MP_STATE_MAX_VALUES 1048576U
#define GE_MP_STATE_MAX_BYTES (16U * 1024U * 1024U)

typedef enum GeMpStateStatus {
    GE_MP_STATE_OK, GE_MP_STATE_INVALID, GE_MP_STATE_INCOMPLETE,
    GE_MP_STATE_CAPACITY, GE_MP_STATE_STALE, GE_MP_STATE_EXHAUSTED
} GeMpStateStatus;

/* Stable creation namespace is immutable; current carrier/killer/team is a
 * separate schema field. Entries remain registered after release, preserving
 * generation history. Entry arrays are sorted by key and read-only to callers. */
typedef struct GeMpRegistryEntry {
    uint32_t key; /* kind:8 namespace:8 slot:16 */
    uint32_t generation; /* initially zero; incremented on every acquire */
    uint8_t live;
} GeMpRegistryEntry;
typedef struct GeMpRegistry {
    GeMpRegistryEntry *entries;
    size_t count, capacity;
} GeMpRegistry;

uint32_t ge_mp_registry_key(uint8_t kind, uint8_t creation_namespace, uint16_t slot);
GeMpStateStatus ge_mp_registry_init(GeMpRegistry *registry,
    GeMpRegistryEntry *storage, size_t capacity);
GeMpStateStatus ge_mp_registry_define(GeMpRegistry *registry, uint32_t key);
GeMpStateStatus ge_mp_registry_acquire(GeMpRegistry *registry, uint32_t key,
    uint64_t *actor_id);
GeMpStateStatus ge_mp_registry_release(GeMpRegistry *registry, uint64_t actor_id);
GeMpStateStatus ge_mp_registry_resolve(const GeMpRegistry *registry,
    uint64_t actor_id, size_t *entry_index);

/* Known domains; further bits require explicit registration in a new schema.
 * A provider must enumerate all enabled systems, even those with zero actors. */
#define GE_MP_SYSTEM_SESSION     (UINT64_C(1) << 0)
#define GE_MP_SYSTEM_PLAYERS     (UINT64_C(1) << 1)
#define GE_MP_SYSTEM_INPUT       (UINT64_C(1) << 2)
#define GE_MP_SYSTEM_GUNS        (UINT64_C(1) << 3)
#define GE_MP_SYSTEM_INVENTORY   (UINT64_C(1) << 4)
#define GE_MP_SYSTEM_PROPS       (UINT64_C(1) << 5)
#define GE_MP_SYSTEM_AI          (UINT64_C(1) << 6)
#define GE_MP_SYSTEM_ANIMATION   (UINT64_C(1) << 7)
#define GE_MP_SYSTEM_OBJECTS     (UINT64_C(1) << 8)
#define GE_MP_SYSTEM_PROJECTILES (UINT64_C(1) << 9)
#define GE_MP_SYSTEM_EXPLOSIONS  (UINT64_C(1) << 10)
#define GE_MP_SYSTEM_RULES       (UINT64_C(1) << 11)
#define GE_MP_SYSTEM_RESPAWN     (UINT64_C(1) << 12)
#define GE_MP_SYSTEM_RNG         (UINT64_C(1) << 13)

typedef enum GeMpStateType {
    GE_MP_STATE_U8 = 1, GE_MP_STATE_U16 = 2, GE_MP_STATE_U32 = 3, GE_MP_STATE_U64 = 4,
    GE_MP_STATE_I8 = 5, GE_MP_STATE_I16 = 6, GE_MP_STATE_I32 = 7, GE_MP_STATE_I64 = 8,
    GE_MP_STATE_F32 = 9, GE_MP_STATE_BOOL = 10,
    GE_MP_STATE_ACTOR_REF = 11, /* uint64_t, live registered actor + present record */
    GE_MP_STATE_SLOT_REF = 12   /* uint32_t registry key, may refer to free slot */
} GeMpStateType;

#define GE_MP_FIELD_NULLABLE 1U /* Only ACTOR_REF/SLOT_REF; zero means null. */
#define GE_MP_FIELD_UNIQUE 2U /* Only refs; reject repeated non-null IDs per field. */
/* Common stable IDs; native fields start at 256. Order fields are ordinary
 * typed sequences: no sorting/deduplication of active/free/room/child lists. */
enum {
    GE_MP_FIELD_ACTIVE_ORDER = 1, GE_MP_FIELD_FREE_ORDER = 2,
    GE_MP_FIELD_ROOM_MEMBER_ORDER = 3, GE_MP_FIELD_CHILD_ORDER = 4,
    GE_MP_FIELD_CURRENT_OWNER = 5, GE_MP_FIELD_NATIVE_BEGIN = 256
};

typedef struct GeMpFieldSchema {
    uint16_t id; /* nonzero, strictly increasing within each kind */
    uint8_t type, flags;
    uint32_t min_count, max_count; /* scalar: 1,1; optional sequence: 0,max */
} GeMpFieldSchema;
typedef struct GeMpRecordSchema {
    uint8_t kind; /* zero is the exactly-one session/global record */
    uint64_t systems; /* nonzero subset of schema.registered_systems */
    const GeMpFieldSchema *fields;
    size_t field_count;
} GeMpRecordSchema;
typedef struct GeMpStateSchema {
    uint32_t version; /* native coverage schema version, distinct from wire */
    uint64_t registered_systems;
    uint8_t complete; /* explicit native-owner coverage attestation; 0 -> INCOMPLETE */
    const GeMpRecordSchema *records; /* strictly increasing kind; kind 0 first */
    size_t record_count;
} GeMpStateSchema;

typedef struct GeMpStateField {
    uint16_t id;
    uint8_t type;
    uint32_t count;
    /* Borrowed immutable typed array: uintN_t/intN_t/float; BOOL is uint8_t.
     * Unaligned storage is supported. No native pointer values go on wire. */
    const void *values;
} GeMpStateField;
typedef struct GeMpStateRecord {
    uint64_t actor_id; /* zero exactly once for globals; others strictly ascending */
    uint8_t kind; /* must match actor ID's top byte, or zero for globals */
    const GeMpStateField *fields; /* exactly schema fields, increasing IDs */
    size_t field_count;
} GeMpStateRecord;
typedef struct GeMpStateCoverage {
    uint64_t enumerated_systems;
    uint32_t unclassified_fields, unresolved_references;
} GeMpStateCoverage;
typedef struct GeMpCanonicalSnapshot {
    uint32_t session; /* fresh nonzero protocol session ID */
    uint64_t tick;
    uint64_t enabled_systems;
    uint8_t build_sha256[32], content_sha256[32], rules_sha256[32]; /* nonzero identities */
    const GeMpStateSchema *schema;
    const GeMpRegistry *registry;
    const GeMpStateRecord *records;
    size_t record_count;
    GeMpStateCoverage coverage;
} GeMpCanonicalSnapshot;

typedef struct GeMpCoverageReport {
    uint64_t missing_systems;
    size_t record_index, field_index; /* SIZE_MAX when not applicable */
    uint32_t unclassified_fields, unresolved_references;
    size_t encoded_size;
} GeMpCoverageReport;

/* Caller holds all schema, registry and values immutable for the whole call.
 * Output buffers must not overlap any borrowed input. Typed values provide
 * count * type-width readable bytes. Two-pass validation resolves forward
 * references before writing anything.
 * Every live registry entry requires one record; every supplied record and
 * field requires a complete schema. Non-finite floats, invalid bools, stale
 * IDs, unknown kinds and bounds errors never produce a state digest.
 * Unknown/missing coverage returns INCOMPLETE; malformed data returns INVALID
 * or STALE. State encoding preserves finite binary32 bits including -0.
 * No native owner, transport finish_tick hook, decode or restore is provided. */
GeMpStateStatus ge_mp_state_measure(const GeMpCanonicalSnapshot *snapshot,
    size_t *required_bytes, GeMpCoverageReport *report);
GeMpStateStatus ge_mp_state_encode(const GeMpCanonicalSnapshot *snapshot,
    uint8_t *destination, size_t capacity, size_t *written,
    GeMpCoverageReport *report);
GeMpStateStatus ge_mp_state_sha256(const GeMpCanonicalSnapshot *snapshot,
    uint8_t digest[32], GeMpCoverageReport *report);
/* Standard SHA-256 for manifests/known-vector testing; output unchanged on
 * invalid arguments. This alone makes no state-coverage claim. */
GeMpStateStatus ge_mp_sha256(const void *bytes, size_t length, uint8_t digest[32]);

#endif
