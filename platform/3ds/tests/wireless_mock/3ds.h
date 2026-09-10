/* Host-only UDS test double. Production compiles against real libctru. */
#ifndef GE_TEST_UDS_H
#define GE_TEST_UDS_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef int32_t Result;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
#define R_FAILED(r) ((r) < 0)
#define R_SUCCEEDED(r) ((r) >= 0)
#define UDS_HOST_NETWORKNODEID 1
#define UDS_BROADCAST_NETWORKNODEID 0xffff
#define UDS_DEFAULT_RECVBUFSIZE 0x2e30
#define UDS_DATAFRAME_MAXSIZE 0x5c6
#define UDS_SENDFLAG_Default 1
typedef enum { UDSCONTYPE_Client = 1 } udsConnectionType;
typedef struct { unsigned active; } udsBindContext;
typedef struct { uint8_t appdata[200]; size_t appdata_size; } udsNetworkStruct;
typedef struct { udsNetworkStruct network; } udsNetworkScanInfo;
typedef struct { u16 cur_NetworkNodeID; u16 node_bitmask; } udsConnectionStatus;
Result udsInit(size_t size, const char *username);
void udsExit(void);
Result udsDestroyNetwork(void);
Result udsDisconnectNetwork(void);
Result udsUnbind(udsBindContext *context);
Result udsScanBeacons(void *out, size_t capacity, udsNetworkScanInfo **networks,
    size_t *count, u32 comm_id, u8 id, const u8 *host, bool connected);
Result udsGetNetworkStructApplicationData(const udsNetworkStruct *network,
    void *out, size_t capacity, size_t *bytes);
void udsGenerateDefaultNetworkStruct(udsNetworkStruct *network,
    u32 comm_id, u8 id, u8 max_nodes);
Result udsCreateNetwork(const udsNetworkStruct *network, const void *pass,
    size_t pass_size, udsBindContext *context, u8 channel, u32 recv_size);
Result udsSetApplicationData(const void *data, size_t size);
Result udsEjectSpectator(void);
Result udsConnectNetwork(const udsNetworkStruct *network, const void *pass,
    size_t pass_size, udsBindContext *context, u16 source,
    udsConnectionType type, u8 channel, u32 recv_size);
Result udsGetConnectionStatus(udsConnectionStatus *status);
Result udsPullPacket(const udsBindContext *context, void *out,
    size_t capacity, size_t *bytes, u16 *source);
Result udsSendTo(u16 destination, u8 channel, u8 flags,
    const void *data, size_t size);
#endif
