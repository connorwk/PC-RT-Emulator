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

union MMUIOspace* iommuregs;
uint32_t lockbits = 0;

static const uint32_t specsizelookup[16] = { 0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215};

void rominit (const char *file) {
	FILE *f = fopen(file, "rb");
	rom = (uint8_t*)malloc(ROMSIZE);
	if (f) {
		fread(rom, ROMSIZE, 1, f);
	} else {
		logmsgf(LOGMMU, "MMU: Error could open file (%s) to init proc diag ROM.", file);
	}
}

void mmuinit (uint8_t* memptr) {
	memory = memptr;
	iommuregs = malloc(MMUCONFIGSIZE*4);
	memset(iommuregs->_direct, 0, MMUCONFIGSIZE);
}

int invalidAddrCheck (uint32_t addr, uint32_t end_addr, uint8_t bytes) {
	if (addr >= end_addr && bytes != INST) {
		logmsgf(LOGMEM, "MEM: Error attempted access (0x%08X) outside of valid range (0x%08X)\n", addr, end_addr);
		if (iommuregs->MemException) {iommuregs->MemException |= MERMultExcp;}
		iommuregs->MemException |= MERInvMemAddr;
		if (!(lockbits & MEARLOCKBIT)) {
			iommuregs->MemExceptionAddr = addr;
			lockbits |= MEARLOCKBIT;
		}
		return 0;
	}
	return 1;
}

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode) {
	//logmsgf(LOGMMU, "MMU: Write 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	if (mode == MEMORY) {
		if (((iommuregs->RAMSpec & RAMSPECSize) == 0 && (iommuregs->ROMSpec & ROMSPECSize) == 0) && addr <= MAXREALADDR) {
			memwrite(memory, addr, data, bytes);
		} else if ((addr >= (ROMSPECStartAddr)) && (addr <= ROMSPECEndAddr) && ((iommuregs->ROMSpec & ROMSPECSize) != 0)) {
			logmsgf(LOGMMU, "MMU: Error attempt to write to rom.\n");
			if (iommuregs->MemException) {iommuregs->MemException |= MERMultExcp;}
			iommuregs->MemException |= MERWriteROM;
			if (!(lockbits & MEARLOCKBIT)) {
				iommuregs->MemExceptionAddr = addr;
				lockbits |= MEARLOCKBIT;
			}
		} else if ((addr >= (RAMSPECStartAddr)) && (addr <= RAMSPECEndAddr) && ((iommuregs->RAMSpec & RAMSPECSize) != 0)) {
			if (invalidAddrCheck(addr - (RAMSPECStartAddr), MEMORYSIZE, bytes)) {
				memwrite(memory, addr - (RAMSPECStartAddr), data, bytes);
			}
		} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
			iowrite(addr, data, bytes);
		} else {
			logmsgf(LOGMMU, "MMU: Error Memory write outside valid ranges 0x%08X: 0x%08X\n", addr, data);
		}
	} else {
		// PIO
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			logmsgf(LOGMMU, "MMU: Write to IO Base Addr Reg DIRECT 0x%08X\n", data);
			iommuregs->IOBaseAddr = data;
		} else if (addr >= (iommuregs->IOBaseAddr << 16) && addr < ((iommuregs->IOBaseAddr << 16) + MMUCONFIGSIZE)){
			logmsgf(LOGMMU, "MMU: Write to IOMMU Regs Decoded 0x%08X: 0x%08X\n", addr & 0x0000FFFF, data);
			if ( ((addr & 0x0000FFFF) >= 0x1000) && ((addr & 0x0000FFFF) <= 0x2FFF) ) {
				// Only last two bits of Ref/Change regs are valid
				iommuregs->_direct[addr & 0x0000FFFF] = (data & 0x00000003);
			} else {
				iommuregs->_direct[addr & 0x0000FFFF] = data;
			}
			switch(addr & 0x0000FFFF) {
				case 0x00000080:
					// Invalidate Entire TLB
					break;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO write outside valid ranges 0x%08X: 0x%08X\n", addr, data);
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode) {
	uint32_t data = 0;
	if (mode == MEMORY) {
		if (((iommuregs->RAMSpec & RAMSPECSize) == 0 && (iommuregs->ROMSpec & ROMSPECSize) == 0) && addr <= MAXREALADDR) {
			data = memread(rom, addr & 0x0000FFFF, bytes);
		} else if ((addr >= (ROMSPECStartAddr)) && (addr <= ROMSPECEndAddr) && ((iommuregs->ROMSpec & ROMSPECSize) != 0)) {
			if (invalidAddrCheck((addr - (ROMSPECStartAddr)) & 0x0000FFFF, ROMSIZE, bytes)) {
				data = memread(rom, (addr - (ROMSPECStartAddr)) & 0x0000FFFF, bytes);
			}
		} else if ((addr >= (RAMSPECStartAddr)) && (addr <= RAMSPECEndAddr) && ((iommuregs->RAMSpec & RAMSPECSize) != 0)) {
			if (invalidAddrCheck(addr - (RAMSPECStartAddr), MEMORYSIZE, bytes)) {
				data = memread(memory, addr - (RAMSPECStartAddr), bytes);
			}
		} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
			data = ioread(addr, bytes);
		} else {
			logmsgf(LOGMMU, "MMU: Error Memory read outside valid ranges 0x%08X\n", addr);
			data = 0;
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
			if ((addr & 0x0000FFFF) == 0x00000011) {
				lockbits &= ~MEARLOCKBIT;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO read outside valid ranges 0x%08X\n", addr);
			data = 0;
		}
	}
	//logmsgf(LOGMMU, "MMU: Read 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}