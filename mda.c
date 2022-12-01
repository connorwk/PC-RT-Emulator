// MDA Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "mda.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

void initMDA (struct structmda* currmda, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask, uint32_t memaddr, uint32_t memaddrMask) {
	currmda->ioAddress = ioaddr;
	currmda->ioAddressMask = ioaddrMask;
	currmda->memAddress = memaddr;
	currmda->memAddressMask = memaddrMask;
	ioBusPtr = ioBusPointer;
}

void writeMDAregs (struct structmda* currmda) {
	if (ioBusPtr->addr == 0x0003B8) {
		logmsgf(LOGMDA, "MDA: Write Control Reg 0x%04X\n", ioBusPtr->data);
		currmda->ctrlReg = (ioBusPtr->data & 0x00FF);
	} else if ((ioBusPtr->addr & 0x000009) == 0x000000) {
		// CRTC Address Reg 0x3B4 (or 0x3B0,2,4,6)
		logmsgf(LOGMDA, "MDA: Write CRTC addr Reg 0x%04X\n", ioBusPtr->data);
	} else if ((ioBusPtr->addr & 0x000009) == 0x000001) {
		// CRTC Register 0x3B5 (or 0x3B1,3,5,7)
		logmsgf(LOGMDA, "MDA: Write CRTC data Reg 0x%04X\n", ioBusPtr->data);
	}
}

void readMDAregs (struct structmda* currmda) {
	if (ioBusPtr->addr == 0x0003BA) {
		ioBusPtr->data = currmda->statusReg;
		logmsgf(LOGMDA, "MDA: Read Status Reg 0x%04X\n", ioBusPtr->data);
	} else if ((ioBusPtr->addr & 0x000009) == 0x000001) {
		// CRTC Register 0x3B5 (or 0x3B1,3,5,7)
		logmsgf(LOGMDA, "MDA: Read CRTC data Reg 0x%04X\n", ioBusPtr->data);
	} else {
		ioBusPtr->data = 0;
	}
}

void writeMDAmem (struct structmda* currmda) {
	if (ioBusPtr->addr >= 0x0B0000 && ioBusPtr->addr <= 0x0B0F9F) {
		logmsgf(LOGMDA, "MDA: Write video memory 0x%04X\n", ioBusPtr->data);
		currmda->videoMem[ioBusPtr->addr & 0x000FFF] = (ioBusPtr->data & 0x00FF);
	}
}

void readMDAmem (struct structmda* currmda) {
	if (ioBusPtr->addr >= 0x0B0000 && ioBusPtr->addr <= 0x0B0F9F) {
		ioBusPtr->data = currmda->videoMem[ioBusPtr->addr & 0x000FFF];
		logmsgf(LOGMDA, "MDA: Read video memory 0x%04X\n", ioBusPtr->data);
	}
}

void accessMDA (struct structmda* currmda) {
	if (ioBusPtr->io == 1) {
		if ((ioBusPtr->addr & currmda->ioAddressMask) == currmda->ioAddress) {
			if (ioBusPtr->rw) {
				writeMDAregs(currmda);
			} else {
				readMDAregs(currmda);
			}
			ioBusPtr->cs16 = 0;
		}
	} else {
		if ((ioBusPtr->addr & currmda->memAddressMask) == currmda->memAddress) {
			if (ioBusPtr->rw) {
				writeMDAmem(currmda);
			} else {
				readMDAmem(currmda);
			}
			ioBusPtr->cs16 = 0;
		}
	}
}