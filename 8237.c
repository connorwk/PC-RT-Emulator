// 8237 DMA Controller Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "8237.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

void init8237 (struct struct8237* curr8237, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask) {
	curr8237->ioAddress = ioaddr;
	curr8237->ioAddressMask = ioaddrMask;
	curr8237->flipFlop = 0;
	ioBusPtr = ioBusPointer;
}

void write8237 (struct struct8237* curr8237) {
	uint8_t data = (ioBusPtr->data & 0x00FF);
	if (!(ioBusPtr->addr & 0x000008)) {
			uint8_t offset = (ioBusPtr->addr & 0x000007) << 1;
			if (!curr8237->flipFlop) {
				if (!(ioBusPtr->addr & 0x000001)) {
					logmsgf(LOG8237, "8237: Write Base+Current Addr Reg %d lower 0x%02X -> 0x%04X\n", (ioBusPtr->addr & 0x000006) >> 1, data, curr8237->_addrWordRegs[offset]);
				} else {
					logmsgf(LOG8237, "8237: Write Base+Current Word Reg %d lower 0x%02X -> 0x%04X\n", (ioBusPtr->addr & 0x000006) >> 1, data, curr8237->_addrWordRegs[offset]);
				}
				curr8237->_addrWordRegs[offset] = (curr8237->_addrWordRegs[offset] & 0xFF00) | data;
				curr8237->_addrWordRegs[offset + 1] = (curr8237->_addrWordRegs[offset + 1] & 0xFF00) | data;
			} else {
				if (!(ioBusPtr->addr & 0x000001)) {
					logmsgf(LOG8237, "8237: Write Base+Current Addr Reg %d upper 0x%02X -> 0x%04X\n", (ioBusPtr->addr & 0x000006) >> 1, data, curr8237->_addrWordRegs[offset]);
				} else {
					logmsgf(LOG8237, "8237: Write Base+Current Word Reg %d upper 0x%02X -> 0x%04X\n", (ioBusPtr->addr & 0x000006) >> 1, data, curr8237->_addrWordRegs[offset]);
				}
				curr8237->_addrWordRegs[offset] = (curr8237->_addrWordRegs[offset] & 0x00FF) | (data << 8);
				curr8237->_addrWordRegs[offset + 1] = (curr8237->_addrWordRegs[offset + 1] & 0x00FF) | (data << 8);
			}
			curr8237->flipFlop = curr8237->flipFlop ^ 0x01;
	} else {
		switch (ioBusPtr->addr & 0x000007) {
			case 0x0:
				logmsgf(LOG8237, "8237: Write Command Reg 0x%02X\n", data);
				curr8237->commandReg = data;
				break;
			case 0x1:
				curr8237->requestReg = (curr8237->requestReg & ~(0x01 << (data & REQMASK_CMD_ChannelSelect))) | (((data & REQMASK_CMD_SetResetReq) >> 2) << (data & REQMASK_CMD_ChannelSelect));
				logmsgf(LOG8237, "8237: Write Request Reg Single 0x%02X -> 0x%02X\n", data, curr8237->requestReg);
				break;
			case 0x2:
				curr8237->maskReg = (curr8237->maskReg & ~(0x01 << (data & REQMASK_CMD_ChannelSelect))) | (((data & REQMASK_CMD_SetResetReq) >> 2) << (data & REQMASK_CMD_ChannelSelect));
				logmsgf(LOG8237, "8237: Write Mask Reg Single 0x%02X -> 0x%02X\n", data, curr8237->maskReg);
				break;
			case 0x3:
				curr8237->_modeRegs[ioBusPtr->data & MODE_ChannelSelect] = (data & ~MODE_ChannelSelect);
				logmsgf(LOG8237, "8237: Write Mode Reg %d 0x%02X -> 0x%02X\n", ioBusPtr->data & MODE_ChannelSelect, data, curr8237->_modeRegs[ioBusPtr->data & MODE_ChannelSelect]);
				break;
			case 0x4:
				curr8237->flipFlop = 0;
				logmsgf(LOG8237, "8237: Clear byte flip-flop\n");
				break;
			case 0x5:
				// Master Clear
				logmsgf(LOG8237, "8237: Master Clear\n");
				break;
			case 0x7:
				curr8237->maskReg = data & 0x0F;
				logmsgf(LOG8237, "8237: Write Mask Reg All 0x%02X\n", data & 0x0F);
				break;
		}
	}
}

void read8237 (struct struct8237* curr8237) {
	if (!(ioBusPtr->addr & 0x000008)) {
		uint8_t offset = (ioBusPtr->addr & 0x000007) << 1;
		if (!curr8237->flipFlop) {
			ioBusPtr->data = curr8237->_addrWordRegs[offset + 1] & 0x00FF;
			if (!(ioBusPtr->addr & 0x000001)) {
				logmsgf(LOG8237, "8237: Read Current Addr Reg %d lower 0x%04X -> 0x%02X\n", (ioBusPtr->addr & 0x000006) >> 1, ioBusPtr->data, curr8237->_addrWordRegs[offset + 1]);
			} else {
				logmsgf(LOG8237, "8237: Read Current Word Reg %d lower 0x%04X -> 0x%02X\n", (ioBusPtr->addr & 0x000006) >> 1, ioBusPtr->data, curr8237->_addrWordRegs[offset + 1]);
			}
		} else {
			ioBusPtr->data = (curr8237->_addrWordRegs[offset + 1] & 0xFF00) >> 8;
			if (!(ioBusPtr->addr & 0x000001)) {
				logmsgf(LOG8237, "8237: Read Current Addr Reg %d upper 0x%04X -> 0x%02X\n", (ioBusPtr->addr & 0x000006) >> 1, ioBusPtr->data, curr8237->_addrWordRegs[offset + 1]);
			} else {
				logmsgf(LOG8237, "8237: Read Current Word Reg %d upper 0x%04X -> 0x%02X\n", (ioBusPtr->addr & 0x000006) >> 1, ioBusPtr->data, curr8237->_addrWordRegs[offset + 1]);
			}
		}
		curr8237->flipFlop = curr8237->flipFlop ^ 0x01;
	} else {
		switch (ioBusPtr->addr & 0x000007) {
			case 0x0:
				ioBusPtr->data = curr8237->statusReg;
				logmsgf(LOG8237, "8237: Read Status Reg 0x%02X\n", ioBusPtr->data);
				break;
			case 0x5:
				ioBusPtr->data = curr8237->tempReg;
				logmsgf(LOG8237, "8237: Read Temp Reg 0x%02X\n", ioBusPtr->data);
				break;
		}
	}
}

void access8237 (struct struct8237* curr8237) {
	if (!curr8237->reset) {
		curr8237->flipFlop = 0;
		return;
	}
	if (ioBusPtr->io == 1) {
		if ((ioBusPtr->addr & curr8237->ioAddressMask) == curr8237->ioAddress) {
			if (ioBusPtr->rw) {
				write8237(curr8237);
			} else {
				read8237(curr8237);
			}
			ioBusPtr->cs16 = 0;
		}
	}
}

void cycle8237 (struct struct8237* curr8237) {
	if (!curr8237->reset) {
		curr8237->flipFlop = 0;
		return;
	}
}