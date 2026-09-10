#ifndef GE_MULTIPLAYER_PROTOCOL_H
#define GE_MULTIPLAYER_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define GE_MP_MAX_PLAYERS 4U
#define GE_MP_DIGEST_SIZE 32U
#define GE_MP_MAX_PACKET 166U
#define GE_MP_RETRY_MS 40U
#define GE_MP_TIMEOUT_MS 3000U
#define GE_MP_PROTOCOL_VERSION 1U

/* Two original N64 controller samples per player, in original button mapping.
 * The caller supplies deterministic sampling and applies both samples to that
 * player's controller slots. No platform button masks are sent on the wire. */
typedef struct GeMpPad {
    uint16_t buttons;
    int8_t stick_x;
    int8_t stick_y;
} GeMpPad;
typedef struct GeMpInput { GeMpPad pad[2]; } GeMpInput;

typedef struct GeMpConfig {
    uint8_t build_sha256[GE_MP_DIGEST_SIZE];
    uint8_t content_sha256[GE_MP_DIGEST_SIZE];
    uint8_t rules_sha256[GE_MP_DIGEST_SIZE];
    uint8_t initial_state_sha256[GE_MP_DIGEST_SIZE];
    uint64_t rng_seed;
    uint32_t session;
    uint16_t nodes[GE_MP_MAX_PLAYERS]; /* host in slot 0; unused entries zero */
    uint16_t tick_hz; /* canonical fixed tick: 60 */
    uint8_t player_count; /* 2..4; immutable membership for the match */
} GeMpConfig;

typedef enum GeMpState {
    GE_MP_WAIT_PREPARE, GE_MP_PREPARE, GE_MP_COMMIT, GE_MP_START,
    GE_MP_RUNNING, GE_MP_ABORTED
} GeMpState;
typedef enum GeMpFailure {
    GE_MP_FAILURE_NONE, GE_MP_FAILURE_CONFIG, GE_MP_FAILURE_CONFLICT,
    GE_MP_FAILURE_DESYNC, GE_MP_FAILURE_HOST_LOST, GE_MP_FAILURE_PEER_LOST,
    GE_MP_FAILURE_TIMEOUT, GE_MP_FAILURE_TRANSPORT, GE_MP_FAILURE_CLOCK,
    GE_MP_FAILURE_TICK_LIMIT, GE_MP_FAILURE_LOCAL_STOP
} GeMpFailure;
typedef enum GeMpReceive {
    GE_MP_RECEIVED, GE_MP_IGNORED, GE_MP_INVALID
} GeMpReceive;

/* Return 1 if queued, 0 for temporary queue-full, -1 for fatal error.
 * Must copy packet bytes before returning; may not reenter the protocol.
 * A source node passed to receive MUST come from the transport, not payload. */
typedef int (*GeMpSend)(void *context, uint16_t destination_node,
    const uint8_t *packet, size_t length);

typedef struct GeMpFrame {
    uint32_t tick;
    uint8_t player_count;
    GeMpInput input[GE_MP_MAX_PLAYERS];
} GeMpFrame;

/* Public storage for stack/static allocation; fields are read-only to callers.
 * One in-flight tick is the entire retransmission window. No prediction,
 * fabricated input, host migration, late join, or state rollback is performed. */
typedef struct GeMpProtocol {
    GeMpConfig config;
    GeMpSend send;
    void *send_context;
    GeMpState state;
    GeMpFailure failure;
    GeMpFrame frame;
    uint8_t hashes[GE_MP_MAX_PLAYERS][GE_MP_DIGEST_SIZE];
    uint8_t previous_hash[GE_MP_DIGEST_SIZE];
    uint64_t now_ms, progress_ms, last_send_ms;
    uint32_t invalid_packets, ignored_packets;
    uint8_t local_slot, phase, barrier_mask, input_mask, delivery_mask;
    uint8_t hash_mask, advance_mask, frame_taken, execution_ready;
    uint8_t previous_valid, send_due;
} GeMpProtocol;

/* Digests must describe the real executable/simulation, all pack and sidecar
 * content, complete canonical rules/setup, and initialized simulation/PRNG.
 * Nonzero bytes are required but cannot establish the caller hashed everything.
 * A caller must never replace these with lobby version numbers or ROM labels. */
int ge_mp_protocol_init(GeMpProtocol *protocol, const GeMpConfig *config,
    unsigned local_slot, GeMpSend send, void *context, uint64_t now_ms);
void ge_mp_protocol_poll(GeMpProtocol *protocol, uint64_t now_ms);
GeMpReceive ge_mp_protocol_receive(GeMpProtocol *protocol,
    uint16_t source_node, const uint8_t *packet, size_t length, uint64_t now_ms);
/* Submit exactly once for the advertised tick, or repeat identical input.
 * Input is accepted at START (barrier pending) but execution remains gated. */
int ge_mp_protocol_submit_input(GeMpProtocol *protocol, uint32_t tick,
    const GeMpInput *input);
/* Returns each frame once, only after every participant acknowledged delivery.
 * Finish it with a canonical state SHA256 before the next frame can be taken. */
int ge_mp_protocol_take_frame(GeMpProtocol *protocol, GeMpFrame *frame);
int ge_mp_protocol_finish_tick(GeMpProtocol *protocol, uint32_t tick,
    const uint8_t state_sha256[GE_MP_DIGEST_SIZE]);
void ge_mp_protocol_disconnect(GeMpProtocol *protocol, unsigned slot);
void ge_mp_protocol_abort(GeMpProtocol *protocol, GeMpFailure failure);

/* Stable IDs for caller-owned actor registry: kind:8 owner:8 index:16
 * generation:32. Kind and generation must be nonzero; owner 0..3 or 255 for
 * world actors. Index is authored/setup index or deterministic allocation ID.
 * Never recycle a generation or derive any part from a process pointer. */
uint64_t ge_mp_actor_id(uint8_t kind, uint8_t owner, uint16_t index,
    uint32_t generation);
int ge_mp_actor_id_valid(uint64_t actor_id);

#endif
