#ifndef GE_3DS_WIRELESS_H
#define GE_3DS_WIRELESS_H

#include <3ds.h>
#include "ge_local_wireless.h"

#define GE_3DS_WIRELESS_SCAN_LIMIT 8U

struct Ge3dsMultiplayerTransport;

typedef struct Ge3dsWireless {
    GeWirelessLobby lobby;
    udsBindContext binding;
    udsNetworkScanInfo networks[GE_3DS_WIRELESS_SCAN_LIMIT];
    GeWirelessPresence advertisements[GE_3DS_WIRELESS_SCAN_LIMIT];
    size_t network_count;
    uint32_t build_id;
    uint32_t asset_id;
    uint64_t last_send_ms;
    Result last_result;
    unsigned incompatible_networks;
    uint8_t initialized;
    uint8_t connected;
    uint8_t host;
    uint8_t send_pending;
    /* Exclusive queue ownership after an explicit match handoff. */
    struct Ge3dsMultiplayerTransport *match_owner;
} Ge3dsWireless;

/* One instance owns UDS. No service activity occurs until init. */
Result ge_3ds_wireless_init(Ge3dsWireless *wireless,
    uint32_t build_id, uint32_t asset_id);
Result ge_3ds_wireless_scan(Ge3dsWireless *wireless);
Result ge_3ds_wireless_host(Ge3dsWireless *wireless, uint32_t session);
Result ge_3ds_wireless_join(Ge3dsWireless *wireless, size_t network_index);
/* Bounded to eight datagrams and one send; call from a lobby once per frame.
 * Rejects without draining while a multiplayer transport owns this queue. */
Result ge_3ds_wireless_poll(Ge3dsWireless *wireless, uint64_t now_ms);
void ge_3ds_wireless_ready(Ge3dsWireless *wireless, int ready);
void ge_3ds_wireless_leave(Ge3dsWireless *wireless);
void ge_3ds_wireless_exit(Ge3dsWireless *wireless);

/* Diagnostic console lobby, BEFORE C3D_Init, with gfx/console initialized.
 * Returns after B/START/APT exit and releases UDS. No matches are started. */
void ge_3ds_wireless_lobby_run(uint32_t build_id, uint32_t asset_id);

#endif
