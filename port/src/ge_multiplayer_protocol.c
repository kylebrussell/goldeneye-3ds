#include "ge_multiplayer_protocol.h"

#include <string.h>

/* All integer fields are big-endian; CRC covers header and payload. */
enum {
    PREPARE = 1, PREPARE_ACK, COMMIT, COMMIT_ACK, START, START_ACK,
    INPUT, FRAME, FRAME_ACK, EXECUTE, HASH, ADVANCE, ADVANCE_ACK, ABORT
};
enum { COLLECT, DELIVER, EXECUTING, ADVANCING };

static uint16_t get16(const uint8_t *p)
{ return (uint16_t)((uint16_t)p[0] << 8 | p[1]); }
static uint32_t get32(const uint8_t *p)
{ return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void put16(uint8_t *p, uint16_t n)
{ p[0] = (uint8_t)(n >> 8); p[1] = (uint8_t)n; }
static void put32(uint8_t *p, uint32_t n)
{ p[0] = (uint8_t)(n >> 24); p[1] = (uint8_t)(n >> 16); p[2] = (uint8_t)(n >> 8); p[3] = (uint8_t)n; }
static uint32_t crc32(const uint8_t *p, size_t size)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    unsigned j;
    for (i = 0; i < size; ++i) {
        crc ^= p[i];
        for (j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static uint8_t all(const GeMpProtocol *p)
{ return (uint8_t)((1U << p->config.player_count) - 1U); }
static uint8_t local(const GeMpProtocol *p)
{ return (uint8_t)(1U << p->local_slot); }
static int nonzero(const uint8_t *bytes, size_t size)
{
    size_t i;
    for (i = 0; i < size; ++i) if (bytes[i]) return 1;
    return 0;
}
static void config_bytes(uint8_t *bytes, const GeMpConfig *c)
{
    unsigned i;
    memcpy(bytes, c->build_sha256, 32);
    memcpy(bytes + 32, c->content_sha256, 32);
    memcpy(bytes + 64, c->rules_sha256, 32);
    memcpy(bytes + 96, c->initial_state_sha256, 32);
    put32(bytes + 128, (uint32_t)(c->rng_seed >> 32));
    put32(bytes + 132, (uint32_t)c->rng_seed);
    put16(bytes + 136, c->tick_hz);
    for (i = 0; i < GE_MP_MAX_PLAYERS; ++i) put16(bytes + 138 + i * 2, c->nodes[i]);
}
static void input_bytes(uint8_t *bytes, const GeMpInput *input)
{
    unsigned i;
    for (i = 0; i < 2; ++i) {
        put16(bytes + i * 4, input->pad[i].buttons);
        bytes[i * 4 + 2] = (uint8_t)input->pad[i].stick_x;
        bytes[i * 4 + 3] = (uint8_t)input->pad[i].stick_y;
    }
}
static int8_t signed_byte(uint8_t n)
{ return (int8_t)(n < 128U ? (int)n : (int)n - 256); }
static void read_input(GeMpInput *input, const uint8_t *bytes)
{
    unsigned i;
    for (i = 0; i < 2; ++i) {
        input->pad[i].buttons = get16(bytes + i * 4);
        input->pad[i].stick_x = signed_byte(bytes[i * 4 + 2]);
        input->pad[i].stick_y = signed_byte(bytes[i * 4 + 3]);
    }
}
static int input_equal(const GeMpInput *a, const GeMpInput *b)
{
    uint8_t aa[8], bb[8];
    input_bytes(aa, a); input_bytes(bb, b);
    return !memcmp(aa, bb, sizeof(aa));
}
static void progress(GeMpProtocol *p)
{ p->progress_ms = p->now_ms; p->send_due = 1; }
static int clock_ok(GeMpProtocol *p, uint64_t now)
{
    if (now < p->now_ms) {
        ge_mp_protocol_abort(p, GE_MP_FAILURE_CLOCK);
        return 0;
    }
    p->now_ms = now;
    return 1;
}
void ge_mp_protocol_abort(GeMpProtocol *p, GeMpFailure failure)
{
    if (!p || p->state == GE_MP_ABORTED) return;
    p->failure = failure > GE_MP_FAILURE_NONE && failure <= GE_MP_FAILURE_LOCAL_STOP
        ? failure : GE_MP_FAILURE_LOCAL_STOP;
    p->state = GE_MP_ABORTED;
    p->execution_ready = 0;
    p->send_due = 1;
}
static void host_progress(GeMpProtocol *p)
{
    unsigned i;
    if (p->local_slot || p->state == GE_MP_ABORTED) return;
    if (p->state >= GE_MP_PREPARE && p->state <= GE_MP_START && p->barrier_mask == all(p)) {
        p->state = (GeMpState)(p->state + 1);
        p->barrier_mask = local(p);
        progress(p);
    }
    if (p->state != GE_MP_RUNNING) return;
    if (p->phase == COLLECT && p->input_mask == all(p)) {
        p->phase = DELIVER;
        p->delivery_mask = local(p);
        progress(p);
    }
    if (p->phase == DELIVER && p->delivery_mask == all(p)) {
        p->phase = EXECUTING;
        p->execution_ready = 1;
        progress(p);
    }
    if (p->phase == EXECUTING && p->hash_mask == all(p)) {
        for (i = 1; i < p->config.player_count; ++i) {
            if (memcmp(p->hashes[0], p->hashes[i], 32)) {
                ge_mp_protocol_abort(p, GE_MP_FAILURE_DESYNC);
                return;
            }
        }
        p->phase = ADVANCING;
        p->advance_mask = local(p);
        progress(p);
    }
}
static void next_tick(GeMpProtocol *p)
{
    if (p->frame.tick == UINT32_MAX) {
        ge_mp_protocol_abort(p, GE_MP_FAILURE_TICK_LIMIT);
        return;
    }
    memcpy(p->previous_hash, p->hashes[p->local_slot], 32);
    p->previous_valid = 1;
    ++p->frame.tick;
    memset(p->frame.input, 0, sizeof(p->frame.input));
    memset(p->hashes, 0, sizeof(p->hashes));
    p->phase = COLLECT;
    p->input_mask = p->delivery_mask = p->hash_mask = p->advance_mask = 0;
    p->frame_taken = p->execution_ready = 0;
    progress(p);
}
static size_t packet_size(unsigned type)
{
    switch (type) {
    case PREPARE: return GE_MP_MAX_PACKET;
    case INPUT: return 28;
    case FRAME: case HASH: case ADVANCE: return 52;
    case ABORT: return 21;
    case PREPARE_ACK: case COMMIT: case COMMIT_ACK: case START: case START_ACK:
    case FRAME_ACK: case EXECUTE: case ADVANCE_ACK: return 20;
    default: return 0;
    }
}
static void send_packet(GeMpProtocol *p, unsigned destination, unsigned type, uint32_t tick)
{
    uint8_t packet[GE_MP_MAX_PACKET] = { 'G', 'E', 'M', 'P' };
    size_t size = packet_size(type);
    unsigned i;
    packet[4] = GE_MP_PROTOCOL_VERSION;
    packet[5] = (uint8_t)type;
    packet[6] = p->local_slot;
    packet[7] = p->config.player_count;
    put32(packet + 8, p->config.session);
    put32(packet + 12, tick);
    if (type == PREPARE) config_bytes(packet + 16, &p->config);
    if (type == INPUT) input_bytes(packet + 16, &p->frame.input[p->local_slot]);
    if (type == FRAME) for (i = 0; i < p->config.player_count; ++i)
        input_bytes(packet + 16 + i * 8, &p->frame.input[i]);
    if (type == HASH || type == ADVANCE) {
        const uint8_t *hash = p->hashes[p->local_slot];
        if (tick != p->frame.tick) hash = p->previous_hash;
        memcpy(packet + 16, hash, 32);
    }
    if (type == ABORT) packet[16] = (uint8_t)p->failure;
    put32(packet + size - 4, crc32(packet, size - 4));
    if (p->send(p->send_context, p->config.nodes[destination], packet, size) < 0)
        ge_mp_protocol_abort(p, GE_MP_FAILURE_TRANSPORT);
}
int ge_mp_protocol_init(GeMpProtocol *p, const GeMpConfig *c,
    unsigned local_slot, GeMpSend send, void *context, uint64_t now)
{
    unsigned i, j;
    if (!p || !c || !send || c->player_count < 2 || c->player_count > 4 ||
        local_slot >= c->player_count || !c->session || c->tick_hz != 60 ||
        !nonzero(c->build_sha256, 32) || !nonzero(c->content_sha256, 32) ||
        !nonzero(c->rules_sha256, 32) || !nonzero(c->initial_state_sha256, 32)) return 0;
    for (i = 0; i < GE_MP_MAX_PLAYERS; ++i) {
        if (i >= c->player_count) { if (c->nodes[i]) return 0; continue; }
        if (!c->nodes[i]) return 0;
        for (j = 0; j < i; ++j) if (c->nodes[j] == c->nodes[i]) return 0;
    }
    memset(p, 0, sizeof(*p));
    p->config = *c;
    p->send = send; p->send_context = context;
    p->local_slot = (uint8_t)local_slot;
    p->state = local_slot ? GE_MP_WAIT_PREPARE : GE_MP_PREPARE;
    p->barrier_mask = local(p);
    p->frame.player_count = c->player_count;
    p->now_ms = p->progress_ms = now;
    p->send_due = 1;
    return 1;
}
void ge_mp_protocol_poll(GeMpProtocol *p, uint64_t now)
{
    unsigned i, type = 0;
    if (!p || !p->send || !clock_ok(p, now)) return;
    if (p->state != GE_MP_ABORTED && now - p->progress_ms >= GE_MP_TIMEOUT_MS)
        ge_mp_protocol_abort(p, p->local_slot ? GE_MP_FAILURE_HOST_LOST : GE_MP_FAILURE_TIMEOUT);
    if (!p->send_due && now - p->last_send_ms < GE_MP_RETRY_MS) return;
    p->send_due = 0; p->last_send_ms = now;
    if (p->state == GE_MP_ABORTED) {
        /* Repeat the terminal abort so one dropped packet cannot strand peers. */
        for (i = 0; i < p->config.player_count; ++i) if (i != p->local_slot)
            send_packet(p, i, ABORT, p->frame.tick);
        return;
    }
    if (!p->local_slot) {
        if (p->state == GE_MP_PREPARE) type = PREPARE;
        if (p->state == GE_MP_COMMIT) type = COMMIT;
        if (p->state == GE_MP_START) type = START;
        if (p->state == GE_MP_RUNNING) {
            if (p->phase == DELIVER) type = FRAME;
            if (p->phase == EXECUTING) type = EXECUTE;
            if (p->phase == ADVANCING) type = ADVANCE;
        }
        if (type) for (i = 1; i < p->config.player_count; ++i) {
            if (p->state == GE_MP_ABORTED) break;
            send_packet(p, i, type, p->frame.tick);
        }
    } else {
        if (p->state == GE_MP_PREPARE) type = PREPARE_ACK;
        if (p->state == GE_MP_COMMIT) type = COMMIT_ACK;
        if (p->state == GE_MP_START) type = START_ACK;
        if (p->state == GE_MP_RUNNING && p->phase == DELIVER) type = FRAME_ACK;
        if (p->state == GE_MP_RUNNING && (p->hash_mask & local(p))) type = HASH;
        if (type) send_packet(p, 0, type, p->frame.tick);
        if (p->state >= GE_MP_START && p->state <= GE_MP_RUNNING &&
            p->phase == COLLECT && (p->input_mask & local(p)))
            send_packet(p, 0, INPUT, p->frame.tick);
    }
}
static GeMpReceive ignored(GeMpProtocol *p)
{ ++p->ignored_packets; return GE_MP_IGNORED; }
static GeMpReceive invalid(GeMpProtocol *p)
{ ++p->invalid_packets; return GE_MP_INVALID; }
GeMpReceive ge_mp_protocol_receive(GeMpProtocol *p, uint16_t source,
    const uint8_t *packet, size_t length, uint64_t now)
{
    unsigned type, slot, i;
    uint32_t tick;
    uint8_t bit;
    if (!p || !p->send || !packet) return GE_MP_INVALID;
    if (!clock_ok(p, now)) return GE_MP_IGNORED;
    if (length < 20 || length > GE_MP_MAX_PACKET || memcmp(packet, "GEMP", 4) ||
        packet[4] != GE_MP_PROTOCOL_VERSION || length != packet_size(packet[5]) ||
        get32(packet + length - 4) != crc32(packet, length - 4)) return invalid(p);
    type = packet[5]; slot = packet[6]; tick = get32(packet + 12);
    if (slot >= p->config.player_count || slot == p->local_slot ||
        p->config.nodes[slot] != source) return invalid(p);
    if (get32(packet + 8) != p->config.session) return ignored(p);
    if (packet[7] != p->config.player_count) {
        ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFIG); return GE_MP_RECEIVED;
    }
    if (p->state == GE_MP_ABORTED) return ignored(p);
    if (type == ABORT) {
        if (!packet[16] || packet[16] > GE_MP_FAILURE_LOCAL_STOP) return invalid(p);
        ge_mp_protocol_abort(p, (GeMpFailure)packet[16]); return GE_MP_RECEIVED;
    }
    /* Host-only and client-only packet roles, independent of claimed payload. */
    if ((type == PREPARE || type == COMMIT || type == START || type == FRAME ||
         type == EXECUTE || type == ADVANCE) ? slot != 0 : p->local_slot != 0)
        return invalid(p);
    if (type <= START_ACK && tick != 0) return invalid(p);
    bit = (uint8_t)(1U << slot);
    if (type == PREPARE) {
        uint8_t expected[146];
        config_bytes(expected, &p->config);
        if (memcmp(expected, packet + 16, sizeof(expected))) {
            ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFIG); return GE_MP_RECEIVED;
        }
        if (p->state != GE_MP_WAIT_PREPARE) return ignored(p);
        p->state = GE_MP_PREPARE; progress(p); return GE_MP_RECEIVED;
    }
    if (type == COMMIT || type == START) {
        GeMpState previous = type == COMMIT ? GE_MP_PREPARE : GE_MP_COMMIT;
        if (p->state != previous) return ignored(p);
        p->state = (GeMpState)(previous + 1); progress(p); return GE_MP_RECEIVED;
    }
    if (type == PREPARE_ACK || type == COMMIT_ACK || type == START_ACK) {
        GeMpState expected = type == PREPARE_ACK ? GE_MP_PREPARE :
            type == COMMIT_ACK ? GE_MP_COMMIT : GE_MP_START;
        if (p->state != expected || (p->barrier_mask & bit)) return ignored(p);
        p->barrier_mask |= bit; progress(p); host_progress(p); return GE_MP_RECEIVED;
    }
    if (p->state != GE_MP_START && p->state != GE_MP_RUNNING) return ignored(p);
    /* Last ADVANCE is retained for loss recovery. Later ticks are never
     * accepted early, and arbitrary old traffic cannot prolong the timeout. */
    if (p->previous_valid && tick == p->frame.tick - 1U) {
        if (type == ADVANCE) {
            if (memcmp(packet + 16, p->previous_hash, 32)) {
                ge_mp_protocol_abort(p, GE_MP_FAILURE_DESYNC); return GE_MP_RECEIVED;
            }
            send_packet(p, 0, ADVANCE_ACK, tick); return GE_MP_RECEIVED;
        }
        if (type == HASH) {
            if (memcmp(packet + 16, p->previous_hash, 32)) {
                ge_mp_protocol_abort(p, GE_MP_FAILURE_DESYNC); return GE_MP_RECEIVED;
            }
            send_packet(p, slot, ADVANCE, tick); return GE_MP_RECEIVED;
        }
        return ignored(p);
    }
    if (tick != p->frame.tick) return ignored(p);
    if (type == INPUT) {
        GeMpInput input;
        read_input(&input, packet + 16);
        if (p->input_mask & bit) {
            if (!input_equal(&input, &p->frame.input[slot]))
                ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFLICT);
            return ignored(p);
        }
        if (p->phase != COLLECT) return ignored(p);
        p->frame.input[slot] = input; p->input_mask |= bit;
        progress(p); host_progress(p); return GE_MP_RECEIVED;
    }
    if (type == FRAME) {
        GeMpInput inputs[GE_MP_MAX_PLAYERS];
        for (i = 0; i < GE_MP_MAX_PLAYERS; ++i) {
            read_input(&inputs[i], packet + 16 + i * 8);
            if (i >= p->config.player_count && nonzero(packet + 16 + i * 8, 8)) return invalid(p);
            if ((p->input_mask & (1U << i)) && !input_equal(&inputs[i], &p->frame.input[i])) {
                ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFLICT); return GE_MP_RECEIVED;
            }
        }
        if (!(p->input_mask & local(p))) return ignored(p);
        if (p->phase != COLLECT) return ignored(p);
        memcpy(p->frame.input, inputs, sizeof(inputs));
        p->input_mask = all(p); p->phase = DELIVER; p->state = GE_MP_RUNNING;
        progress(p); return GE_MP_RECEIVED;
    }
    if (type == FRAME_ACK) {
        if (p->phase != DELIVER || (p->delivery_mask & bit)) return ignored(p);
        p->delivery_mask |= bit; progress(p); host_progress(p); return GE_MP_RECEIVED;
    }
    if (type == EXECUTE) {
        if (p->phase != DELIVER) return ignored(p);
        p->phase = EXECUTING; p->execution_ready = 1; progress(p); return GE_MP_RECEIVED;
    }
    if (type == HASH) {
        if (p->hash_mask & bit) {
            if (memcmp(p->hashes[slot], packet + 16, 32)) ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFLICT);
            return ignored(p);
        }
        if (p->phase != EXECUTING) return ignored(p);
        memcpy(p->hashes[slot], packet + 16, 32); p->hash_mask |= bit;
        progress(p); host_progress(p); return GE_MP_RECEIVED;
    }
    if (type == ADVANCE) {
        if (!(p->hash_mask & local(p))) return ignored(p);
        if (memcmp(packet + 16, p->hashes[p->local_slot], 32)) {
            ge_mp_protocol_abort(p, GE_MP_FAILURE_DESYNC); return GE_MP_RECEIVED;
        }
        send_packet(p, 0, ADVANCE_ACK, tick);
        if (p->state != GE_MP_ABORTED) next_tick(p);
        return GE_MP_RECEIVED;
    }
    if (type == ADVANCE_ACK) {
        if (p->phase != ADVANCING || (p->advance_mask & bit)) return ignored(p);
        p->advance_mask |= bit; progress(p);
        if (p->advance_mask == all(p)) next_tick(p);
        return GE_MP_RECEIVED;
    }
    return ignored(p);
}
int ge_mp_protocol_submit_input(GeMpProtocol *p, uint32_t tick, const GeMpInput *input)
{
    if (!p || !input || (p->state != GE_MP_START && p->state != GE_MP_RUNNING) ||
        p->frame.tick != tick) return 0;
    if (p->input_mask & local(p)) {
        if (input_equal(input, &p->frame.input[p->local_slot])) return 1;
        ge_mp_protocol_abort(p, GE_MP_FAILURE_CONFLICT); return 0;
    }
    if (p->phase != COLLECT) return 0;
    p->frame.input[p->local_slot] = *input; p->input_mask |= local(p);
    progress(p); host_progress(p); return 1;
}
int ge_mp_protocol_take_frame(GeMpProtocol *p, GeMpFrame *frame)
{
    if (!p || !frame || p->state != GE_MP_RUNNING || !p->execution_ready || p->frame_taken) return 0;
    *frame = p->frame; p->frame_taken = 1;
    return 1;
}
int ge_mp_protocol_finish_tick(GeMpProtocol *p, uint32_t tick, const uint8_t hash[32])
{
    if (!p || !hash || p->state != GE_MP_RUNNING || tick != p->frame.tick ||
        !p->frame_taken || (p->hash_mask & local(p))) return 0;
    memcpy(p->hashes[p->local_slot], hash, 32); p->hash_mask |= local(p);
    progress(p); host_progress(p); return 1;
}
void ge_mp_protocol_disconnect(GeMpProtocol *p, unsigned slot)
{
    if (!p || slot >= p->config.player_count) return;
    ge_mp_protocol_abort(p, slot == 0 ? GE_MP_FAILURE_HOST_LOST : GE_MP_FAILURE_PEER_LOST);
}
uint64_t ge_mp_actor_id(uint8_t kind, uint8_t owner, uint16_t index, uint32_t generation)
{
    if (!kind || !generation || (owner >= GE_MP_MAX_PLAYERS && owner != UINT8_MAX)) return 0;
    return (uint64_t)kind << 56 | (uint64_t)owner << 48 | (uint64_t)index << 32 | generation;
}
int ge_mp_actor_id_valid(uint64_t id)
{
    return ge_mp_actor_id((uint8_t)(id >> 56), (uint8_t)(id >> 48),
        (uint16_t)(id >> 32), (uint32_t)id) == id && id != 0;
}
