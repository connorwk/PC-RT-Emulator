// 8237 DMA Controller Emulation
#ifndef _8237
#define _8237
#include <stdint.h>
#include "iocc.h"

struct struct8237 {
	uint32_t ioAddress;
	uint32_t ioAddressMask;
	uint8_t reset;
	uint8_t flipFlop;
	union {
		uint8_t _modeRegs[4];
		struct {
			uint8_t modeReg0;
			uint8_t modeReg1;
			uint8_t modeReg2;
			uint8_t modeReg3;
		};
	};
	union {
			uint16_t _addrWordRegs[16];
		struct {
			uint16_t baseAddr0;
			uint16_t currAddr0;
			uint16_t baseWordCount0;
			uint16_t currWordCount0;
			uint16_t baseAddr1;
			uint16_t currAddr1;
			uint16_t baseWordCount1;
			uint16_t currWordCount1;
			uint16_t baseAddr2;
			uint16_t currAddr2;
			uint16_t baseWordCount2;
			uint16_t currWordCount2;
			uint16_t baseAddr3;
			uint16_t currAddr3;
			uint16_t baseWordCount3;
			uint16_t currWordCount3;
		};
	};
	uint16_t tempAddr;
	uint16_t tempWordCount;
	uint8_t statusReg;
	uint8_t commandReg;
	uint8_t tempReg;
	uint8_t maskReg;
	uint8_t requestReg;
	//uint8_t intreq;
};

#define CMD_DACKSense			0x80
#define CMD_DREQSense			0x40
#define CMD_WriteSel			0x20
#define CMD_Priority			0x10
#define CMD_Timing				0x08
#define CMD_CtrlrEnable		0x04
#define CMD_Chan0AddrHold	0x02
#define CMD_MemtoMem			0x01

#define MODE_ModeSelect			0xC0
#define MODE_AddrIncDec			0x20
#define MODE_AutoInit				0x10
#define MODE_TransferMode		0x0C
#define MODE_ChannelSelect	0x03

#define REQMASK_CMD_SetResetReq		0x04
#define REQMASK_CMD_ChannelSelect	0x03

#define REQMASK_Chan0	0x01
#define REQMASK_Chan1	0x02
#define REQMASK_Chan2	0x04
#define REQMASK_Chan3	0x08

void init8237 (struct struct8237* curr8237, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask);
void access8237 (struct struct8237* curr8237);
void cycle8237 (struct struct8237* curr8237);

#endif