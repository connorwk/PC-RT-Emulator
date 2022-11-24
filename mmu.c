// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"
#include "romp.h"
#include "memory.h"
#include "io.h"
#include "logfac.h"

uint8_t* memory;
uint8_t* rom;

union MMUIOspace* iommuregs;
uint32_t lockbits = 0;
uint32_t* ICSptr;
uint8_t lastUsedTLB[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

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

void mmuinit (uint8_t* memptr, uint32_t* SCRICSptr) {
	memory = memptr;
	iommuregs = malloc(MMUCONFIGSIZE*4);
	memset(iommuregs->_direct, 0, MMUCONFIGSIZE);
	ICSptr = SCRICSptr;
}

int invalidAddrCheck (uint32_t addr, uint32_t end_addr, uint8_t bytes) {
	if ((addr >= end_addr || (addr == (end_addr - 1) && bytes > BYTE) || (addr == (end_addr - 2) && bytes > HALFWORD) || (addr == (end_addr - 3) && bytes > HALFWORD)) && bytes != INST) {
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

void realwrite (uint32_t addr, uint32_t data, uint8_t bytes) {
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
}

uint32_t realread (uint32_t addr, uint8_t bytes) {
	uint32_t data;
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
	return data;
}

/*
void reloadTLB () {
	
}
*/

uint32_t checkTLB (uint32_t segID, int	segIDoffset, uint32_t virtPageIdx, uint32_t pageDisp, uint8_t TLBNum) {
	uint32_t realAddr;
	if (iommuregs->TLB0_AddrTagField[TLBNum] == ((segID << segIDoffset) & (virtPageIdx & 0x0000FFF0))) {
		if (iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 0;
			realAddr = ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & TLBRealPgNum4K) << 16) & pageDisp;
		} else {
			logmsgf(LOGMMU, "MMU: TLB0 Invalid. Loading TLB0.\n");
		}
	} else if (iommuregs->TLB1_AddrTagField[TLBNum] == ((segID << segIDoffset) & (virtPageIdx & 0xFFFFFFF0))) {
		if (iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 1;
			realAddr = ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & TLBRealPgNum4K) << 16) & pageDisp;
		} else {
			logmsgf(LOGMMU, "MMU: TLB1 Invalid. Loading TLB1.\n");
		}
	} else {
		logmsgf(LOGMMU, "MMU: Both TLB0 and TLB1 are stale. ");
		if (lastUsedTLB[TLBNum]) {
			logmsgf(LOGMMU, "Reloading TLB0.\n");
			lastUsedTLB[TLBNum] = 0;

		} else {
			logmsgf(LOGMMU, "Reloading TLB1.\n");
			lastUsedTLB[TLBNum] = 1;

		}
	}

	return realAddr;
}

uint32_t translateaddr(uint32_t addr, uint32_t segment) {
	uint32_t segID;
	int	segIDoffset;
	uint32_t virtPageIdx;
	uint32_t pageDisp;
	uint8_t TLBNum;
	uint32_t realAddr;
	
	if ((iommuregs->TranslationCtrl & TRANSCTRLSegReg0VirtEqReal) && (addr & 0xF0000000) == 0) {
		return addr & 0x00FFFFFF;
	} else if (iommuregs->TranslationCtrl & TRANSCTRLPageSize) {
		// 4K Pages
		segID = (segment & SEGREGSegID) >> 2;
		segIDoffset = 16;
		virtPageIdx = (addr & 0x0FFFF000) >> 12;
		pageDisp = addr & 0x00000FFF;
		TLBNum = virtPageIdx & 0x0000000F;
		
		realAddr = checkTLB(segID, segIDoffset, virtPageIdx, pageDisp, TLBNum);
	} else {
		// 2K Pages
		segID = (segment & SEGREGSegID) >> 2;
		segIDoffset = 15;
		virtPageIdx = (addr & 0x0FFFF800) >> 11;
		pageDisp = addr & 0x000007FF;
		TLBNum = virtPageIdx & 0x0000000F;

		realAddr = checkTLB(segID, segIDoffset, virtPageIdx, pageDisp, TLBNum);
	}
	return realAddr;
}

uint32_t translatecheck(uint32_t addr, uint8_t tag) {
	uint32_t realAddr;
	uint32_t segment = iommuregs->_direct[(addr & 0xF0000000) >> 28];
	
	if (segment & SEGREGPresent) {
		if ((segment & SEGREGProcAcc) && tag != TAG_PROC) {
			realAddr = translateaddr(addr, segment);
		} else {
			// Segment protected from PROC access
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from Processor accesses.\n", (addr & 0xF0000000) >> 28);
		}

		if ((segment & SEGREGIOAcc) && tag != TAG_IO) {
			realAddr = translateaddr(addr, segment);
		} else {
			// Segment protected from IO access
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from I/O accesses.\n", (addr & 0xF0000000) >> 28);
		}
	} else {
		// Segment disabled.
		// Supposedly we just ignore the request allowing multiple MMUs/devices on the Proc channel.
		// But I assume that some failure of an access happens on the processor side... TODO
		logmsgf(LOGMMU, "MMU: Error Segment %d is disabled.\n", (addr & 0xF0000000) >> 28);
	}
	return realAddr;
}

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode, uint8_t tag) {
	//logmsgf(LOGMMU, "MMU: Write 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	uint32_t segment;
	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			realwrite(translatecheck(addr, tag), data, bytes);
		} else {
			realwrite(addr, data, bytes);
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
					for (int i=0; i < 16; i++) {
						iommuregs->TLB0_RealPageNum_VBs_KBs[i] &= ~TLBValidBit;
						iommuregs->TLB1_RealPageNum_VBs_KBs[i] &= ~TLBValidBit;
					}
					break;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO write outside valid ranges 0x%08X: 0x%08X\n", addr, data);
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode, uint8_t tag) {
	uint32_t data = 0;
	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			data = realread(translatecheck(addr, tag), bytes);
		} else {
			data = realread(addr, bytes);
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