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

struct procBusStruct* procBusPtr;

void init8259(struct struct8259* curr8259, struct procBusStruct* procBusPointer) {
	curr8259->initreq = 4;
	procBusPtr = procBusPointer;
}

void write8259(struct struct8259* curr8259) {
	uint8_t data = (procBusPtr->data & 0x000000FF);
	if (curr8259->initreq) {
		//logmsgf(LOG8259, "8259: Initreq 0x%08X\n", curr8259->initreq);
		if (curr8259->initreq == 4) {
			if (procBusPtr->addr & 0x00000001) {
				logmsgf(LOG8259, "8259: Error writing ICW1 addr bit 0 should be 0 0x%08X\n", procBusPtr->addr);
			}
			// Write ICW1
			logmsgf(LOG8259, "8259: Write ICW1 0x%08X\n", procBusPtr->data);
			curr8259->icw1 = data;
			// Reset some other stuff
			curr8259->ocw3 = 0x4A;
		} else if (curr8259->initreq == 3) {
			if (!(procBusPtr->addr & 0x00000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW2 addr bit 0 should be 1 0x%08X\n", procBusPtr->addr);
			}
			// Write ICW2
			logmsgf(LOG8259, "8259: Write ICW2 0x%08X\n", procBusPtr->data);
			curr8259->icw2 = data;

			if (curr8259->icw1 & ICW1_SingleMode) {
				curr8259->initreq--;
			}
			if (!(curr8259->icw1 & ICW1_ICW4Needed)) {
				curr8259->initreq--;
			}
		} else if (curr8259->initreq == 2) {
			if (!(procBusPtr->addr & 0x00000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW3 addr bit 0 should be 1 0x%08X\n", procBusPtr->addr);
			}
			// Write ICW3
			logmsgf(LOG8259, "8259: Write ICW3 0x%08X\n", procBusPtr->data);
			curr8259->icw3 = data;
		} else if (curr8259->initreq == 1) {
			if (!(procBusPtr->addr & 0x00000001)) {
				logmsgf(LOG8259, "8259: Error writing ICW4 addr bit 0 should be 1 0x%08X\n", procBusPtr->addr);
			}
			// Write ICW4
			logmsgf(LOG8259, "8259: Write ICW4 0x%08X\n", procBusPtr->data);
			curr8259->icw4 = data;
		}
		curr8259->initreq--;
	} else {
		if (procBusPtr->addr & 0x00000001) {
			// Write IMR (OCW1)
			logmsgf(LOG8259, "8259: Write OCW1 0x%08X\n", procBusPtr->data);
			curr8259->ocw1 = data;
		} else if ((procBusPtr->data & 0x00000018) == 0x00) {
			// Write OCW2
			if ((procBusPtr->data & OCW2_CMD) == OCW2_CMD_NonSpecEOI) {
				logmsgf(LOG8259, "8259: OCW2 NonSpecific EOI reset IRR: 0x%02X\n", ~curr8259->isr);
				curr8259->irr &= ~curr8259->isr;
				curr8259->isr = 0;
			} else if ((procBusPtr->data & OCW2_CMD) == OCW2_CMD_SpecEOI) {
				logmsgf(LOG8259, "8259: OCW2 Specific EOI reset IRR: 0x%02X\n", ~(0x80 >> (procBusPtr->data & OCW2_Level)));
				curr8259->irr &= ~(0x80 >> (procBusPtr->data & OCW2_Level));
				curr8259->isr = 0;
			} else {
				logmsgf(LOG8259, "8259: Error not supported OCW2 0x%08X\n", procBusPtr->data);
			}
		} else if ((procBusPtr->data & 0x00000018) == 0x08) {
			// Write OCW3
			logmsgf(LOG8259, "8259: Write OCW3 0x%08X\n", procBusPtr->data);
			curr8259->ocw3 = data;
		} else {
			logmsgf(LOG8259, "8259: Error invalid write 0x%08X\n", procBusPtr->addr);
		}
	}
}

void read8259(struct struct8259* curr8259) {
	if (procBusPtr->addr & 0x00000001) {
		// Read IMR (OCW1)
		logmsgf(LOG8259, "8259: Read IMR (OCW1) 0x%08X\n", curr8259->ocw1);
		procBusPtr->data = curr8259->ocw1;
	} else if (curr8259->ocw3 & OCW3_PollingCmd) {
		// Poll CMD
		if (curr8259->irr & ~curr8259->ocw1) {
			uint8_t highestInt;
			if (curr8259->irr == 0x80) {highestInt = 7; curr8259->isr = 0x80;}
			if (curr8259->irr == 0x40) {highestInt = 6; curr8259->isr = 0x40;}
			if (curr8259->irr == 0x20) {highestInt = 5; curr8259->isr = 0x20;}
			if (curr8259->irr == 0x10) {highestInt = 4; curr8259->isr = 0x10;}
			if (curr8259->irr == 0x08) {highestInt = 3; curr8259->isr = 0x08;}
			if (curr8259->irr == 0x04) {highestInt = 2; curr8259->isr = 0x04;}
			if (curr8259->irr == 0x02) {highestInt = 1; curr8259->isr = 0x02;}
			if (curr8259->irr == 0x01) {highestInt = 0; curr8259->isr = 0x01;}
			procBusPtr->data = 0x80 | highestInt;
			logmsgf(LOG8259, "8259: Poll command 0x%08X\n", procBusPtr->data);
		}
		curr8259->ocw3 &= ~OCW3_PollingCmd;
	} else if ((curr8259->ocw3 & OCW3_ReadRegCmd) == 0x02) {
		// Read IRR
		logmsgf(LOG8259, "8259: Read IRR 0x%08X\n", curr8259->irr);
		procBusPtr->data = curr8259->irr;
	} else if ((curr8259->ocw3 & OCW3_ReadRegCmd) == 0x03) {
		// Read ISR
		logmsgf(LOG8259, "8259: Read ISR 0x%08X\n", curr8259->isr);
		procBusPtr->data = curr8259->isr;
	}
}

void access8259 (struct struct8259* curr8259) {
	//if (!curr8259->reset) {curr8259->initreq = 4; return;}
	if (procBusPtr->rw) {
		write8259(curr8259);
	} else {
		read8259(curr8259);
	}
}

void cycle8259 (struct struct8259* curr8259) {
	//if (!curr8259->reset) {curr8259->initreq = 4; return;}
	if (curr8259->intlines & ~curr8259->ocw1) {
		curr8259->irr = curr8259->intlines & ~curr8259->ocw1;
		curr8259->intlines = 0;
	}

	if (curr8259->irr) {
		curr8259->intreq = 1;
	} else {
		curr8259->intreq = 0;
	}
}