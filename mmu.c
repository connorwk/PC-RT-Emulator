// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <mmu.h>
#include <memory.h>
#include <logfac.h>

uint8_t* memory;
uint8_t* rom;
#define ROMSIZE 65536
union MMUIOspace* iommuregs;

static const uint32_t specsizelookup[16] { 0, 65536, 65536, 65536, 65536, 65536, 65536, 65536, 131072, 262144, 524288, 1048576, 2097152, 4194304, 8388608, 16777216}

void rominit(const char *file) {
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
	if (mode == MEMORY) {
		if (iommuregs->RAMSpec & RAMSPECSize == 0 || iommuregs->ROMSpec & ROMSPECSize == 0) {
			memwrite(memory, addr, data, bytes);
		} else if (addr >= ROMSPECStartAddr && addr < ROMSPECEndAddr) {
			// TODO: Set some exception reg?
			logmsgf(LOGMMU, "ERROR: Attempt to write to rom.");
		} else if (addr >= RAMSPECStartAddr && addr < RAMSPECEndAddr) {
			memwrite(memory, addr, data, bytes);
		}
	} else {
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			memwrite(iommuregs->IOBaseAddr, 0, data, bytes);
		} else if (addr > iommuregs->IOBaseAddr && addr < iommuregs->IOBaseAddr+MMUCONFIGSIZE){
			memwrite(iommuregs->_direct, addr, data, bytes);
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode) {
	uint32_t data;
	if (mode == MEMORY) {
		if (iommuregs->RAMSpec & RAMSPECSize == 0 || iommuregs->ROMSpec & ROMSPECSize == 0) {
			data = memread(rom, addr, bytes);
		} else if (addr >= ROMSPECStartAddr && addr < ROMSPECEndAddr) {
			data = memread(rom, addr, bytes);
		} else if (addr >= RAMSPECStartAddr && addr < RAMSPECEndAddr) {
			data = memread(memory, addr, bytes);
		}
	} else {
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			data = memread(iommuregs->IOBaseAddr, 0, bytes);
		} else if (addr >= iommuregs->IOBaseAddr && addr < iommuregs->IOBaseAddr+MMUCONFIGSIZE){
			data = memread(iommuregs->_direct, addr, bytes);
		}
	}
	return data;
}