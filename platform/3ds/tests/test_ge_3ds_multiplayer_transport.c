#include "ge_3ds_multiplayer_transport.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAPACITY 512U
typedef struct Datagram {
    uint8_t bytes[UDS_DATAFRAME_MAXSIZE];
    size_t length;
    uint16_t source;
} Datagram;
typedef struct Queue { Datagram packets[CAPACITY]; unsigned count; } Queue;
static Queue queues[4];
static unsigned selected, players, sends, pulls, leaves, busy;
static uint16_t members;
static Result status_result, pull_result, send_result;
static Ge3dsWireless wireless[4];
static Ge3dsMultiplayerTransport transport[4];

Result udsGetConnectionStatus(udsConnectionStatus *status)
{
    status->cur_NetworkNodeID = (u16)(selected + 1);
    status->node_bitmask = members;
    return status_result;
}
Result udsPullPacket(const udsBindContext *binding, void *out,
    size_t capacity, size_t *bytes, u16 *source)
{
    Queue *q = &queues[selected];
    (void)binding;
    assert(capacity == UDS_DATAFRAME_MAXSIZE);
    ++pulls; *bytes = 0;
    if (R_FAILED(pull_result)) return pull_result;
    if (q->count) {
        Datagram *d = &q->packets[--q->count]; /* LIFO exercises actual reordering. */
        *bytes = d->length; *source = d->source;
        memcpy(out, d->bytes, *bytes);
    }
    return 0;
}
Result udsSendTo(u16 destination, u8 channel, u8 flags, const void *data, size_t size)
{
    Queue *q;
    Datagram *d;
    assert(destination >= 1 && destination <= players && destination != selected + 1);
    assert(channel == 1 && flags == UDS_SENDFLAG_Default && size <= GE_MP_MAX_PACKET);
    ++sends;
    if (R_FAILED(send_result)) return send_result;
    if (busy && sends % 7 == 0) return (Result)UINT32_C(0xc86113f0);
    q = &queues[destination - 1];
    assert(q->count < CAPACITY);
    d = &q->packets[q->count++];
    memcpy(d->bytes, data, size); d->length = size; d->source = (u16)(selected + 1);
    return 0;
}
/* Only lifecycle glue is doubled here; the existing wireless suite verifies
 * its real leave/ownership guard against the broader UDS service doubles. */
void ge_3ds_wireless_leave(Ge3dsWireless *w)
{ ++leaves; w->connected = 0; w->match_owner = NULL; }

static GeMpConfig config(unsigned count)
{
    GeMpConfig c;
    unsigned i;
    memset(&c, 0, sizeof(c));
    c.player_count = (uint8_t)count; c.session = 123; c.tick_hz = 60; c.rng_seed = 1;
    memset(c.build_sha256, 1, 32); memset(c.content_sha256, 2, 32);
    memset(c.rules_sha256, 3, 32); memset(c.initial_state_sha256, 4, 32);
    for (i = 0; i < count; ++i) c.nodes[i] = (uint16_t)(i + 1);
    return c;
}
static void reset(unsigned count)
{
    unsigned i;
    memset(queues, 0, sizeof(queues));
    memset(wireless, 0, sizeof(wireless));
    memset(transport, 0, sizeof(transport));
    sends = pulls = leaves = busy = 0;
    status_result = pull_result = send_result = 0;
    players = count; members = (uint16_t)((1U << count) - 1U);
    for (i = 0; i < count; ++i) {
        wireless[i].initialized = wireless[i].connected = 1;
        wireless[i].host = i == 0;
        wireless[i].lobby.local_node = (uint16_t)(i + 1);
        wireless[i].lobby.local.session = 123;
    }
}
static void begin_all(unsigned count)
{
    GeMpConfig c = config(count);
    unsigned i;
    reset(count);
    for (i = 0; i < count; ++i) {
        selected = i;
        assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_begin(&transport[i], &wireless[i], &c, i, 0)));
        assert(wireless[i].match_owner == &transport[i]);
    }
}
static void test_handoff(void)
{
    GeMpConfig c = config(2);
    Ge3dsMultiplayerTransport replacement = {0};
    unsigned i;
    reset(2); selected = 0;
    members = 7;
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 0, 0)));
    assert(!wireless[0].match_owner && wireless[0].connected);
    members = 3; c.nodes[1] = 17;
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 0, 0)));
    c = config(2); c.session = 999;
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 0, 0)));
    c = config(2); c.nodes[0] = 2; c.nodes[1] = 1;
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 1, 0)));
    c = config(2); memset(c.content_sha256, 0, 32);
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 0, 0)));
    assert(!wireless[0].match_owner);
    c = config(2);
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_begin(&transport[0], &wireless[0], &c, 0, 0)));
    assert(R_FAILED(ge_3ds_multiplayer_transport_begin(&replacement, &wireless[0], &c, 0, 0)));
    /* Consume old presence and oversized packets, bounded at eight per poll. */
    for (i = 0; i < 12; ++i) {
        Datagram *d = &queues[0].packets[queues[0].count++];
        memset(d->bytes, 0, sizeof(d->bytes));
        d->length = i % 2 ? UDS_DATAFRAME_MAXSIZE : GE_WIRELESS_PACKET_SIZE;
        d->source = 2;
    }
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(pulls == 8 && queues[0].count == 4 && transport[0].protocol.invalid_packets == 8);
    assert(sends == 1);
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], 1)));
    assert(pulls == 13 && !queues[0].count && sends == 1);
    /* An external leave and new match do not let a stale owner tear it down. */
    ge_3ds_wireless_leave(&wireless[0]); wireless[0].connected = 1;
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_begin(&replacement, &wireless[0], &c, 0, 2)));
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[0], 2)));
    ge_3ds_multiplayer_transport_end(&transport[0]);
    assert(leaves == 1 && wireless[0].match_owner == &replacement && wireless[0].connected);
    ge_3ds_multiplayer_transport_end(&replacement);
    ge_3ds_multiplayer_transport_end(&replacement);
    assert(leaves == 2 && !wireless[0].match_owner && !wireless[0].connected);
}
static void test_failures(void)
{
    GeMpFrame frame;
    /* A valid PREPARE queued before a long pause must not renew its deadline. */
    begin_all(2); selected = 0;
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(queues[1].count == 1);
    selected = 1;
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[1], 10000)));
    assert(transport[1].protocol.failure == GE_MP_FAILURE_HOST_LOST);
    assert(!ge_mp_protocol_take_frame(&transport[1].protocol, &frame));
    begin_all(2); selected = 1; members = 2;
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[1], 0)));
    assert(transport[1].protocol.failure == GE_MP_FAILURE_HOST_LOST);
    begin_all(4); selected = 0; members = 7;
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(transport[0].protocol.failure == GE_MP_FAILURE_PEER_LOST);
    begin_all(2); selected = 0; members = 7;
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(transport[0].protocol.failure == GE_MP_FAILURE_CONFIG);
    begin_all(2); selected = 0; status_result = -7;
    assert(ge_3ds_multiplayer_transport_poll(&transport[0], 0) == -7);
    assert(transport[0].protocol.failure == GE_MP_FAILURE_TRANSPORT);
    begin_all(2); selected = 0; pull_result = -8;
    assert(ge_3ds_multiplayer_transport_poll(&transport[0], 0) == -8);
    assert(transport[0].protocol.failure == GE_MP_FAILURE_TRANSPORT);
    begin_all(2); selected = 0; send_result = -9;
    assert(R_FAILED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(transport[0].protocol.failure == GE_MP_FAILURE_TRANSPORT);
    begin_all(2); selected = 0; send_result = (Result)UINT32_C(0xc86113f0);
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], 0)));
    assert(sends == 1);
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], 1)));
    assert(sends == 1);
    assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[0], GE_MP_RETRY_MS)));
    assert(sends == 2 && transport[0].protocol.state != GE_MP_ABORTED);
}
static void test_sessions(unsigned count)
{
    uint32_t done[4] = {0}, value[4] = {0};
    unsigned i, step;
    begin_all(count); busy = 1;
    for (step = 0; step < 2000; ++step) {
        unsigned advanced = 0;
        for (i = 0; i < count; ++i) {
            GeMpProtocol *p = &transport[i].protocol;
            GeMpInput input = {{{(uint16_t)(p->frame.tick + i * 100), -128, 127}, {0x8123, 27, -43}}};
            GeMpFrame frame;
            selected = i;
            if (p->frame.tick < 12) (void)ge_mp_protocol_submit_input(p, p->frame.tick, &input);
            assert(R_SUCCEEDED(ge_3ds_multiplayer_transport_poll(&transport[i], step * 5U)));
            if (ge_mp_protocol_take_frame(p, &frame)) {
                uint8_t hash[32] = {0};
                unsigned slot;
                assert(frame.tick == done[i]);
                for (slot = 0; slot < count; ++slot) {
                    assert(frame.input[slot].pad[0].buttons == frame.tick + slot * 100);
                    assert(frame.input[slot].pad[0].stick_x == -128);
                    assert(frame.input[slot].pad[1].buttons == 0x8123);
                    value[i] = value[i] * 7U + frame.input[slot].pad[0].buttons;
                }
                for (slot = 0; slot < 32; ++slot) hash[slot] = (uint8_t)(value[i] >> ((slot % 4) * 8));
                assert(ge_mp_protocol_finish_tick(p, frame.tick, hash));
                ++done[i];
            }
            advanced += p->frame.tick == 12;
        }
        if (advanced == count) break;
    }
    assert(step < 2000);
    for (i = 0; i < count; ++i) {
        assert(done[i] == 12 && value[i] == value[0]);
        ge_3ds_multiplayer_transport_end(&transport[i]);
    }
    assert(leaves == count);
}
int main(void)
{
    test_handoff(); test_failures(); test_sessions(2); test_sessions(4);
    puts("3DS multiplayer UDS adapter: ownership, bounds, loss, and 2/4-peer mock sessions passed");
    return 0;
}
