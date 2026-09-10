#include "ge_3ds_wireless.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Result init_result, create_result, advertisement_result, send_result;
static unsigned exits, destroys, disconnects, unbinds, sends, pulls;
static unsigned packets_pending;
static u16 node = 1, members = 3;
static uint8_t incoming[GE_WIRELESS_PACKET_SIZE];
static uint8_t advertised[GE_WIRELESS_PACKET_SIZE];

Result udsInit(size_t size, const char *username)
{ assert(size % 0x1000 == 0); (void)username; return init_result; }
void udsExit(void) { ++exits; }
Result udsDestroyNetwork(void) { ++destroys; return 0; }
Result udsDisconnectNetwork(void) { ++disconnects; return 0; }
Result udsUnbind(udsBindContext *context) { (void)context; ++unbinds; return 0; }
Result udsScanBeacons(void *out, size_t capacity, udsNetworkScanInfo **networks,
    size_t *count, u32 comm_id, u8 id, const u8 *host, bool connected)
{
    GeWirelessPresence presence = {10, 0, 7, 8, 0};
    size_t i;
    (void)out; (void)capacity; (void)comm_id; (void)id; (void)host;
    assert(!connected);
    *count = 12;
    *networks = calloc(*count, sizeof(**networks));
    assert(*networks);
    for (i = 0; i < *count; ++i) {
        if (i == 0) presence.build_id = 99;
        else presence.build_id = 7;
        assert(ge_wireless_encode((*networks)[i].network.appdata,
            sizeof((*networks)[i].network.appdata), &presence));
        (*networks)[i].network.appdata_size = GE_WIRELESS_PACKET_SIZE;
    }
    return 0;
}
Result udsGetNetworkStructApplicationData(const udsNetworkStruct *network,
    void *out, size_t capacity, size_t *bytes)
{
    assert(capacity >= network->appdata_size);
    *bytes = network->appdata_size;
    memcpy(out, network->appdata, *bytes);
    return 0;
}
void udsGenerateDefaultNetworkStruct(udsNetworkStruct *network,
    u32 comm_id, u8 id, u8 max_nodes)
{ (void)comm_id; (void)id; assert(max_nodes == 4); memset(network, 0, sizeof(*network)); }
Result udsCreateNetwork(const udsNetworkStruct *network, const void *pass,
    size_t pass_size, udsBindContext *context, u8 channel, u32 recv_size)
{
    (void)network; (void)pass; (void)pass_size; (void)context;
    assert(channel && recv_size >= 0x5f4); return create_result;
}
Result udsSetApplicationData(const void *data, size_t size)
{ assert(size == sizeof(advertised)); memcpy(advertised, data, size); return advertisement_result; }
Result udsEjectSpectator(void) { return 0; }
Result udsConnectNetwork(const udsNetworkStruct *network, const void *pass,
    size_t pass_size, udsBindContext *context, u16 source,
    udsConnectionType type, u8 channel, u32 recv_size)
{
    (void)network; (void)pass; (void)pass_size; (void)context; (void)recv_size;
    assert(source == UDS_BROADCAST_NETWORKNODEID && type == UDSCONTYPE_Client && channel);
    return 0;
}
Result udsGetConnectionStatus(udsConnectionStatus *status)
{ status->cur_NetworkNodeID = node; status->node_bitmask = members; return 0; }
Result udsPullPacket(const udsBindContext *context, void *out,
    size_t capacity, size_t *bytes, u16 *source)
{
    (void)context;
    assert(capacity == UDS_DATAFRAME_MAXSIZE);
    ++pulls;
    *bytes = 0;
    *source = 0;
    if (packets_pending) {
        --packets_pending;
        *source = 2;
        *bytes = sizeof(incoming);
        memcpy(out, incoming, *bytes);
    }
    return 0;
}
Result udsSendTo(u16 destination, u8 channel, u8 flags,
    const void *data, size_t size)
{
    GeWirelessPresence presence;
    assert(destination == UDS_BROADCAST_NETWORKNODEID && channel && flags);
    assert(ge_wireless_decode(&presence, data, size));
    ++sends;
    return send_result;
}

int main(void)
{
    Ge3dsWireless wireless;
    GeWirelessPresence presence = {10, 1, 7, 8, 1};
    init_result = -1;
    assert(R_FAILED(ge_3ds_wireless_init(&wireless, 7, 8)));
    ge_3ds_wireless_exit(&wireless);
    assert(exits == 0);
    init_result = 0;
    assert(R_SUCCEEDED(ge_3ds_wireless_init(&wireless, 7, 8)));
    create_result = -2;
    assert(R_FAILED(ge_3ds_wireless_host(&wireless, 10)));
    assert(!wireless.connected && !destroys && !unbinds);
    create_result = 0;
    advertisement_result = -3;
    assert(R_FAILED(ge_3ds_wireless_host(&wireless, 10)));
    assert(!wireless.connected && destroys == 1 && unbinds == 1);
    advertisement_result = 0;
    assert(R_SUCCEEDED(ge_3ds_wireless_host(&wireless, 10)));
    assert(ge_wireless_decode(&presence, advertised, sizeof(advertised)));
    presence.sequence = 1;
    presence.ready = 1;
    assert(ge_wireless_encode(incoming, sizeof(incoming), &presence));
    packets_pending = 100;
    /* A match owns the same UDS binding: lobby must not consume its packets. */
    wireless.match_owner = (void *)&wireless;
    assert(R_FAILED(ge_3ds_wireless_poll(&wireless, 99)));
    assert(!pulls && !sends && packets_pending == 100);
    ge_3ds_wireless_ready(&wireless, 1);
    assert(!wireless.lobby.local.ready);
    wireless.match_owner = NULL;
    assert(R_SUCCEEDED(ge_3ds_wireless_poll(&wireless, 100)));
    assert(pulls == 8 && packets_pending == 92 && sends == 1);
    assert(wireless.lobby.peers[1].active);
    assert(R_SUCCEEDED(ge_3ds_wireless_poll(&wireless, 101)));
    assert(sends == 1);
    ge_3ds_wireless_ready(&wireless, 1);
    assert(R_SUCCEEDED(ge_3ds_wireless_poll(&wireless, 102)));
    assert(sends == 2 && ge_wireless_lobby_all_ready(&wireless.lobby));
    send_result = (Result)UINT32_C(0xc86113f0);
    assert(R_SUCCEEDED(ge_3ds_wireless_poll(&wireless, 352)));
    assert(wireless.connected && sends == 3);
    assert(R_SUCCEEDED(ge_3ds_wireless_poll(&wireless, 353)));
    assert(sends == 3);
    send_result = -4;
    assert(R_FAILED(ge_3ds_wireless_poll(&wireless, 602)));
    assert(!wireless.connected && destroys == 2 && unbinds == 2);
    send_result = 0;
    assert(R_SUCCEEDED(ge_3ds_wireless_scan(&wireless)));
    assert(wireless.network_count == 8 && wireless.incompatible_networks == 1);
    assert(R_FAILED(ge_3ds_wireless_join(&wireless, 8)));
    node = 2;
    assert(R_SUCCEEDED(ge_3ds_wireless_join(&wireless, 0)));
    assert(wireless.connected && !wireless.host && wireless.lobby.local_node == 2);
    members = 2; /* Host disappears. */
    assert(R_FAILED(ge_3ds_wireless_poll(&wireless, 700)));
    assert(!wireless.connected && disconnects == 1 && unbinds == 3);
    ge_3ds_wireless_exit(&wireless);
    ge_3ds_wireless_exit(&wireless);
    assert(exits == 1 && unbinds == 3);
    puts("3DS wireless transport mock tests passed");
    return 0;
}
