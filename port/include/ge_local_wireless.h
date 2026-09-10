#ifndef GE_LOCAL_WIRELESS_H
#define GE_LOCAL_WIRELESS_H

#include <stddef.h>
#include <stdint.h>

#define GE_WIRELESS_PROTOCOL_VERSION 1U
#define GE_WIRELESS_PACKET_SIZE 32U
#define GE_WIRELESS_MAX_NODES 16U
#define GE_WIRELESS_MAX_PLAYERS 4U
#define GE_WIRELESS_PEER_TIMEOUT_MS 3000U

/* Wire records are encoded explicitly; never transmit a native C struct.
 * This protocol deliberately advertises only a lobby, not working matches. */
typedef struct GeWirelessPresence {
    uint32_t session;
    uint32_t sequence;
    uint32_t build_id;
    uint32_t asset_id;
    uint8_t ready;
} GeWirelessPresence;

typedef struct GeWirelessPeer {
    GeWirelessPresence presence;
    uint64_t seen_ms;
    uint8_t active;
} GeWirelessPeer;

typedef struct GeWirelessLobby {
    GeWirelessPresence local;
    GeWirelessPeer peers[GE_WIRELESS_MAX_NODES];
    uint16_t connected_mask;
    uint16_t local_node;
    uint32_t rejected_packets;
} GeWirelessLobby;

typedef enum GeWirelessReceive {
    GE_WIRELESS_ACCEPTED,
    GE_WIRELESS_INVALID,
    GE_WIRELESS_INCOMPATIBLE,
    GE_WIRELESS_STALE
} GeWirelessReceive;

int ge_wireless_encode(uint8_t *out, size_t size,
    const GeWirelessPresence *presence);
int ge_wireless_decode(GeWirelessPresence *out,
    const uint8_t *bytes, size_t size);
void ge_wireless_lobby_init(GeWirelessLobby *lobby, uint16_t local_node,
    uint32_t session, uint32_t build_id, uint32_t asset_id);
/* Supply the transport-reported source node and membership mask;
 * payloads cannot claim another player's identity. Disconnect clears ready. */
void ge_wireless_lobby_membership(GeWirelessLobby *lobby,
    uint16_t connected_mask, uint64_t now_ms);
GeWirelessReceive ge_wireless_lobby_receive(GeWirelessLobby *lobby,
    uint16_t source_node, const uint8_t *bytes, size_t size, uint64_t now_ms);
unsigned ge_wireless_lobby_peer_count(const GeWirelessLobby *lobby);
int ge_wireless_lobby_all_ready(const GeWirelessLobby *lobby);

#endif
