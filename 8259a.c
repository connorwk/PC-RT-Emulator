// 8259A Interrupt Controller Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "8259a.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

void init8259 (struct struct8259* curr8259, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask) {
	curr8259->initreq = 4;
	curr8259->edgeLatches = 0xFF;
	curr8259->ioAddress = ioaddr;
	curr8259->ioAddressMask = ioaddrMask;
	ioBusPtr = ioBusPointer;
}

void write8259 (struct struct8259* curr8259) {
	uint8_t data = (ioBusPtr->data & 0x00FF);
	if (curr8259->initreq) {
		//logmsgf(LOG8259, "8259: Initreq 0x%08X\n", curr8259->initreq);
		if (curr8259->initreq == 4) {
			if (ioBusPtr->addr & 0x000001) {
				logmsgf(LOG8259, "8259: Error writing ICW1 addr bit 0 should be 0 0x%06X\n", ioBusPtr->addr);
			}
			// Write ICW1
			logmsgf(LOG8259, "8259: Write ICW1 0x%04X\n", ioBusPtr->data);
			curr8259->icw1 = data;
			// Reset some other stuff
			curr8259->ocw3 = 0x4A;
		} else if (curr8259->initreq == 3) {
			if (!(ioBusPtr->addr & 0x000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW2 addr bit 0 should be 1 0x%06X\n", ioBusPtr->addr);
			}
			// Write ICW2
			logmsgf(LOG8259, "8259: Write ICW2 0x%04X\n", ioBusPtr->data);
			curr8259->icw2 = data;

			if (curr8259->icw1 & ICW1_SingleMode) {
				curr8259->initreq--;
			}
			if (!(curr8259->icw1 & ICW1_ICW4Needed)) {
				curr8259->initreq--;
			}
		} else if (curr8259->initreq == 2) {
			if (!(ioBusPtr->addr & 0x000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW3 addr bit 0 should be 1 0x%06X\n", ioBusPtr->addr);
			}
			// Write ICW3
			logmsgf(LOG8259, "8259: Write ICW3 0x%08X\n", ioBusPtr->data);
			curr8259->icw3 = data;
		} else if (curr8259->initreq == 1) {
			if (!(ioBusPtr->addr & 0x000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW4 addr bit 0 should be 1 0x%06X\n", ioBusPtr->addr);
			}
			// Write ICW4
			logmsgf(LOG8259, "8259: Write ICW4 0x%04X\n", ioBusPtr->data);
			curr8259->icw4 = data;
		}
		curr8259->initreq--;
	} else {
		if (ioBusPtr->addr & 0x000001) {
			// Write IMR (OCW1)
			logmsgf(LOG8259, "8259: Write OCW1 0x%04X\n", ioBusPtr->data);
			curr8259->ocw1 = data;
		} else if ((ioBusPtr->data & 0x0018) == 0x00) {
			// Write OCW2
			if ((ioBusPtr->data & OCW2_CMD) == OCW2_CMD_NonSpecEOI) {
				logmsgf(LOG8259, "8259: OCW2 NonSpecific EOI resets highest ISR: 0x%02X\n", curr8259->isr);
				curr8259->freezeIRR = 0;
				curr8259->irr = 0;
				curr8259->intreq = 0;
				if (curr8259->isr & 0x01) {curr8259->isr &= ~0x01; return;}
				if (curr8259->isr & 0x02) {curr8259->isr &= ~0x02; return;}
				if (curr8259->isr & 0x04) {curr8259->isr &= ~0x04; return;}
				if (curr8259->isr & 0x08) {curr8259->isr &= ~0x08; return;}
				if (curr8259->isr & 0x10) {curr8259->isr &= ~0x10; return;}
				if (curr8259->isr & 0x20) {curr8259->isr &= ~0x20; return;}
				if (curr8259->isr & 0x40) {curr8259->isr &= ~0x40; return;}
				if (curr8259->isr & 0x80) {curr8259->isr &= ~0x80; return;}
			} else if ((ioBusPtr->data & OCW2_CMD) == OCW2_CMD_SpecEOI) {
				logmsgf(LOG8259, "8259: OCW2 Specific EOI reset ISR: 0x%02X\n", (~(0x01 << (ioBusPtr->data & OCW2_Level)) & 0xFF));
				curr8259->freezeIRR = 0;
				curr8259->irr = 0;
				curr8259->intreq = 0;
				curr8259->isr &= ~(0x01 << (ioBusPtr->data & OCW2_Level));
			} else {
				logmsgf(LOG8259, "8259: Error not supported OCW2 0x%04X\n", ioBusPtr->data);
			}
		} else if ((ioBusPtr->data & 0x00000018) == 0x08) {
			// Write OCW3
			logmsgf(LOG8259, "8259: Write OCW3 0x%04X\n", ioBusPtr->data);
			curr8259->ocw3 = data;
		} else if ((ioBusPtr->data & 0x00000018) == 0x10) {
			// Write ICW1
			logmsgf(LOG8259, "8259: Write ICW1 0x%04X\n", ioBusPtr->data);
			curr8259->icw1 = data;
		} else {
			logmsgf(LOG8259, "8259: Error invalid write 0x%04X\n", ioBusPtr->addr);
		}
	}
}

void read8259 (struct struct8259* curr8259) {
	if (ioBusPtr->addr & 0x000001) {
		// Read IMR (OCW1)
		logmsgf(LOG8259, "8259: Read IMR (OCW1) 0x%02X\n", curr8259->ocw1);
		ioBusPtr->data = curr8259->ocw1;
	} else if (curr8259->ocw3 & OCW3_PollingCmd) {
		// Poll CMD
		logmsgf(LOG8259, "8259: Poll command EL:0x%02X IL:0x%02X IRR:0x%02X, M:0x%02X\n", curr8259->edgeLatches, curr8259->intLines, curr8259->irr, curr8259->ocw1);
		if (curr8259->intreq) {
			curr8259->freezeIRR = 1;
			uint8_t maskedIrr = (curr8259->irr & ~curr8259->ocw1);
			uint8_t highestInt = 7;
			uint8_t isrToBeSet = 0;
			if (maskedIrr & 0x80) {highestInt = 7; isrToBeSet = 0x80;}
			if (maskedIrr & 0x40) {highestInt = 6; isrToBeSet = 0x40;}
			if (maskedIrr & 0x20) {highestInt = 5; isrToBeSet = 0x20;}
			if (maskedIrr & 0x10) {highestInt = 4; isrToBeSet = 0x10;}
			if (maskedIrr & 0x08) {highestInt = 3; isrToBeSet = 0x08;}
			if (maskedIrr & 0x04) {highestInt = 2; isrToBeSet = 0x04;}
			if (maskedIrr & 0x02) {highestInt = 1; isrToBeSet = 0x02;}
			if (maskedIrr & 0x01) {highestInt = 0; isrToBeSet = 0x01;}
			curr8259->isr |= isrToBeSet;
			curr8259->edgeLatches &= ~isrToBeSet;
			ioBusPtr->data = 0x80 | highestInt;
			curr8259->intreq = 0;
		} else {
			ioBusPtr->data = 0;
		}
		
		logmsgf(LOG8259, "8259: Poll command returned 0x%04X\n", ioBusPtr->data);
		curr8259->ocw3 &= ~OCW3_PollingCmd;
	} else if ((curr8259->ocw3 & OCW3_ReadRegCmd) == 0x02) {
		// Read IRR
		logmsgf(LOG8259, "8259: Read IRR 0x%02X\n", curr8259->irr);
		ioBusPtr->data = curr8259->irr;
	} else if ((curr8259->ocw3 & OCW3_ReadRegCmd) == 0x03) {
		// Read ISR
		logmsgf(LOG8259, "8259: Read ISR 0x%02X\n", curr8259->isr);
		ioBusPtr->data = curr8259->isr;
	} else {
		logmsgf(LOG8259, "8259: Error read mode not configured.\n");
		ioBusPtr->data = 0;
	}
}

void access8259 (struct struct8259* curr8259) {
	//if (!curr8259->reset) {curr8259->initreq = 4; return;}
	if (ioBusPtr->io == 1) {
		if ((ioBusPtr->addr & curr8259->ioAddressMask) == curr8259->ioAddress) {
			if (ioBusPtr->rw) {
				write8259(curr8259);
			} else {
				read8259(curr8259);
			}
			ioBusPtr->cs16 = 0;
		}
	}
}

void cycle8259 (struct struct8259* curr8259) {
	//if (!curr8259->reset) {curr8259->initreq = 4; return;}

	// If IRR isn't frozen, set only if edgeLatch is set (ie hasn't been polled and reset to zero)
	if (!curr8259->freezeIRR) {
		curr8259->irr = curr8259->edgeLatches & curr8259->intLines;
	}

	// Rising edge
	//if (curr8259->intLines & ~curr8259->prevIntLines) {
	if (curr8259->irr & ~curr8259->ocw1) {
		// If any pass our mask, trigger the int pin.
		curr8259->intreq = 1;
		//logmsgf(LOG8259, "8259: Interrupts received 0x%02X\n", curr8259->irr);
	}
	//}

	// Edge latches only get enabled on a falling edge
	if (~curr8259->intLines & curr8259->prevIntLines) {
		curr8259->edgeLatches |= ~curr8259->intLines & curr8259->prevIntLines;
	}
	curr8259->prevIntLines = curr8259->intLines;
}