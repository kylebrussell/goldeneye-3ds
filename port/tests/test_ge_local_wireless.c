#include "ge_local_wireless.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void send_presence(GeWirelessLobby *lobby, uint16_t node,
    const GeWirelessPresence *presence, uint64_t now, GeWirelessReceive expected)
{
    uint8_t bytes[GE_WIRELESS_PACKET_SIZE];
    assert(ge_wireless_encode(bytes, sizeof(bytes), presence));
    assert(ge_wireless_lobby_receive(lobby, node, bytes, sizeof(bytes), now) == expected);
}

int main(void)
{
    GeWirelessPresence original = {0x12345678U, UINT32_MAX, 123, 456, 1};
    GeWirelessPresence decoded = {0};
    GeWirelessLobby lobby;
    uint8_t bytes[GE_WIRELESS_PACKET_SIZE + 1];
    unsigned i;
    assert(ge_wireless_encode(bytes, sizeof(bytes), &original));
    assert(bytes[8] == 0x12 && bytes[9] == 0x34
        && bytes[10] == 0x56 && bytes[11] == 0x78);
    assert(ge_wireless_decode(&decoded, bytes, GE_WIRELESS_PACKET_SIZE));
    assert(decoded.session == original.session && decoded.sequence == UINT32_MAX
        && decoded.build_id == 123 && decoded.asset_id == 456 && decoded.ready);
    for (i = 0; i < GE_WIRELESS_PACKET_SIZE; ++i) {
        bytes[i] ^= 1;
        assert(!ge_wireless_decode(&decoded, bytes, GE_WIRELESS_PACKET_SIZE));
        bytes[i] ^= 1;
    }
    for (i = 0; i < GE_WIRELESS_PACKET_SIZE; ++i)
        assert(!ge_wireless_decode(&decoded, bytes, i));
    assert(!ge_wireless_decode(&decoded, bytes, sizeof(bytes)));
    assert(!ge_wireless_decode(NULL, bytes, GE_WIRELESS_PACKET_SIZE));
    assert(!ge_wireless_encode(bytes, GE_WIRELESS_PACKET_SIZE - 1, &original));
    decoded = original;
    decoded.session = 0;
    assert(!ge_wireless_encode(bytes, sizeof(bytes), &decoded));
    decoded = original;
    decoded.ready = 2;
    assert(!ge_wireless_encode(bytes, sizeof(bytes), &decoded));

    ge_wireless_lobby_init(&lobby, 1, original.session, 123, 456);
    assert(!ge_wireless_lobby_all_ready(&lobby));
    ge_wireless_lobby_membership(&lobby, 3, 0);
    lobby.local.ready = 1;
    send_presence(&lobby, 2, &original, 10, GE_WIRELESS_ACCEPTED);
    assert(ge_wireless_lobby_peer_count(&lobby) == 1);
    assert(ge_wireless_lobby_all_ready(&lobby));
    send_presence(&lobby, 2, &original, 20, GE_WIRELESS_STALE);
    assert(lobby.peers[1].seen_ms == 10); /* Replays cannot keep a peer alive. */
    original.sequence = 0;
    send_presence(&lobby, 2, &original, 30, GE_WIRELESS_ACCEPTED);
    original.sequence = UINT32_MAX;
    send_presence(&lobby, 2, &original, 40, GE_WIRELESS_STALE);
    original.sequence = UINT32_C(0x80000000);
    send_presence(&lobby, 2, &original, 40, GE_WIRELESS_STALE);
    original.sequence = 1;
    ++original.build_id;
    send_presence(&lobby, 2, &original, 40, GE_WIRELESS_INCOMPATIBLE);
    --original.build_id;
    ++original.asset_id;
    send_presence(&lobby, 2, &original, 40, GE_WIRELESS_INCOMPATIBLE);
    --original.asset_id;
    ++original.session;
    send_presence(&lobby, 2, &original, 40, GE_WIRELESS_INCOMPATIBLE);
    --original.session;
    send_presence(&lobby, 0, &original, 40, GE_WIRELESS_INVALID);
    send_presence(&lobby, 1, &original, 40, GE_WIRELESS_INVALID);
    send_presence(&lobby, 17, &original, 40, GE_WIRELESS_INVALID);
    send_presence(&lobby, 3, &original, 40, GE_WIRELESS_INVALID);
    assert(lobby.rejected_packets == 7);

    ge_wireless_lobby_membership(&lobby, 3, 3029);
    assert(lobby.peers[1].active);
    ge_wireless_lobby_membership(&lobby, 3, 3030);
    assert(!lobby.peers[1].active && !lobby.local.ready);
    send_presence(&lobby, 2, &original, 3100, GE_WIRELESS_ACCEPTED);
    lobby.local.ready = 1;
    ge_wireless_lobby_membership(&lobby, 1, 3101);
    assert(!ge_wireless_lobby_peer_count(&lobby) && !lobby.local.ready);
    ge_wireless_lobby_membership(&lobby, 3, 3102);
    original.sequence = 0; /* UDS node reuse begins a new sequence. */
    send_presence(&lobby, 2, &original, 3103, GE_WIRELESS_ACCEPTED);
    lobby.local.ready = 1;
    ge_wireless_lobby_membership(&lobby, 3, 3000); /* Clock moved backwards. */
    assert(!lobby.peers[1].active && !lobby.local.ready);

    ge_wireless_lobby_membership(&lobby, 15, 4000);
    for (i = 2; i <= 4; ++i)
        send_presence(&lobby, (uint16_t)i, &original, 4001, GE_WIRELESS_ACCEPTED);
    lobby.local.ready = 1;
    assert(ge_wireless_lobby_all_ready(&lobby));
    ge_wireless_lobby_membership(&lobby, 31, 4002);
    send_presence(&lobby, 5, &original, 4003, GE_WIRELESS_ACCEPTED);
    lobby.local.ready = 1;
    assert(!ge_wireless_lobby_all_ready(&lobby)); /* Never admit >4 players. */
    ge_wireless_lobby_membership(&lobby, 14, 4004);
    lobby.local.ready = 1;
    assert(!ge_wireless_lobby_all_ready(&lobby)); /* Host lost. */
    puts("local wireless protocol and lobby tests passed");
    return 0;
}
