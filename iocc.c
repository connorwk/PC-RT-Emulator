// IO Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "iocc.h"
#include "mmu.h"
#include "8259a.h"
#include "logfac.h"

struct SysBrdConfig sysbrdcnfg;
struct struct8259 intCtrl1;
struct struct8259 intCtrl2;

struct procBusStruct* procBusPtr;

uint8_t mdaVideoMem[4000];
uint8_t mdaCtrlReg;
uint8_t mdaStatusReg;

uint8_t CSRlocked;

void ioinit (struct procBusStruct* procBusPointer) {
	procBusPtr = procBusPointer;
	//Any Init required
	sysbrdcnfg.CSR = CSR_ReservedBits;
	sysbrdcnfg.MemCnfgReg = MEMCNFG_Reserved;
	init8259(&intCtrl1, procBusPointer);
	init8259(&intCtrl2, procBusPointer);
}

uint8_t* getMDAPtr (void) {
	return &mdaVideoMem[0];
}

void iocycle (void) {
	if (sysbrdcnfg.DIAReg) {
		intCtrl1.intlines = 0xFF;
		intCtrl2.intlines = 0xFF;
	} else {
		intCtrl1.intlines = 0;
		intCtrl2.intlines = 0;
	}

	cycle8259(&intCtrl1);
	cycle8259(&intCtrl2);

	if (intCtrl1.intreq) { procBusPtr->intrpt |= INTRPT_3_IOChan; }
	if (intCtrl2.intreq) { procBusPtr->intrpt |= INTRPT_4_IOChan; }
}

void ioput(uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes) {
	switch (bytes) {
		case WIDTH_BYTE:
			ptr[addr ^ 0x00000001] = data;
			break;
		case WIDTH_HALFWORD:
			ptr[addr & 0xFFFFFFFE] = (data & 0x0000FF00) >> 8;
			ptr[(addr & 0xFFFFFFFE) + 1] = (data & 0x000000FF);
			break;
		case WIDTH_WORD:
			ptr[addr & 0xFFFFFFFC] = (data & 0xFF000000) >> 24;
			ptr[addr & 0xFFFFFFFC + 1] = (data & 0x00FF0000) >> 16;
			ptr[addr & 0xFFFFFFFC + 2] = (data & 0x0000FF00) >> 8;
			ptr[(addr & 0xFFFFFFFC) + 3] = (data & 0x000000FF);
			break;
	}
}

uint32_t ioget(uint8_t* ptr, uint32_t addr, uint8_t bytes) {
	uint32_t data;
	switch (bytes) {
		case WIDTH_BYTE:
			data = ptr[addr ^ 0x00000001];
			break;
		case WIDTH_HALFWORD:
			data = ptr[addr & 0xFFFFFFFE] << 8 | ptr[(addr & 0xFFFFFFFE) + 1];
			break;
		case WIDTH_WORD:
			data = (ptr[addr & 0xFFFFFFFC] << 24) | (ptr[addr & 0xFFFFFFFC + 1] << 16) | (ptr[addr & 0xFFFFFFFC + 2] << 8) | (ptr[(addr & 0xFFFFFFFC) + 3]);
			break;
	}
	return data;
}

void iowrite (uint32_t addr, uint32_t data, uint8_t bytes) {
	if (addr == 0xF00003B8) {
		// Write-only
		mdaCtrlReg = data;
	} else if (addr >= 0xF0008000 && addr <= 0xF0008003) {
		sysbrdcnfg.Z8530[addr & 0x00000003] = data;
	} else if (addr >= 0xF0008020 && addr <= 0xF0008023) {
		sysbrdcnfg.CHAExtReg = data;
	} else if (addr >= 0xF0008040 && addr <= 0xF0008043) {
		sysbrdcnfg.CHBExtReg = data;
	} else if (addr >= 0xF0008060 && addr <= 0xF0008063) {
		logmsgf(LOGIO, "IO: Error can't write to Z8530 Intack Reg 0x%08X\n", addr);
	} else if (addr >= 0xF00080E0 && addr <= 0xF00080E3) {
		logmsgf(LOGIO, "IO: IO Delay\n");
		//sysbrdcnfg.IODelayReg = data;
	} else if (addr >= 0xF0008400 && addr <= 0xF000847F) {
		sysbrdcnfg.KeyboardAdpt[addr & 0x00000007] = data;
	} else if (addr >= 0xF0008800 && addr <= 0xF000883F) {
		sysbrdcnfg.RTC[addr & 0x0000003F] = data;
	} else if (addr >= 0xF0008840 && addr <= 0xF000885F) {
		sysbrdcnfg.P8237_1[addr & 0x0000000F] = data;
	} else if (addr >= 0xF0008860 && addr <= 0xF000887F) {
		sysbrdcnfg.P8237_2[addr & 0x0000000F] = data;
	} else if (addr >= 0xF0008880 && addr <= 0xF000889F) {
		logmsgf(LOGIO, "IO: Interrupt Controller 1\n");
		access8259(&intCtrl1);
	} else if (addr >= 0xF00088A0 && addr <= 0xF00088BF) {
		logmsgf(LOGIO, "IO: Interrupt Controller 2\n");
		access8259(&intCtrl2);
	} else if (addr >= 0xF00088C0 && addr <= 0xF00088DF) {
		sysbrdcnfg.DMARegDBR = data;
	} else if (addr >= 0xF00088E0 && addr <= 0xF00088FF) {
		sysbrdcnfg.DMARegDMR = data;
	} else if (addr >= 0xF0008C00 && addr <= 0xF0008C03) {
		sysbrdcnfg.CH8EnReg = data;
	} else if (addr >= 0xF0008C20 && addr <= 0xF0008C23) {
		sysbrdcnfg.CtrlRegCCR = data;
	} else if (addr >= 0xF0008C40 && addr <= 0xF0008C43) {
		sysbrdcnfg.CRRAReg = data;
	} else if (addr >= 0xF0008C60 && addr <= 0xF0008C63) {
		sysbrdcnfg.CRRBReg = data;
	} else if (addr >= 0xF0008C80 && addr <= 0xF0008C83) {
		logmsgf(LOGIO, "IO: Error can't write to Memory Config Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0008CA0 && addr <= 0xF0008CA3) {
		// Diagnostic Interrupt Activate Register pg. 5-58
		sysbrdcnfg.DIAReg = data;
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF && addr & 0x00000001) {
		logmsgf(LOGIO, "IO: Error unaligned read from TCW 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF) {
		sysbrdcnfg.TCW[(addr & 0x000007FE) >> 1] = data;
	} else if (addr >= 0xF0010800 && addr <= 0xF0010FFF) {
		// TODO Read or Written ONLY by the processor board
		sysbrdcnfg.CSR = data | CSR_ReservedBits;
	} else {
		logmsgf(LOGIO, "IO: Error IO write to non-existant addr 0x%08X: 0x%08X\n", addr, data);
	}
	logmsgf(LOGIO, "IO: IO Write 0x%08X : 0x%08X\n", addr, data);
}

uint32_t ioread (uint32_t addr, uint8_t bytes) {
	uint32_t data = 0;
	if (addr == 0xF00003BA) {
		// Read-only
		data = mdaStatusReg;
	} else if (addr >= 0xF0008000 && addr <= 0xF0008003) {
		data = sysbrdcnfg.Z8530[addr & 0x00000003];
	} else if (addr >= 0xF0008020 && addr <= 0xF0008023) {
		data = sysbrdcnfg.CHAExtReg;
	} else if (addr >= 0xF0008040 && addr <= 0xF0008043) {
		data = sysbrdcnfg.CHBExtReg;
	} else if (addr >= 0xF0008060 && addr <= 0xF0008063) {
		data = sysbrdcnfg.Z8530Intack;
	} else if (addr >= 0xF00080E0 && addr <= 0xF00080E3) {
		data = 0x000000FF;
		logmsgf(LOGIO, "IO: Error can't read from I/O Delay Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0008400 && addr <= 0xF000847F) {
		data = sysbrdcnfg.KeyboardAdpt[addr & 0x00000007];
	} else if (addr >= 0xF0008800 && addr <= 0xF000883F) {
		data = sysbrdcnfg.RTC[addr & 0x0000003F];
	} else if (addr >= 0xF0008840 && addr <= 0xF000885F) {
		data = sysbrdcnfg.P8237_1[addr & 0x0000000F];
	} else if (addr >= 0xF0008860 && addr <= 0xF000887F) {
		data = sysbrdcnfg.P8237_2[addr & 0x0000000F];
	} else if (addr >= 0xF0008880 && addr <= 0xF000889F) {
		logmsgf(LOGIO, "IO: Interrupt Controller 1\n");
		access8259(&intCtrl1);
	} else if (addr >= 0xF00088A0 && addr <= 0xF00088BF) {
		logmsgf(LOGIO, "IO: Interrupt Controller 2\n");
		access8259(&intCtrl2);
	} else if (addr >= 0xF00088C0 && addr <= 0xF00088DF) {
		data = sysbrdcnfg.DMARegDBR;
	} else if (addr >= 0xF00088E0 && addr <= 0xF00088FF) {
		data = sysbrdcnfg.DMARegDMR;
	} else if (addr >= 0xF0008C00 && addr <= 0xF0008C03) {
		data = sysbrdcnfg.CH8EnReg;
	} else if (addr >= 0xF0008C20 && addr <= 0xF0008C23) {
		data = sysbrdcnfg.CtrlRegCCR;
	} else if (addr >= 0xF0008C40 && addr <= 0xF0008C43) {
		data = sysbrdcnfg.CRRAReg;
	} else if (addr >= 0xF0008C60 && addr <= 0xF0008C63) {
		data = sysbrdcnfg.CRRBReg;
	} else if (addr >= 0xF0008C80 && addr <= 0xF0008C83) {
		data = sysbrdcnfg.MemCnfgReg;
	} else if (addr >= 0xF0008CA0 && addr <= 0xF0008CA3) {
		data = 0x000000FF;
		logmsgf(LOGIO, "IO: Error can't read from DIA Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF && addr & 0x00000001) {
		logmsgf(LOGIO, "IO: Error unaligned read from TCW 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF) {
		data = sysbrdcnfg.TCW[(addr & 0x000007FE) >> 1];
	} else if (addr >= 0xF0010800 && addr <= 0xF0010FFF) {
		// TODO Read or Written ONLY by the processor board
		data = sysbrdcnfg.CSR;
	} else {
		logmsgf(LOGIO, "IO: Error IO read to non-existant addr 0x%08X\n", addr);
	}
	logmsgf(LOGIO, "IO: IO Read  0x%08X : 0x%08X\n", addr, data);
	return data;
}

void iomemwrite (uint32_t addr, uint32_t data, uint8_t bytes) {
	logmsgf(LOGIO, "IO: IO Mem Write 0x%08X : 0x%08X\n", addr, data);
	if (addr >= 0x0B0000 && addr <= 0x0B0F9F) {
		ioput(mdaVideoMem, addr & 0x000FFF, data, bytes);
	} else {
		logmsgf(LOGIO, "IO: Error IO Mem write to non-existant addr 0x%08X: 0x%08X\n", addr, data);
	}
}

uint32_t iomemread (uint32_t addr, uint8_t bytes) {
	uint32_t data = 0;
	if (addr >= 0x0B0000 && addr <= 0x0B0F9F) {
		data = ioget(mdaVideoMem, addr & 0x000FFF, bytes);
	} else {
		logmsgf(LOGIO, "IO: Error IO Mem read to non-existant addr 0x%08X\n", addr);
	}
	logmsgf(LOGIO, "IO: IO Mem Read  0x%08X : 0x%08X\n", addr, data);
	return data;
}