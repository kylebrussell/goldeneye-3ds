#include "ge_local_wireless.h"

#include <string.h>

static void write32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t read32(const uint8_t *p)
{
    return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16
        | (uint32_t)p[2] << 8 | p[3];
}

static uint32_t checksum(const uint8_t *bytes, size_t count)
{
    uint32_t crc = UINT32_MAX;
    size_t i;
    for (i = 0; i < count; ++i) {
        unsigned bit;
        crc ^= bytes[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (UINT32_C(0xedb88320)
                & (0U - (crc & 1U)));
    }
    return ~crc;
}

int ge_wireless_encode(uint8_t *out, size_t size,
    const GeWirelessPresence *presence)
{
    if (out == NULL || presence == NULL || size < GE_WIRELESS_PACKET_SIZE
        || presence->session == 0 || presence->ready > 1) return 0;
    memset(out, 0, GE_WIRELESS_PACKET_SIZE);
    memcpy(out, "GEWL", 4);
    out[4] = GE_WIRELESS_PROTOCOL_VERSION;
    out[5] = 1; /* Presence; no gameplay capability is advertised. */
    out[6] = presence->ready;
    write32(out + 8, presence->session);
    write32(out + 12, presence->sequence);
    write32(out + 16, presence->build_id);
    write32(out + 20, presence->asset_id);
    write32(out + 28, checksum(out, 28));
    return 1;
}

int ge_wireless_decode(GeWirelessPresence *out,
    const uint8_t *bytes, size_t size)
{
    GeWirelessPresence parsed;
    if (out == NULL || bytes == NULL || size != GE_WIRELESS_PACKET_SIZE
        || memcmp(bytes, "GEWL", 4) != 0
        || bytes[4] != GE_WIRELESS_PROTOCOL_VERSION || bytes[5] != 1
        || bytes[6] > 1 || bytes[7] != 0 || read32(bytes + 24) != 0
        || read32(bytes + 28) != checksum(bytes, 28)) return 0;
    parsed.session = read32(bytes + 8);
    if (parsed.session == 0) return 0;
    parsed.sequence = read32(bytes + 12);
    parsed.build_id = read32(bytes + 16);
    parsed.asset_id = read32(bytes + 20);
    parsed.ready = bytes[6];
    *out = parsed;
    return 1;
}

void ge_wireless_lobby_init(GeWirelessLobby *lobby, uint16_t local_node,
    uint32_t session, uint32_t build_id, uint32_t asset_id)
{
    if (lobby == NULL) return;
    memset(lobby, 0, sizeof(*lobby));
    if (local_node == 0 || local_node > GE_WIRELESS_MAX_NODES) return;
    lobby->local_node = local_node;
    lobby->local.session = session;
    lobby->local.build_id = build_id;
    lobby->local.asset_id = asset_id;
}

void ge_wireless_lobby_membership(GeWirelessLobby *lobby,
    uint16_t connected_mask, uint64_t now_ms)
{
    unsigned i;
    if (lobby == NULL) return;
    if (lobby->connected_mask != connected_mask) lobby->local.ready = 0;
    lobby->connected_mask = connected_mask;
    for (i = 0; i < GE_WIRELESS_MAX_NODES; ++i) {
        GeWirelessPeer *peer = &lobby->peers[i];
        if ((connected_mask & (1U << i)) == 0 || (peer->active
            && (now_ms < peer->seen_ms
                || now_ms - peer->seen_ms >= GE_WIRELESS_PEER_TIMEOUT_MS))) {
            if (peer->active) lobby->local.ready = 0;
            memset(peer, 0, sizeof(*peer));
        }
    }
}

GeWirelessReceive ge_wireless_lobby_receive(GeWirelessLobby *lobby,
    uint16_t source_node, const uint8_t *bytes, size_t size, uint64_t now_ms)
{
    GeWirelessPresence presence;
    GeWirelessPeer *peer;
    if (lobby == NULL) return GE_WIRELESS_INVALID;
    if (source_node == 0 || source_node > GE_WIRELESS_MAX_NODES
        || source_node == lobby->local_node
        || (lobby->connected_mask & (1U << (source_node - 1))) == 0
        || !ge_wireless_decode(&presence, bytes, size)) {
        ++lobby->rejected_packets;
        return GE_WIRELESS_INVALID;
    }
    if (presence.session != lobby->local.session
        || presence.build_id != lobby->local.build_id
        || presence.asset_id != lobby->local.asset_id) {
        ++lobby->rejected_packets;
        return GE_WIRELESS_INCOMPATIBLE;
    }
    peer = &lobby->peers[source_node - 1];
    /* RFC-1982 serial ordering, without implementation-defined signed casts. */
    if (peer->active && (presence.sequence - peer->presence.sequence == 0
        || presence.sequence - peer->presence.sequence >= UINT32_C(0x80000000)))
        return GE_WIRELESS_STALE;
    peer->presence = presence;
    peer->seen_ms = now_ms;
    peer->active = 1;
    return GE_WIRELESS_ACCEPTED;
}

unsigned ge_wireless_lobby_peer_count(const GeWirelessLobby *lobby)
{
    unsigned i, count = 0;
    if (lobby == NULL) return 0;
    for (i = 0; i < GE_WIRELESS_MAX_NODES; ++i)
        count += lobby->peers[i].active != 0;
    return count;
}

int ge_wireless_lobby_all_ready(const GeWirelessLobby *lobby)
{
    unsigned i, count = 0;
    if (lobby == NULL || !lobby->local.ready || lobby->local.session == 0
        || lobby->local_node == 0 || lobby->local_node > GE_WIRELESS_MAX_NODES
        || !(lobby->connected_mask & 1U)
        || !(lobby->connected_mask & (1U << (lobby->local_node - 1)))) return 0;
    for (i = 0; i < GE_WIRELESS_MAX_NODES; ++i) {
        if (!(lobby->connected_mask & (1U << i))) continue;
        ++count;
        if (i + 1 != lobby->local_node
            && (!lobby->peers[i].active || !lobby->peers[i].presence.ready)) return 0;
    }
    return count >= 2 && count <= GE_WIRELESS_MAX_PLAYERS;
}
