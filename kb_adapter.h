// Keyboard Adapter Emulation
#ifndef _KB_ADAPTER
#define _KB_ADAPTER
#include <stdint.h>
#include "iocc.h"

#define RESET_Delay 8192
#define SOFTRESET_Delay RESET_Delay+2

struct structkbadpt {
	uint32_t ioAddress;
	uint32_t ioAddressMask;
	uint8_t reset;
	uint32_t initReq;
	uint8_t cmdReg;
	uint8_t PA;
	uint8_t PB;
	uint8_t PC;
	uint8_t sharedRam[32];
	uint8_t irqEn;
	uint8_t intReq;
	uint8_t ramQueue;
	uint8_t ramOffset;
	uint8_t ramClear;
	uint8_t keylock;
};

#define PC_PAOutBufEmpty	0x80
#define PC_PAInBufFull		0x20
#define PC_IntReq					0x08
#define PC_IntLevel				0x07

// pg. 5-111
#define MODE0_RepKBAckByteWithInfoInt	0x01
#define MODE0_RepUARTTXComplete				0x02
#define MODE0_RepKeylockSWChange			0x04
#define MODE0_RepSpkToneComplete			0x08
#define MODE0_BlockReceivedUARTBytes	0x20
#define MODE0_SysAttnSeqSearchEn			0x40
#define MODE0_SuppressClick						0x80
// pg. 5-112
#define MODE1_SpkVol						0x03
#define MODE1_DiagMode					0x04
#define MODE1_KBInterfaceEn			0x08
#define MODE1_UARTInterfaceEn		0x10
#define MODE1_HonorKeylock			0x20
#define MODE1_InhibitAutoClick	0x40
#define MODE1_IgnoreUARTInput		0x80
// pg. 5-112
#define STATUS_SpkTimerBusy			0x01
#define STATUS_TimeoutTimerBsy	0x02
#define STATUS_KeylockSet				0x04
#define STATUS_KBTXBusy					0x08
#define STATUS_UARTTXBusy				0x10
#define STATUS_ClickBusy				0x20
#define STATUS_SpkQueueFull			0x80

void initkbadpt (struct structkbadpt* currkbadpt, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask);
void accesskbadpt (struct structkbadpt* currkbadpt);
void cyclekbadpt (struct structkbadpt* currkbadpt);

#endif