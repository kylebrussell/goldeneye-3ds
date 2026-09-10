#include "ge_3ds_wireless.h"

#include <stdlib.h>
#include <string.h>

/* Private homebrew discovery ID, not a Nintendo title ID. */
#define GE_UDS_COMM_ID UINT32_C(0x47453037)
#define GE_UDS_CHANNEL 1U
#define GE_UDS_INVALID ((Result)-1)
#define GE_UDS_SCAN_BUFFER_SIZE 0x4000U
static const char ge_uds_passphrase[] = "GoldenEye3DS-lobby-v1";

Result ge_3ds_wireless_init(Ge3dsWireless *wireless,
    uint32_t build_id, uint32_t asset_id)
{
    Result result;
    if (wireless == NULL) return GE_UDS_INVALID;
    memset(wireless, 0, sizeof(*wireless));
    wireless->build_id = build_id;
    wireless->asset_id = asset_id;
    result = udsInit(0x3000, NULL);
    wireless->last_result = result;
    wireless->initialized = R_SUCCEEDED(result);
    return result;
}

void ge_3ds_wireless_leave(Ge3dsWireless *wireless)
{
    if (wireless == NULL) return;
    if (wireless->connected) {
        if (wireless->host) (void)udsDestroyNetwork();
        else (void)udsDisconnectNetwork();
        (void)udsUnbind(&wireless->binding);
    }
    memset(&wireless->binding, 0, sizeof(wireless->binding));
    memset(&wireless->lobby, 0, sizeof(wireless->lobby));
    wireless->connected = 0;
    wireless->host = 0;
    wireless->send_pending = 0;
    wireless->network_count = 0;
}

void ge_3ds_wireless_exit(Ge3dsWireless *wireless)
{
    if (wireless == NULL) return;
    ge_3ds_wireless_leave(wireless);
    if (wireless->initialized) udsExit();
    wireless->initialized = 0;
}

Result ge_3ds_wireless_scan(Ge3dsWireless *wireless)
{
    udsNetworkScanInfo *networks = NULL;
    size_t count = 0, i;
    void *scan;
    Result result;
    if (wireless == NULL || !wireless->initialized || wireless->connected)
        return GE_UDS_INVALID;
    wireless->network_count = 0;
    wireless->incompatible_networks = 0;
    scan = malloc(GE_UDS_SCAN_BUFFER_SIZE);
    if (scan == NULL) return wireless->last_result = GE_UDS_INVALID;
    result = udsScanBeacons(scan, GE_UDS_SCAN_BUFFER_SIZE, &networks, &count,
        GE_UDS_COMM_ID, GE_WIRELESS_PROTOCOL_VERSION, NULL, false);
    if (R_SUCCEEDED(result)) {
        for (i = 0; i < count; ++i) {
            GeWirelessPresence presence;
            uint8_t packet[GE_WIRELESS_PACKET_SIZE];
            size_t bytes = 0;
            if (R_FAILED(udsGetNetworkStructApplicationData(&networks[i].network,
                    packet, sizeof(packet), &bytes))
                || !ge_wireless_decode(&presence, packet, bytes)
                || presence.build_id != wireless->build_id
                || presence.asset_id != wireless->asset_id) {
                ++wireless->incompatible_networks;
                continue;
            }
            if (wireless->network_count < GE_3DS_WIRELESS_SCAN_LIMIT) {
                size_t index = wireless->network_count++;
                wireless->networks[index] = networks[i];
                wireless->advertisements[index] = presence;
            }
        }
    }
    free(networks);
    free(scan);
    return wireless->last_result = result;
}

Result ge_3ds_wireless_host(Ge3dsWireless *wireless, uint32_t session)
{
    udsNetworkStruct network;
    uint8_t packet[GE_WIRELESS_PACKET_SIZE];
    Result result;
    if (wireless == NULL || !wireless->initialized || wireless->connected
        || session == 0) return GE_UDS_INVALID;
    ge_wireless_lobby_init(&wireless->lobby, UDS_HOST_NETWORKNODEID,
        session, wireless->build_id, wireless->asset_id);
    udsGenerateDefaultNetworkStruct(&network, GE_UDS_COMM_ID,
        GE_WIRELESS_PROTOCOL_VERSION, GE_WIRELESS_MAX_PLAYERS);
    result = udsCreateNetwork(&network, ge_uds_passphrase,
        sizeof(ge_uds_passphrase), &wireless->binding, GE_UDS_CHANNEL,
        UDS_DEFAULT_RECVBUFSIZE);
    if (R_SUCCEEDED(result)) {
        wireless->connected = 1;
        wireless->host = 1;
        wireless->send_pending = 1;
        (void)ge_wireless_encode(packet, sizeof(packet), &wireless->lobby.local);
        result = udsSetApplicationData(packet, sizeof(packet));
        if (R_SUCCEEDED(result)) result = udsEjectSpectator();
        if (R_FAILED(result)) ge_3ds_wireless_leave(wireless);
    }
    return wireless->last_result = result;
}

Result ge_3ds_wireless_join(Ge3dsWireless *wireless, size_t network_index)
{
    Result result;
    udsConnectionStatus status;
    if (wireless == NULL || !wireless->initialized || wireless->connected
        || network_index >= wireless->network_count) return GE_UDS_INVALID;
    result = udsConnectNetwork(&wireless->networks[network_index].network,
        ge_uds_passphrase, sizeof(ge_uds_passphrase), &wireless->binding,
        UDS_BROADCAST_NETWORKNODEID, UDSCONTYPE_Client, GE_UDS_CHANNEL,
        UDS_DEFAULT_RECVBUFSIZE);
    if (R_SUCCEEDED(result)) {
        wireless->connected = 1;
        wireless->host = 0;
        result = udsGetConnectionStatus(&status);
        if (R_SUCCEEDED(result) && status.cur_NetworkNodeID > 1
            && status.cur_NetworkNodeID <= GE_WIRELESS_MAX_NODES) {
            ge_wireless_lobby_init(&wireless->lobby, status.cur_NetworkNodeID,
                wireless->advertisements[network_index].session,
                wireless->build_id, wireless->asset_id);
            wireless->send_pending = 1;
        } else {
            ge_3ds_wireless_leave(wireless);
            if (R_SUCCEEDED(result)) result = GE_UDS_INVALID;
        }
    }
    return wireless->last_result = result;
}

void ge_3ds_wireless_ready(Ge3dsWireless *wireless, int ready)
{
    if (wireless == NULL || !wireless->connected) return;
    wireless->lobby.local.ready = ready != 0;
    wireless->send_pending = 1;
}

Result ge_3ds_wireless_poll(Ge3dsWireless *wireless, uint64_t now_ms)
{
    udsConnectionStatus status;
    /* Receive the full UDS limit so oversized protocol packets are consumed
     * and rejected, rather than permanently blocking the receive queue. */
    uint8_t packet[UDS_DATAFRAME_MAXSIZE];
    Result result;
    unsigned i;
    if (wireless == NULL || !wireless->connected) return GE_UDS_INVALID;
    result = udsGetConnectionStatus(&status);
    if (R_FAILED(result)) goto failed;
    if (status.cur_NetworkNodeID != wireless->lobby.local_node
        || !(status.node_bitmask & 1U)
        || !(status.node_bitmask & (1U << (wireless->lobby.local_node - 1)))) {
        result = GE_UDS_INVALID;
        goto failed;
    }
    ge_wireless_lobby_membership(&wireless->lobby, status.node_bitmask, now_ms);
    for (i = 0; i < 8; ++i) {
        size_t bytes = 0;
        uint16_t source = 0;
        result = udsPullPacket(&wireless->binding, packet, sizeof(packet),
            &bytes, &source);
        if (R_FAILED(result)) goto failed;
        if (bytes == 0) break;
        (void)ge_wireless_lobby_receive(&wireless->lobby,
            source, packet, bytes, now_ms);
    }
    if (wireless->send_pending || now_ms < wireless->last_send_ms
        || now_ms - wireless->last_send_ms >= 250) {
        ++wireless->lobby.local.sequence;
        (void)ge_wireless_encode(packet, sizeof(packet), &wireless->lobby.local);
        result = udsSendTo(UDS_BROADCAST_NETWORKNODEID, GE_UDS_CHANNEL,
            UDS_SENDFLAG_Default, packet, GE_WIRELESS_PACKET_SIZE);
        /* libctru documents this one queue-full result as transient. Cast
         * explicitly because its macro compares signed Result to unsigned. */
        if (R_FAILED(result) && (uint32_t)result != UINT32_C(0xc86113f0)) goto failed;
        if (R_SUCCEEDED(result)) wireless->send_pending = 0;
        /* Throttle queue-full retries too; transient loss cannot spin. */
        wireless->last_send_ms = now_ms;
        if (R_FAILED(result)) wireless->send_pending = 0;
    }
    return wireless->last_result = 0;
failed:
    ge_3ds_wireless_leave(wireless);
    return wireless->last_result = result;
}

