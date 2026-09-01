#ifndef GE_PORT_PR_OS_H
#define GE_PORT_PR_OS_H

/* Source-compatible declarations needed to compile the original joy.c. */
#include <ultra64.h>

#define CONT_JOYPORT 0x0001U
#define CONT_ABSOLUTE 0x0001U
#define CONT_RELATIVE 0x0002U
#define CONT_CARD_ON 0x01U
#define CONT_NO_RESPONSE_ERROR 0x08U

#define PFS_ERR_DEVICE 11
#define PFS_ERR_ID_FATAL 10
#define PFS_ERR_INCONSISTENT 3

s32 debTryAdd(s32 *notice, const char *name);
s32 osContInit(OSMesgQueue *queue, u8 *controllers, OSContStatus *status);
s32 osContStartQuery(OSMesgQueue *queue);
void osContGetQuery(OSContStatus *status);
s32 osContStartReadData(OSMesgQueue *queue);
void osContGetReadData(OSContPad *pads);
s32 osPfsInit(OSMesgQueue *queue, OSPfs *pfs, s32 controller);
s32 osMotorInit(OSMesgQueue *queue, OSPfs *pfs, s32 controller);
s32 osMotorStart(OSPfs *pfs);
s32 osMotorStop(OSPfs *pfs);
s32 osEepromProbe(OSMesgQueue *queue);
s32 osEepromRead(OSMesgQueue *queue, u8 address, u8 *buffer);
s32 osEepromWrite(OSMesgQueue *queue, u8 address, u8 *buffer);
s32 osEepromLongRead(OSMesgQueue *queue, u8 address, u8 *buffer, s32 length);
s32 osEepromLongWrite(OSMesgQueue *queue, u8 address, u8 *buffer, s32 length);

#endif
