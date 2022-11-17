// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"
#include "memory.h"
#include "io.h"
#include "logfac.h"

uint8_t* memory;
uint8_t* rom;
#define ROMSIZE 65536
union MMUIOspace* iommuregs;

static const uint32_t specsizelookup[16] = { 0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215};

void rominit (const char *file) {
	FILE *f = fopen(file, "rb");
	rom = (uint8_t*)malloc(ROMSIZE);
	if (f) {
		fread(rom, ROMSIZE, 1, f);
	} else {
		logmsgf(LOGMMU, "ERROR: Could open file (%s) to init proc diag ROM.", file);
	}
}

void mmuinit (uint8_t* memptr) {
	memory = memptr;
	iommuregs = (uint8_t*)malloc(MMUCONFIGSIZE);
	memset(iommuregs->_direct, 0, MMUCONFIGSIZE);
}

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode) {
	//logmsgf(LOGMMU, "MMU: Write 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	if (mode == MEMORY) {
		if ((iommuregs->RAMSpec & RAMSPECSize) == 0 || (iommuregs->ROMSpec & ROMSPECSize) == 0) {
			memwrite(memory, addr, data, bytes);
		} else if ((addr >= ROMSPECStartAddr) && (addr <= ROMSPECEndAddr)) {
			// TODO: Set some exception reg?
			logmsgf(LOGMMU, "ERROR: Attempt to write to rom.\n");
		} else if ((addr >= RAMSPECStartAddr) && (addr <= RAMSPECEndAddr)) {
			memwrite(memory, addr - RAMSPECStartAddr, data, bytes);
		} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
			iowrite(addr, data, bytes);
		}
	} else {
		// PIO
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			logmsgf(LOGMMU, "MMU: Write to IO Base Addr Reg DIRECT 0x%08X\n", data);
			iommuregs->IOBaseAddr = data;
		} else if (addr >= (iommuregs->IOBaseAddr << 16) && addr < ((iommuregs->IOBaseAddr << 16) + MMUCONFIGSIZE)){
			logmsgf(LOGMMU, "MMU: Write to IOMMU Regs Decoded 0x%08X: 0x%08X\n", addr & 0x0000FFFF, data);
			iommuregs->_direct[addr & 0x0000FFFF] = data;
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode) {
	uint32_t data;
	if (mode == MEMORY) {
		if ((iommuregs->RAMSpec & RAMSPECSize) == 0 || (iommuregs->ROMSpec & ROMSPECSize) == 0) {
			data = memread(rom, addr & 0x0000FFFF, bytes);
		} else if ((addr >= ROMSPECStartAddr) && (addr <= ROMSPECEndAddr)) {
			data = memread(rom, addr - ROMSPECStartAddr, bytes);
		} else if ((addr >= RAMSPECStartAddr) && (addr <= RAMSPECEndAddr)) {
			data = memread(memory, addr - RAMSPECStartAddr, bytes);
		} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
			data = ioread(addr, bytes);
		}
	} else {
		// PIO
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			data = iommuregs->IOBaseAddr;
			logmsgf(LOGMMU, "MMU: Read from IO Base Addr Reg DIRECT 0x%08X\n", data);
		} else if (addr >= (iommuregs->IOBaseAddr << 16) && addr < ((iommuregs->IOBaseAddr << 16) + MMUCONFIGSIZE)){
			data = iommuregs->_direct[addr & 0x0000FFFF];
			logmsgf(LOGMMU, "MMU: Read from IOMMU Regs Decoded 0x%08X: 0x%08X\n", addr & 0x0000FFFF, data);
		}
	}
	//logmsgf(LOGMMU, "MMU: Read 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}