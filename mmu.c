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

static const uint32_t specsizelookup[16] = {0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215};
// Used in HAT/IPT Base address calculation, the value is how far left to shift and thus "multiply" the result
static const uint32_t HATIPTBaseAddrMultLookup[32] = {0, 9, 9, 9, 9, 9, 9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 8, 8, 8, 8, 8, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static const uint32_t HATAddrGenMask[32] = {0, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF};

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

uint32_t findIPT (uint32_t genAddrTag, uint32_t TLBNum, uint32_t virtPageIdx, uint32_t segment) {
	uint32_t segID = (segment & SEGREGSegID) >> 2;
	int page4k = (iommuregs->TranslationCtrl & TRANSCTRLPageSize) ? 1 : 0;
	uint32_t realPgMask = page4k? TLBRealPgNum4K : TLBRealPgNum2K;
	uint32_t lookupIdx = ((iommuregs->TranslationCtrl & TRANSCTRLPageSize) >> 4) | (iommuregs->RAMSpec & RAMSPECSize);
	uint32_t baseAddrHATIPT = page4k ? (iommuregs->TranslationCtrl & TRANSCTRLHATIPTBaseAddr4K) : (iommuregs->TranslationCtrl & TRANSCTRLHATIPTBaseAddr2K);
	baseAddrHATIPT = baseAddrHATIPT << HATIPTBaseAddrMultLookup[lookupIdx];
	uint32_t effectiveAddrBits = page4k ? (virtPageIdx >> 1) : virtPageIdx;
	uint32_t offsetHAT = ((effectiveAddrBits ^ segID) & HATAddrGenMask[lookupIdx]) << 4;

	uint32_t HATentry;
	uint32_t IPTentry;
	uint32_t Ctrlentry;
	uint32_t HATIPTaddr = baseAddrHATIPT + offsetHAT;
	
	HATentry = realread(HATIPTaddr | 0x4, WORD);
	if (HATentry & HATIPT_EmptyBit) {
		// TODO: Page Fault Mem Except Reg pg. 11-119
	} else {
		HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_HATPtr) >> 12);
		IPTentry = realread(HATIPTaddr, WORD);
		HATentry = realread(HATIPTaddr | 0x4, WORD);
		while (1) {
			if ((IPTentry & HATIPT_AddrTag) == (genAddrTag | TLBNum)) {
				// TLB Reload
				logmsgf(LOGMMU, "MMU: IPT Entry found at: 0x%08X\n", HATIPTaddr);
				if (lastUsedTLB[TLBNum]) {
					logmsgf(LOGMMU, "MMU: Reloading TLB0.\n");
					lastUsedTLB[TLBNum] = 0;
					iommuregs->TLB0_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB0_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x4, WORD);
					}
					return ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
				} else {
					logmsgf(LOGMMU, "MMU: Reloading TLB1.\n");
					lastUsedTLB[TLBNum] = 1;
					iommuregs->TLB1_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB1_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x4, WORD);
					}
					return ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
				}
			} else {
				if (HATentry & HATIPT_LastBit) {
					// TODO: Page Fault Mem Except Reg pg. 11-119
					break;
				} else {
					HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_IPTPtr) << 4);
					IPTentry = realread(HATIPTaddr, WORD);
					HATentry = realread(HATIPTaddr | 0x4, WORD);
				}
			}
		};
	}
	return 0xFFFFFFFF;
}

uint32_t checkTLB (uint32_t segment, uint32_t virtPageIdx, uint32_t pageDisp, uint8_t TLBNum) {
	uint32_t segID = (segment & SEGREGSegID) >> 2;
	uint32_t realPgMask = (iommuregs->TranslationCtrl & TRANSCTRLPageSize) ? TLBRealPgNum4K : TLBRealPgNum2K;
	uint32_t realAddr;
	uint32_t hashAnchorTableEntry;
	uint32_t genAddrTag = ((segID << 17) | (virtPageIdx & 0x0001FFF0));

	if (iommuregs->TLB0_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 0;
			realAddr = ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8) | pageDisp;
		} else {
			logmsgf(LOGMMU, "MMU: TLB0 Addr Tag matched but invalid. Searching HAT/IPT.\n");
			realAddr = findIPT(genAddrTag, TLBNum, virtPageIdx, segment) | pageDisp;
		}
	} else if (iommuregs->TLB1_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 1;
			realAddr = ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8) | pageDisp;
		} else {
			logmsgf(LOGMMU, "MMU: TLB0 Addr Tag matched but invalid. Searching HAT/IPT.\n");
			realAddr = findIPT(genAddrTag, TLBNum, virtPageIdx, segment) | pageDisp;
		}
	} else {
		logmsgf(LOGMMU, "MMU: Both TLB0 and TLB1 are stale. Searching HAT/IPT.\n");
		realAddr = findIPT(genAddrTag, TLBNum, virtPageIdx, segment);
	}

	return realAddr;
}

uint32_t translateaddr(uint32_t addr, uint32_t segment) {
	uint32_t virtPageIdx;
	uint32_t pageDisp;
	uint8_t TLBNum;
	uint32_t realAddr;
	
	if ((iommuregs->TranslationCtrl & TRANSCTRLSegReg0VirtEqReal) && (addr & 0xF0000000) == 0) {
		return addr & 0x00FFFFFF;
	} else if (iommuregs->TranslationCtrl & TRANSCTRLPageSize) {
		// 4K Pages
		virtPageIdx = (addr & 0x0FFFF000) >> 11;
		pageDisp = addr & 0x00000FFF;
		TLBNum = virtPageIdx & 0x0000000F;
		
		realAddr = checkTLB(segment, virtPageIdx, pageDisp, TLBNum);
	} else {
		// 2K Pages
		virtPageIdx = (addr & 0x0FFFF800) >> 11;
		pageDisp = addr & 0x000007FF;
		TLBNum = virtPageIdx & 0x0000000F;

		realAddr = checkTLB(segment, virtPageIdx, pageDisp, TLBNum);
	}
	return realAddr;
}

uint32_t translatecheck(uint32_t addr, uint8_t tag) {
	uint32_t realAddr;
	uint32_t segment = iommuregs->_direct[(addr & 0xF0000000) >> 28];
	
	if (segment & SEGREGPresent) {
		if ((segment & SEGREGProcAcc) && tag == TAG_PROC) {
			// Segment protected from PROC access
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from Processor accesses.\n", (addr & 0xF0000000) >> 28);
		} else if ((segment & SEGREGIOAcc) && tag == TAG_IO) {
			// Segment protected from IO access
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from I/O accesses.\n", (addr & 0xF0000000) >> 28);
		} else {
			realAddr = translateaddr(addr, segment);
			// TODO: Update R/C bits
			logmsgf(LOGMMU, "MMU: Address (0x%08X) translated to: 0x%08X\n", addr, realAddr);
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