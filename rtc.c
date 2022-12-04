// RTC Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
// Time for system time borrowing, SDL for square wave/interrupt generation
#include <time.h>
#include <SDL.h>

#include "defs.h"
#include "rtc.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

// ms delay between square wave outputs/interrupts
uint32_t squareWaveRate[16] = {0, 0, 0, 0, 0, 0, 1, 2, 4, 8, 16, 32, 63, 125, 250, 500};

void initRTC (struct structrtc* currrtc, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask) {
	currrtc->ioAddress = ioaddr;
	currrtc->ioAddressMask = ioaddrMask;
	ioBusPtr = ioBusPointer;
}

void writeRTCregs (struct structrtc* currrtc) {
	logmsgf(LOGRTC, "RTC: Write 0x%02X: 0x%02X\n", ioBusPtr->addr & 0x00003F, ioBusPtr->data & 0x00FF);
	if ((ioBusPtr->addr & 0x00003F) == 0x0C) {logmsgf(LOGRTC, "RTC: Error register C is read only.\n"); return;}
	currrtc->_direct[ioBusPtr->addr & 0x00003F] = (ioBusPtr->data & 0x00FF);
}

void readRTCregs (struct structrtc* currrtc) {
	ioBusPtr->data = currrtc->_direct[ioBusPtr->addr & 0x00003F];
	if ((ioBusPtr->addr & 0x00003F) == 0x0C) {logmsgf(LOGRTC, "RTC: Read register C causing clear.\n"); currrtc->regC = 0;}
	logmsgf(LOGRTC, "RTC: Read 0x%02X: 0x%02X\n", ioBusPtr->addr & 0x00003F, ioBusPtr->data & 0x00FF);
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

void cycleRTC (struct structrtc* currrtc) {
	if (currrtc->regB != 0) {
		// RTC effectively disabled if regB is zero.
		uint32_t msdelay = squareWaveRate[currrtc->regB & 0x0F];
		if ((SDL_GetTicks64() - currrtc->prevticks) >= msdelay) {
			currrtc->prevticks = SDL_GetTicks64();
			// Fire
			if (currrtc->regB & REGB_SquareWaveEn) {
				currrtc->sqwOut ^= 0x01;
			} else {
				currrtc->sqwOut = 0;
			}
			currrtc->regC |= REGC_PeriodicFlag;
		}

		// If any flags are set and enabled set IRQ Flag
		if ((currrtc->regC & REGC_PeriodicFlag & currrtc->regB) | (currrtc->regC & REGC_AlarmFlag & currrtc->regB) | (currrtc->regC & REGC_UpdateEnded & currrtc->regB)) {
			currrtc->regC |= REGC_IRQFlag;
		}
		
		currrtc->intReq = currrtc->regC >> 7;
	}
}