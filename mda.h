// MDA Emulation
#ifndef _MDA
#define _MDA
#include <stdint.h>
#include "iocc.h"

struct structmda {
	uint32_t ioAddress;
	uint32_t ioAddressMask;
	uint32_t memAddress;
	uint32_t memAddressMask;
	uint8_t reset;
	uint8_t ctrlReg;
	uint8_t statusReg;
	uint8_t videoMem[4000];
};

void initMDA (struct structmda* currmda, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask, uint32_t memaddr, uint32_t memaddrMask);
void accessMDA (struct structmda* currmda);
#endif