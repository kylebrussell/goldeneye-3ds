#include "ge_multiplayer_protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define QUEUE 4096U
#define TICKS 48U

typedef struct Event {
    uint8_t bytes[GE_MP_MAX_PACKET];
    size_t length;
    uint64_t due;
    unsigned source, destination;
} Event;
struct Network;
typedef struct Endpoint { struct Network *network; unsigned slot; } Endpoint;
typedef struct Network {
    GeMpProtocol peer[4];
    Endpoint endpoint[4];
    Event events[QUEUE];
    unsigned count, players, adversarial, sent[15], dropped, duplicated, corrupt;
    uint32_t random, model[4], completed[4];
    uint64_t now;
    unsigned blocked_type, blocked_slot, fatal_slot, divergent_slot;
} Network;

static uint32_t random_next(Network *n)
{
    n->random = n->random * 1664525U + 1013904223U;
    return n->random;
}
static void enqueue(Network *n, unsigned source, unsigned destination,
    const uint8_t *bytes, size_t length, uint64_t delay, int corrupt)
{
    Event *e;
    assert(n->count < QUEUE);
    e = &n->events[n->count++];
    memcpy(e->bytes, bytes, length);
    e->length = length; e->source = source; e->destination = destination;
    e->due = n->now + delay;
    if (corrupt) { e->bytes[length - 1] ^= 1; ++n->corrupt; }
}
static int send_mock(void *context, uint16_t destination, const uint8_t *bytes, size_t length)
{
    Endpoint *endpoint = context;
    Network *n = endpoint->network;
    uint32_t r = random_next(n);
    unsigned slot = destination - 11U, type = bytes[5];
    assert(destination >= 11U && slot < n->players);
    assert(type >= 1U && type <= 14U && length <= GE_MP_MAX_PACKET);
    ++n->sent[type];
    if (n->fatal_slot == endpoint->slot + 1U) return -1;
    if (type == n->blocked_type && (n->blocked_slot == 99U || n->blocked_slot == slot)) {
        ++n->dropped; return 1;
    }
    if (n->adversarial && r % 11U == 0) return 0; /* UDS queue full */
    if (n->adversarial && r % 4U == 0) { ++n->dropped; return 1; }
    enqueue(n, endpoint->slot, slot, bytes, length,
        n->adversarial ? 1U + (r >> 8) % 91U : 0U,
        n->adversarial && r % 27U == 0);
    if (n->adversarial && r % 5U == 0) {
        enqueue(n, endpoint->slot, slot, bytes, length, 3U + (r >> 16) % 137U, 0);
        ++n->duplicated;
    }
    return 1;
}
static GeMpConfig config(unsigned players)
{
    GeMpConfig c;
    unsigned i;
    memset(&c, 0, sizeof(c));
    c.player_count = (uint8_t)players; c.session = 0x125689afU;
    c.tick_hz = 60; c.rng_seed = UINT64_C(0xdeadbeef11223344);
    for (i = 0; i < 32; ++i) {
        c.build_sha256[i] = (uint8_t)(i + 1);
        c.content_sha256[i] = (uint8_t)(i + 51);
        c.rules_sha256[i] = (uint8_t)(i + 101);
        c.initial_state_sha256[i] = (uint8_t)(i + 151);
    }
    for (i = 0; i < players; ++i) c.nodes[i] = (uint16_t)(11 + i);
    return c;
}
static Network *network(unsigned players, uint32_t seed, unsigned adversarial)
{
    Network *n = calloc(1, sizeof(*n));
    GeMpConfig c = config(players);
    unsigned i;
    assert(n);
    n->players = players; n->random = seed; n->adversarial = adversarial;
    for (i = 0; i < players; ++i) {
        n->endpoint[i].network = n; n->endpoint[i].slot = i;
        n->model[i] = (uint32_t)c.rng_seed;
        assert(ge_mp_protocol_init(&n->peer[i], &c, i, send_mock, &n->endpoint[i], 0));
    }
    return n;
}
static GeMpInput sample(uint32_t tick, unsigned slot)
{
    GeMpInput input;
    input.pad[0].buttons = (uint16_t)(0x8001U ^ (tick * 71U + slot * 17U));
    input.pad[0].stick_x = (int8_t)((int)((tick * 3U + slot * 31U) % 256U) - 128);
    input.pad[0].stick_y = (int8_t)((int)((tick * 7U + slot * 11U) % 256U) - 128);
    input.pad[1].buttons = (uint16_t)(0x1420U ^ (tick * 41U + slot * 13U));
    input.pad[1].stick_x = (int8_t)((int)((tick * 5U + slot * 37U) % 256U) - 128);
    input.pad[1].stick_y = (int8_t)((int)((tick * 11U + slot * 7U) % 256U) - 128);
    return input;
}
/* This model is deliberately NOT gameplay or a substitute game-state SHA256.
 * Its independently predicted transcript detects loss/duplication/order errors. */
static uint32_t model_tick(uint32_t value, const GeMpFrame *frame)
{
    unsigned i, j;
    value = value * 1664525U + frame->tick + 1013904223U;
    for (i = 0; i < frame->player_count; ++i) for (j = 0; j < 2; ++j) {
        const GeMpPad *pad = &frame->input[i].pad[j];
        value = (value << 7 | value >> 25) ^ pad->buttons;
        value = value * 33U + (uint8_t)pad->stick_x;
        value = value * 33U + (uint8_t)pad->stick_y;
    }
    return value;
}
static void model_hash(uint8_t bytes[32], uint32_t model)
{
    unsigned i;
    for (i = 0; i < 32; ++i) bytes[i] = (uint8_t)((model >> ((i % 4U) * 8U)) ^ (i * 13U));
}
static void drive(Network *n, unsigned target_ticks)
{
    unsigned i, remaining;
    for (i = 0; i < n->players; ++i) {
        GeMpProtocol *p = &n->peer[i];
        GeMpInput input = sample(p->frame.tick, i);
        GeMpFrame frame;
        if (p->frame.tick < target_ticks && !(p->input_mask & (1U << i)))
            (void)ge_mp_protocol_submit_input(p, p->frame.tick, &input);
        if (ge_mp_protocol_take_frame(p, &frame)) {
            uint8_t hash[32];
            unsigned slot;
            assert(frame.tick == n->completed[i] && frame.player_count == n->players);
            for (slot = 0; slot < n->players; ++slot) {
                GeMpInput expected = sample(frame.tick, slot);
                assert(!memcmp(&expected, &frame.input[slot], sizeof(expected)));
            }
            assert(!ge_mp_protocol_take_frame(p, &frame));
            n->model[i] = model_tick(n->model[i], &frame);
            if (n->divergent_slot == i + 1U && frame.tick == 3U) n->model[i] ^= 1;
            model_hash(hash, n->model[i]);
            assert(ge_mp_protocol_finish_tick(p, frame.tick, hash));
            assert(!ge_mp_protocol_finish_tick(p, frame.tick, hash));
            ++n->completed[i];
        }
        ge_mp_protocol_poll(p, n->now);
    }
    /* Random queue rotation also exercises reorder among same-time packets. */
    remaining = n->count;
    while (remaining-- && n->count) {
        unsigned index = random_next(n) % n->count;
        Event e = n->events[index];
        if (e.due > n->now) continue;
        n->events[index] = n->events[--n->count];
        (void)ge_mp_protocol_receive(&n->peer[e.destination], (uint16_t)(11U + e.source),
            e.bytes, e.length, n->now);
    }
    n->now += 5;
}
static void test_loss(unsigned players, uint32_t seed)
{
    Network *n = network(players, seed, 1);
    GeMpFrame reference;
    uint32_t expected = (uint32_t)n->peer[0].config.rng_seed;
    unsigned i, tick, attempts;
    for (attempts = 0; attempts < 24000U; ++attempts) {
        unsigned finished = 0;
        drive(n, TICKS);
        for (i = 0; i < players; ++i) {
            assert(n->peer[i].state != GE_MP_ABORTED);
            finished += n->peer[i].frame.tick == TICKS;
        }
        if (finished == players) break;
    }
    assert(attempts < 24000U);
    reference.player_count = (uint8_t)players;
    for (tick = 0; tick < TICKS; ++tick) {
        reference.tick = tick;
        for (i = 0; i < players; ++i) reference.input[i] = sample(tick, i);
        expected = model_tick(expected, &reference);
    }
    for (i = 0; i < players; ++i) {
        assert(n->completed[i] == TICKS && n->model[i] == expected);
    }
    assert(n->dropped && n->duplicated && n->corrupt);
    free(n);
}
static uint32_t crc32(const uint8_t *bytes, size_t length)
{
    uint32_t crc = UINT32_MAX;
    unsigned bit;
    size_t i;
    for (i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320U : 0U);
    }
    return ~crc;
}
static void seal(uint8_t *bytes, size_t length)
{
    uint32_t crc = crc32(bytes, length - 4);
    bytes[length - 4] = (uint8_t)(crc >> 24); bytes[length - 3] = (uint8_t)(crc >> 16);
    bytes[length - 2] = (uint8_t)(crc >> 8); bytes[length - 1] = (uint8_t)crc;
}
static void test_wire(void)
{
    Network *n = network(2, 71, 0);
    uint8_t bytes[GE_MP_MAX_PACKET + 1];
    GeMpProtocol snapshot;
    Event e;
    unsigned i;
    ge_mp_protocol_poll(&n->peer[0], 0);
    assert(n->count == 1);
    e = n->events[0]; assert(e.length == GE_MP_MAX_PACKET);
    assert(!memcmp(e.bytes, "GEMP\x01\x01\x00\x02\x12\x56\x89\xaf", 12));
    assert(!memcmp(e.bytes + 144, "\xde\xad\xbe\xef\x11\x22\x33\x44\x00\x3c\x00\x0b\x00\x0c\x00\x00\x00\x00", 18));
    snapshot = n->peer[1];
    for (i = 0; i < e.length; ++i) {
        memcpy(bytes, e.bytes, e.length); bytes[i] ^= 1;
        assert(ge_mp_protocol_receive(&n->peer[1], 11, bytes, e.length, 0) == GE_MP_INVALID);
        assert(n->peer[1].state == snapshot.state);
    }
    for (i = 0; i < e.length; ++i)
        assert(ge_mp_protocol_receive(&n->peer[1], 11, e.bytes, i, 0) == GE_MP_INVALID);
    memcpy(bytes, e.bytes, e.length); bytes[e.length] = 0;
    assert(ge_mp_protocol_receive(&n->peer[1], 11, bytes, sizeof(bytes), 0) == GE_MP_INVALID);
    assert(ge_mp_protocol_receive(&n->peer[1], 13, e.bytes, e.length, 0) == GE_MP_INVALID);
    memcpy(bytes, e.bytes, e.length); bytes[8] ^= 1; seal(bytes, e.length);
    assert(ge_mp_protocol_receive(&n->peer[1], 11, bytes, e.length, 0) == GE_MP_IGNORED);
    memcpy(bytes, e.bytes, e.length); bytes[5] = 255; seal(bytes, e.length);
    assert(ge_mp_protocol_receive(&n->peer[1], 11, bytes, e.length, 0) == GE_MP_INVALID);
    /* Every compatibility byte, including final SHA bytes and PRNG/roster,
     * is compared after a correct CRC, rather than trusting a truncated ID. */
    for (i = 16; i < e.length - 4; ++i) {
        n->peer[1] = snapshot;
        memcpy(bytes, e.bytes, e.length); bytes[i] ^= 1; seal(bytes, e.length);
        assert(ge_mp_protocol_receive(&n->peer[1], 11, bytes, e.length, 0) == GE_MP_RECEIVED);
        assert(n->peer[1].state == GE_MP_ABORTED && n->peer[1].failure == GE_MP_FAILURE_CONFIG);
    }
    free(n);
}
static void test_barriers(void)
{
    unsigned type;
    /* Lose all PREPARE, COMMIT, START, or FRAME acknowledgments from one
     * participant: nobody may execute even if everyone else is ready. */
    for (type = 2; type <= 9; type = type == 6 ? 9 : type + 2) {
        Network *n = network(4, 91, 0);
        unsigned i;
        n->blocked_type = type; n->blocked_slot = 0;
        for (i = 0; i < 100; ++i) drive(n, 2);
        for (i = 0; i < 4; ++i) assert(n->completed[i] == 0);
        n->blocked_type = 0;
        for (i = 0; i < 400; ++i) drive(n, 2);
        for (i = 0; i < 4; ++i) assert(n->completed[i] == 2);
        free(n);
    }
}
static void test_terminal(void)
{
    Network *n = network(4, 93, 1);
    unsigned i;
    n->divergent_slot = 3;
    for (i = 0; i < 5000; ++i) {
        drive(n, 10);
        if (n->peer[0].state == GE_MP_ABORTED) break;
    }
    assert(i < 5000 && n->peer[0].failure == GE_MP_FAILURE_DESYNC);
    for (i = 0; i < 300; ++i) drive(n, 10);
    for (i = 0; i < 4; ++i) {
        assert(n->peer[i].state == GE_MP_ABORTED && n->completed[i] == 4);
    }
    free(n);
    n = network(2, 97, 0);
    n->blocked_type = 2; n->blocked_slot = 0;
    for (i = 0; i < 1000; ++i) drive(n, 10);
    assert(n->peer[0].state == GE_MP_ABORTED && n->peer[1].state == GE_MP_ABORTED);
    assert(!n->completed[0] && !n->completed[1]);
    free(n);
    n = network(2, 97, 0);
    ge_mp_protocol_disconnect(&n->peer[1], 0);
    assert(n->peer[1].failure == GE_MP_FAILURE_HOST_LOST);
    ge_mp_protocol_disconnect(&n->peer[0], 1);
    assert(n->peer[0].failure == GE_MP_FAILURE_PEER_LOST);
    free(n);
    n = network(2, 97, 0);
    n->fatal_slot = 1;
    ge_mp_protocol_poll(&n->peer[0], 0);
    assert(n->peer[0].failure == GE_MP_FAILURE_TRANSPORT);
    ge_mp_protocol_poll(&n->peer[1], 4);
    ge_mp_protocol_poll(&n->peer[1], 3);
    assert(n->peer[1].failure == GE_MP_FAILURE_CLOCK);
    free(n);
}
static void test_receive_deadline(void)
{
    const uint64_t times[] = { GE_MP_TIMEOUT_MS - 1U, GE_MP_TIMEOUT_MS, 10000U };
    unsigned slot, scenario;
    /* A receive-first event loop must enforce the same deadline as poll,
     * both for client PREPARE and host PREPARE_ACK progress. */
    for (slot = 0; slot < 2; ++slot) for (scenario = 0; scenario < 4; ++scenario) {
        Network *n = network(2, 101, 0);
        GeMpProtocol *p = &n->peer[slot];
        Event e;
        uint64_t now = scenario < 3 ? times[scenario] : 99U;
        uint64_t progress_before;
        unsigned aborts;
        ge_mp_protocol_poll(&n->peer[0], 0);
        assert(n->count == 1);
        e = n->events[0]; n->count = 0;
        if (!slot) {
            assert(ge_mp_protocol_receive(&n->peer[1], 11, e.bytes, e.length, 0) == GE_MP_RECEIVED);
            ge_mp_protocol_poll(&n->peer[1], 0);
            assert(n->count == 1);
            e = n->events[0]; n->count = 0;
        }
        ge_mp_protocol_poll(p, scenario < 3 ? GE_MP_TIMEOUT_MS - 1U : 100U);
        progress_before = p->progress_ms;
        assert(p->state != GE_MP_ABORTED);
        if (scenario == 0) {
            assert(ge_mp_protocol_receive(p, (uint16_t)(11U + e.source), e.bytes, e.length, now) == GE_MP_RECEIVED);
            assert(p->state == (slot ? GE_MP_PREPARE : GE_MP_COMMIT));
            assert(p->progress_ms == now);
            ge_mp_protocol_poll(p, GE_MP_TIMEOUT_MS);
            assert(p->state != GE_MP_ABORTED);
        } else {
            assert(ge_mp_protocol_receive(p, (uint16_t)(11U + e.source), e.bytes, e.length, now) == GE_MP_IGNORED);
            assert(p->state == GE_MP_ABORTED && !p->execution_ready);
            assert(p->failure == (scenario == 3 ? GE_MP_FAILURE_CLOCK :
                slot ? GE_MP_FAILURE_HOST_LOST : GE_MP_FAILURE_TIMEOUT));
            assert(p->progress_ms == progress_before);
            aborts = n->sent[14];
            ge_mp_protocol_poll(p, scenario == 3 ? 100U : now);
            assert(n->sent[14] == aborts + 1U);
        }
        free(n);
    }
    /* A delayed EXECUTE must not revive a stalled running match or expose
     * its old frame after a long pause before receive draining resumes. */
    {
        Network *n = network(2, 71, 0);
        GeMpFrame frame;
        Event e;
        unsigned i;
        n->blocked_type = 10; n->blocked_slot = 1;
        for (i = 0; i < 100; ++i) drive(n, 1);
        assert(n->peer[1].state == GE_MP_RUNNING && !n->completed[1]);
        assert(!ge_mp_protocol_take_frame(&n->peer[1], &frame));
        n->blocked_type = 0; n->count = 0;
        ge_mp_protocol_poll(&n->peer[0], n->now + GE_MP_RETRY_MS);
        assert(n->count == 1 && n->events[0].bytes[5] == 10);
        e = n->events[0];
        assert(ge_mp_protocol_receive(&n->peer[1], 11, e.bytes, e.length, 10000) == GE_MP_IGNORED);
        ge_mp_protocol_poll(&n->peer[1], 10000);
        assert(n->peer[1].state == GE_MP_ABORTED && n->peer[1].failure == GE_MP_FAILURE_HOST_LOST);
        assert(!ge_mp_protocol_take_frame(&n->peer[1], &frame));
        free(n);
    }
}
static void test_api(void)
{
    Network *n = network(2, 88, 0);
    GeMpConfig c = config(2);
    GeMpProtocol p;
    GeMpInput input = sample(0, 0);
    GeMpFrame frame;
    uint8_t hash[32] = { 1 };
    unsigned i;
    assert(!ge_mp_protocol_init(NULL, &c, 0, send_mock, NULL, 0));
    assert(!ge_mp_protocol_init(&p, &c, 2, send_mock, NULL, 0));
    c.nodes[1] = c.nodes[0]; assert(!ge_mp_protocol_init(&p, &c, 0, send_mock, NULL, 0));
    c = config(2); memset(c.content_sha256, 0, 32);
    assert(!ge_mp_protocol_init(&p, &c, 0, send_mock, NULL, 0));
    assert(!ge_mp_protocol_submit_input(&n->peer[0], 0, &input));
    assert(!ge_mp_protocol_take_frame(&n->peer[0], &frame));
    assert(!ge_mp_protocol_finish_tick(&n->peer[0], 0, hash));
    for (i = 0; i < 100; ++i) drive(n, 0);
    assert(ge_mp_protocol_submit_input(&n->peer[0], 0, &input));
    assert(ge_mp_protocol_submit_input(&n->peer[0], 0, &input));
    assert(!ge_mp_protocol_submit_input(&n->peer[0], 1, &input));
    input.pad[1].buttons ^= 1;
    assert(!ge_mp_protocol_submit_input(&n->peer[0], 0, &input));
    assert(n->peer[0].failure == GE_MP_FAILURE_CONFLICT);
    assert(ge_mp_actor_id(1, 3, 0xabcd, 0x12345678) == UINT64_C(0x0103abcd12345678));
    assert(ge_mp_actor_id_valid(ge_mp_actor_id(2, 255, 123, 4)));
    assert(!ge_mp_actor_id(0, 0, 0, 1)); assert(!ge_mp_actor_id(1, 4, 0, 1));
    assert(!ge_mp_actor_id(1, 0, 0, 0)); assert(!ge_mp_actor_id_valid(0));
    assert(!ge_mp_actor_id_valid(UINT64_C(0x0104000012345678)));
    free(n);
}
int main(void)
{
    unsigned seed;
    test_wire(); test_api(); test_barriers(); test_terminal(); test_receive_deadline();
    for (seed = 1; seed <= 24; ++seed) {
        test_loss(2, seed * 1709U); test_loss(4, seed * 7919U);
    }
    puts("multiplayer protocol: 2/4-peer adversarial transport fixtures passed (mock simulation only)");
    return 0;
}
