// RTC Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "rtc.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

void initRTC (struct structrtc* currrtc, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask) {
	currrtc->ioAddress = ioaddr;
	currrtc->ioAddressMask = ioaddrMask;
	ioBusPtr = ioBusPointer;
}

void writeRTCregs (struct structrtc* currrtc) {
	currrtc->_direct[ioBusPtr->addr & 0x00003F] = (ioBusPtr->data & 0x00FF);
}

void readRTCregs (struct structrtc* currrtc) {
	ioBusPtr->data = currrtc->_direct[ioBusPtr->addr & 0x00003F];
}

void accessRTC (struct structrtc* currrtc) {
	if (ioBusPtr->io == 1) {
		if ((ioBusPtr->addr & currrtc->ioAddressMask) == currrtc->ioAddress) {
			if (ioBusPtr->rw) {
				writeRTCregs(currrtc);
			} else {
				readRTCregs(currrtc);
			}
			ioBusPtr->cs16 = 0;
		}
	}
}