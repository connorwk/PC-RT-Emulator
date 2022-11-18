// IO Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "io.h"
#include "memory.h"
#include "logfac.h"

struct SysBrdConfig sysbrdcnfg;

void ioinit (void) {
	//Any Init required
}

void iowrite (uint32_t addr, uint32_t data, uint8_t bytes) {
	if (addr >= 0xF0008000 && addr <= 0xF0008003) {
		sysbrdcnfg.Z8530[addr & 0x00000003] = data;
	} else if (addr >= 0xF0008020 && addr <= 0xF0008023) {
		sysbrdcnfg.CHAExtReg = data;
	} else if (addr >= 0xF0008040 && addr <= 0xF0008043) {
		sysbrdcnfg.CHBExtReg = data;
	} else if (addr >= 0xF0008060 && addr <= 0xF0008063) {
		logmsgf(LOGMMU, "IO: Error can't write to Z8530 Intack Reg 0x%08X\n", addr);
	} else if (addr >= 0xF00080E0 && addr <= 0xF00080E3) {
		sysbrdcnfg.IODelayReg = data;
	} else if (addr >= 0xF0008400 && addr <= 0xF000847F) {
		sysbrdcnfg.KeyboardAdpt[addr & 0x00000007] = data;
	} else if (addr >= 0xF0008800 && addr <= 0xF000883F) {
		sysbrdcnfg.RTC[addr & 0x0000003F] = data;
	} else if (addr >= 0xF0008840 && addr <= 0xF000885F) {
		sysbrdcnfg.P8237_1[addr & 0x0000000F] = data;
	} else if (addr >= 0xF0008860 && addr <= 0xF000887F) {
		sysbrdcnfg.P8237_2[addr & 0x0000000F] = data;
	} else if (addr >= 0xF0008880 && addr <= 0xF000889F) {
		sysbrdcnfg.P8259_1[addr & 0x00000001] = data;
	} else if (addr >= 0xF00088A0 && addr <= 0xF00088BF) {
		sysbrdcnfg.P8259_2[addr & 0x00000001] = data;
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
		logmsgf(LOGMMU, "IO: Error can't write to Memory Config Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0008CA0 && addr <= 0xF0008CA3) {
		sysbrdcnfg.DIAReg = data;
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF && addr & 0x00000001) {
		logmsgf(LOGMMU, "IO: Error unaligned read from TCW 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF) {
		sysbrdcnfg.TCW[(addr & 0x000007FE) >> 1] = data;
	} else if (addr >= 0xF0010800 && addr <= 0xF0010FFF) {
		sysbrdcnfg.CSR = data;
	} else {
		logmsgf(LOGMMU, "IO: Error IO write to non-existant IO addr 0x%08X: 0x%08X\n", addr, data);
	}
}

uint32_t ioread (uint32_t addr, uint8_t bytes) {
	uint32_t data;
	if (addr >= 0xF0008000 && addr <= 0xF0008003) {
		data = sysbrdcnfg.Z8530[addr & 0x00000003];
	} else if (addr >= 0xF0008020 && addr <= 0xF0008023) {
		data = sysbrdcnfg.CHAExtReg;
	} else if (addr >= 0xF0008040 && addr <= 0xF0008043) {
		data = sysbrdcnfg.CHBExtReg;
	} else if (addr >= 0xF0008060 && addr <= 0xF0008063) {
		data = sysbrdcnfg.Z8530Intack;
	} else if (addr >= 0xF00080E0 && addr <= 0xF00080E3) {
		data = 0x000000FF;
		logmsgf(LOGMMU, "IO: Error can't read from I/O Delay Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0008400 && addr <= 0xF000847F) {
		data = sysbrdcnfg.KeyboardAdpt[addr & 0x00000007];
	} else if (addr >= 0xF0008800 && addr <= 0xF000883F) {
		data = sysbrdcnfg.RTC[addr & 0x0000003F];
	} else if (addr >= 0xF0008840 && addr <= 0xF000885F) {
		data = sysbrdcnfg.P8237_1[addr & 0x0000000F];
	} else if (addr >= 0xF0008860 && addr <= 0xF000887F) {
		data = sysbrdcnfg.P8237_2[addr & 0x0000000F];
	} else if (addr >= 0xF0008880 && addr <= 0xF000889F) {
		data = sysbrdcnfg.P8259_1[addr & 0x00000001];
	} else if (addr >= 0xF00088A0 && addr <= 0xF00088BF) {
		data = sysbrdcnfg.P8259_2[addr & 0x00000001];
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
		logmsgf(LOGMMU, "IO: Error can't read from DIA Reg 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF && addr & 0x00000001) {
		logmsgf(LOGMMU, "IO: Error unaligned read from TCW 0x%08X\n", addr);
	} else if (addr >= 0xF0010000 && addr <= 0xF00107FF) {
		data = sysbrdcnfg.TCW[(addr & 0x000007FE) >> 1];
	} else if (addr >= 0xF0010800 && addr <= 0xF0010FFF) {
		data = sysbrdcnfg.CSR;
	} else {
		logmsgf(LOGMMU, "IO: Error IO read to non-existant IO addr 0x%08X\n", addr);
	}
	return data;
}