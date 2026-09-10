#include "ge_3ds_multiplayer_transport.h"

#define GE_MP_UDS_CHANNEL 1U
#define GE_MP_UDS_INVALID ((Result)-1)

static int send_packet(void *context, uint16_t destination,
    const uint8_t *bytes, size_t size)
{
    Ge3dsMultiplayerTransport *transport = context;
    Ge3dsWireless *wireless = transport->wireless;
    Result result;
    if (!wireless || wireless->match_owner != transport || !wireless->connected)
        return -1;
    result = udsSendTo(destination, GE_MP_UDS_CHANNEL, UDS_SENDFLAG_Default,
        bytes, size);
    transport->last_result = result;
    if (R_SUCCEEDED(result)) return 1;
    if ((uint32_t)result == UINT32_C(0xc86113f0)) return 0;
    return -1;
}
Result ge_3ds_multiplayer_transport_begin(Ge3dsMultiplayerTransport *transport,
    Ge3dsWireless *wireless, const GeMpConfig *config, unsigned local_slot,
    uint64_t now_ms)
{
    udsConnectionStatus status;
    Result result;
    uint16_t mask = 0;
    unsigned i;
    if (!transport || transport->wireless || !wireless || !config || !wireless->initialized ||
        !wireless->connected || wireless->match_owner || config->player_count < 2 ||
        config->player_count > GE_MP_MAX_PLAYERS || local_slot >= config->player_count ||
        config->session != wireless->lobby.local.session ||
        config->nodes[0] != UDS_HOST_NETWORKNODEID ||
        config->nodes[local_slot] != wireless->lobby.local_node ||
        (local_slot == 0) != (wireless->host != 0)) return GE_MP_UDS_INVALID;
    for (i = 0; i < config->player_count; ++i) {
        if (!config->nodes[i] || config->nodes[i] > GE_WIRELESS_MAX_NODES)
            return GE_MP_UDS_INVALID;
        mask |= (uint16_t)(1U << (config->nodes[i] - 1U));
    }
    result = udsGetConnectionStatus(&status);
    if (R_FAILED(result)) return result;
    if (status.node_bitmask != mask ||
        status.cur_NetworkNodeID != config->nodes[local_slot]) return GE_MP_UDS_INVALID;
    /* Validate and initialize first: do not steal the queue on any failure. */
    if (!ge_mp_protocol_init(&transport->protocol, config, local_slot,
            send_packet, transport, now_ms)) return GE_MP_UDS_INVALID;
    transport->wireless = wireless;
    transport->roster_mask = mask;
    transport->last_result = 0;
    wireless->match_owner = transport;
    return 0;
}
Result ge_3ds_multiplayer_transport_poll(Ge3dsMultiplayerTransport *transport,
    uint64_t now_ms)
{
    uint8_t packet[UDS_DATAFRAME_MAXSIZE];
    udsConnectionStatus status;
    Ge3dsWireless *wireless;
    Result result;
    unsigned i;
    if (!transport || !transport->wireless) return GE_MP_UDS_INVALID;
    wireless = transport->wireless;
    if (wireless->match_owner != transport || !wireless->connected) {
        ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_TRANSPORT);
        return transport->last_result = GE_MP_UDS_INVALID;
    }
    result = udsGetConnectionStatus(&status);
    if (R_FAILED(result)) {
        ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_TRANSPORT);
        return transport->last_result = result;
    }
    if (status.cur_NetworkNodeID != transport->protocol.config.nodes[transport->protocol.local_slot])
        ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_TRANSPORT);
    if (status.node_bitmask != transport->roster_mask) {
        for (i = 0; i < transport->protocol.config.player_count; ++i) {
            uint16_t node = transport->protocol.config.nodes[i];
            if (!(status.node_bitmask & (1U << (node - 1U))))
                ge_mp_protocol_disconnect(&transport->protocol, i);
        }
        /* Additional node membership is not a permitted mid-match join. */
        if (status.node_bitmask & (uint16_t)~transport->roster_mask)
            ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_CONFIG);
    }
    /* Apply the elapsed deadline before draining queued traffic. In particular,
     * a stale EXECUTE after suspend cannot resurrect an expired tick. */
    ge_mp_protocol_poll(&transport->protocol, now_ms);
    for (i = 0; i < 8; ++i) {
        size_t bytes = 0;
        uint16_t source = 0;
        result = udsPullPacket(&wireless->binding, packet, sizeof(packet), &bytes, &source);
        if (R_FAILED(result)) {
            ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_TRANSPORT);
            return transport->last_result = result;
        }
        if (!bytes) break;
        /* Old lobby packets and oversize datagrams are consumed and rejected.
         * The trusted UDS source is kept separate from the claimed slot. */
        (void)ge_mp_protocol_receive(&transport->protocol, source, packet, bytes, now_ms);
    }
    if (transport->protocol.state == GE_MP_ABORTED)
        return transport->last_result = GE_MP_UDS_INVALID;
    return transport->last_result = 0;
}
void ge_3ds_multiplayer_transport_end(Ge3dsMultiplayerTransport *transport)
{
    if (!transport) return;
    if (transport->wireless && transport->wireless->match_owner == transport) {
        ge_mp_protocol_abort(&transport->protocol, GE_MP_FAILURE_LOCAL_STOP);
        ge_3ds_wireless_leave(transport->wireless);
    }
    transport->wireless = NULL;
}
