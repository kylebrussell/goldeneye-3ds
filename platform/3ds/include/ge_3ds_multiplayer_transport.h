#ifndef GE_3DS_MULTIPLAYER_TRANSPORT_H
#define GE_3DS_MULTIPLAYER_TRANSPORT_H

#include "ge_3ds_wireless.h"
#include "ge_multiplayer_protocol.h"

typedef struct Ge3dsMultiplayerTransport {
    GeMpProtocol protocol;
    Ge3dsWireless *wireless;
    Result last_result;
    uint16_t roster_mask;
} Ge3dsMultiplayerTransport;

/* Transfers exclusive packet-queue ownership from an already connected lobby.
 * No scanning, console UI, C3D or simulation calls. The config must contain real
 * full manifests/state hashes; existing lobby hints are insufficient.
 * Validates the entire fixed roster against current transport membership,
 * session, host role and local node before attaching. Failure leaves the
 * connected lobby untouched. Zero-initialize storage before its first begin;
 * it must outlive its ownership. Call end before reusing it. */
Result ge_3ds_multiplayer_transport_begin(Ge3dsMultiplayerTransport *transport,
    Ge3dsWireless *wireless, const GeMpConfig *config, unsigned local_slot,
    uint64_t now_ms);
/* Validate membership/deadlines and retry sends, then drain at most eight
 * full-size UDS packets. Queue-full is retried by the portable protocol.
 * A failure freezes protocol execution; ownership persists for repeated abort
 * delivery until end. An old detached instance cannot affect a new match. */
Result ge_3ds_multiplayer_transport_poll(Ge3dsMultiplayerTransport *transport,
    uint64_t now_ms);
/* End this match and leave its network. Keeps udsInit owned by Ge3dsWireless;
 * caller later calls ge_3ds_wireless_exit as usual. Idempotent. */
void ge_3ds_multiplayer_transport_end(Ge3dsMultiplayerTransport *transport);

#endif
