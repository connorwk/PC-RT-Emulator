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
uint8_t MEARlocked = 0;
uint32_t* ICSptr;
uint8_t lastUsedTLB[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Lookup tables
static const uint32_t specsizelookup[16] = {0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215};
// Used in HAT/IPT Base address calculation, the value is how far left to shift and thus "multiply" the result
static const uint32_t HATIPTBaseAddrMultLookup[32] = {0, 9, 9, 9, 9, 9, 9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 8, 8, 8, 8, 8, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static const uint32_t HATAddrGenMask[32] = {0, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF};

// Globals for MMU
uint32_t effectiveAddr;
uint8_t tag;
uint8_t loadOrStore;
uint8_t instFetch;
uint8_t inIPTSearch = 0;

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

void updateMERandMEAR (uint32_t merBit) {
	logmsgf(LOGMMU, "MMU: Error MERbits to set: 0x%08X MER: 0x%08X Effective Addr: 0x%08X\n", merBit, iommuregs->MemException, effectiveAddr);
	// If override tag don't set any exception bits...
	if (tag == TAG_OVERRIDE) {return;}

	if (tag == TAG_PROC) {
		// If SegProt, IPT Spec, Page Fault, Protection, or Data Error is hit
		if (merBit & MERMultiCheck) {
			// If any other exceptions are already set, set Multiple Exception flag
			if ((iommuregs->MemException & MERMultiCheck) || MEARlocked) {
				iommuregs->MemException |= MERMultExcp;
			}
			// If MEAR is not locked, load it.
			if (!MEARlocked && !instFetch) {
				iommuregs->MemExceptionAddr = effectiveAddr;
				MEARlocked = 1;
			}
		}

		/*
		switch (merBit) {
			case MERSegProtV:
				
				break;
			case MERIPTSpecErr:
				
				break;
			case MERPageFault:
				
				break;
			case MERProtect:
				
				break;
			case MERData:
				
				break;
		}
		*/
		iommuregs->MemException |= merBit;

		// if !RASDiagMode
					// Exception reply

		if (loadOrStore) {
			iommuregs->MemException |= MERAccType;
		} else {
			iommuregs->MemException &= ~MERAccType;
		}
	}

	// Set Extern Device Exception accordingly
	if ((merBit & MERMultiCheck) && tag == TAG_IO) {
		iommuregs->MemException |= MERExtDevExcp;
	} else {
		iommuregs->MemException &= ~MERExtDevExcp;
	}


	switch (merBit) {
		case MERInvMemAddr:
			iommuregs->MemException |= merBit;
			// Exception reply for Read and Trans Write
			// Prog Check Interrupt for non-Trans Write
			if (!MEARlocked) {
				iommuregs->MemExceptionAddr = effectiveAddr;
			}
			break;
		case MERInvIOAddr:
			iommuregs->MemException |= merBit;
			// if !RASDiagMode
				// Exception reply for I/O Read
			// Prog Check Interrupt for I/O Write
			if (!MEARlocked) {
				iommuregs->MemExceptionAddr = effectiveAddr;
			}
			break;
		case MERTLBReload:
			// if TRANSCTRLIntOnSuccTLBReload
				// set bit
				// Exception reply if !RASDiagMode
			break;
		case MERWriteROM:
			iommuregs->MemException |= merBit;
			if (!MEARlocked) {
				iommuregs->MemExceptionAddr = effectiveAddr;
			}
			break;
		case MERTLBSpec:
			iommuregs->MemException |= merBit;
			// if !RASDiagMode
				// Exception reply
				// Machine Check Interrupt
			break;
	}
}

int invalidAddrCheck (uint32_t addr, uint32_t end_addr, uint8_t bytes) {
	if ((addr >= end_addr || (addr == (end_addr - 1) && bytes > BYTE) || (addr == (end_addr - 2) && bytes > HALFWORD) || (addr == (end_addr - 3) && bytes > HALFWORD)) && bytes != INST) {
		logmsgf(LOGMEM, "MEM: Error attempted access (0x%08X) outside of valid range (0x%08X)\n", addr, end_addr);
		if (inIPTSearch) {
			updateMERandMEAR(MERPageFault);
		} else {
			updateMERandMEAR(MERInvMemAddr);
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
		updateMERandMEAR(MERWriteROM);
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

uint8_t memProtectAndLockbitCheck(uint32_t segment, uint32_t TLBNum, uint32_t virtPageIdx, uint32_t RealPageNum_VBs_KBs, uint32_t WB_TransID_LBs) {
	uint32_t lockbitLine = virtPageIdx & 0x0000000F;
	uint32_t lockbit = WB_TransID_LBs & (0x00001000 >> lockbitLine);
	
	// Check for memory access exceptions.
	if (segment & SEGREGSpecial) {
		// Lockbit processing pg 11-110
		if (((WB_TransID_LBs & TLBTransactionID) >> 16) == iommuregs->TransactionID) {
			if (WB_TransID_LBs & TLBWriteBit) {
				if (loadOrStore == STORE && !lockbit) {
					// Lockbit Violation
					updateMERandMEAR(MERData);
					return 0;
				}
			} else {
				if (loadOrStore == STORE || !lockbit) {
					// Lockbit Violation
					updateMERandMEAR(MERData);
					return 0;
				}
			}
		} else {
			// TID Not equal
			updateMERandMEAR(MERData);
			return 0;
		}
	} else {
		// Memory protection processing pg 11-109
		switch (RealPageNum_VBs_KBs & TLBKeyBits) {
			case 0x00000000:
				// Key 0 Fetch-Protected
				if (segment & SEGREGKeyBit) {
					// Not Allowed
					updateMERandMEAR(MERProtect);
					return 0;
				}
				break;
			case 0x00000001:
				// Key 0 Read/Write
				if (segment & SEGREGKeyBit) {
					if (loadOrStore == STORE) {
						// Not Allowed
						updateMERandMEAR(MERProtect);
						return 0;
					}
				}
				break;
			case 0x00000002:
				// Public Read/Write
				// Always Allowed
				return 1;
				break;
			case 0x00000003:
				// Public Read-Only
				if (loadOrStore == STORE) {
					// Not Allowed
					updateMERandMEAR(MERProtect);
					return 0;
				}
				break;
		}
	}
	return 1;
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

	uint32_t searchCnt = 0;
	inIPTSearch = 1;
	
	HATentry = realread(HATIPTaddr | 0x4, WORD);
	if (HATentry & HATIPT_EmptyBit) {
		updateMERandMEAR(MERIPTSpecErr & MERPageFault);
	} else {
		HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_HATPtr) >> 12);
		IPTentry = realread(HATIPTaddr, WORD);
		HATentry = realread(HATIPTaddr | 0x4, WORD);
		while (1) {
			searchCnt++;
			if ((IPTentry & HATIPT_AddrTag) == (genAddrTag | TLBNum)) {
				// TLB Reload
				logmsgf(LOGMMU, "MMU: IPT Entry found at: 0x%08X\n", HATIPTaddr);
				if (lastUsedTLB[TLBNum]) {
					logmsgf(LOGMMU, "MMU: Reloading TLB0[%d].\n", TLBNum);
					lastUsedTLB[TLBNum] = 0;
					iommuregs->TLB0_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB0_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x8, WORD);
					}
					if (memProtectAndLockbitCheck(segment, virtPageIdx, TLBNum, iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum], iommuregs->TLB0_WB_TransID_LBs[TLBNum])) {
						return ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
					} else {
						return 0xFFFFFFFF;
					}
					
				} else {
					logmsgf(LOGMMU, "MMU: Reloading TLB1[%d].\n", TLBNum);
					lastUsedTLB[TLBNum] = 1;
					iommuregs->TLB1_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB1_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x8, WORD);
					}
					if (memProtectAndLockbitCheck(segment, virtPageIdx, TLBNum, iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum], iommuregs->TLB1_WB_TransID_LBs[TLBNum])) {
						return ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
					} else {
						return 0xFFFFFFFF;
					}
				}
			} else {
				if (HATentry & HATIPT_LastBit) {
					updateMERandMEAR(MERIPTSpecErr & MERPageFault);
					break;
				} else {
					if (searchCnt == 127 && (iommuregs->TranslationCtrl & TRANSCTRLTermLongIPTSearch)) {
						updateMERandMEAR(MERIPTSpecErr & MERPageFault);
						break;
					}
					HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_IPTPtr) << 4);
					IPTentry = realread(HATIPTaddr, WORD);
					HATentry = realread(HATIPTaddr | 0x4, WORD);
				}
			}
		};
	}
	return 0xFFFFFFFF;
}

uint32_t checkTLB (uint32_t segment, uint32_t virtPageIdx, uint8_t TLBNum) {
	uint32_t segID = (segment & SEGREGSegID) >> 2;
	uint32_t realPgMask = (iommuregs->TranslationCtrl & TRANSCTRLPageSize) ? TLBRealPgNum4K : TLBRealPgNum2K;
	uint32_t realAddr = 0;
	uint32_t hashAnchorTableEntry;
	uint32_t genAddrTag = ((segID << 17) | (virtPageIdx & 0x0001FFF0));
	uint8_t TLBused = 0;

	if (iommuregs->TLB0_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 0;
			realAddr |= ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
			TLBused++;
		}
	}
	
	if (iommuregs->TLB1_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 1;
			realAddr |= ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
			TLBused++;
		}
	}

	if (!TLBused) {
		logmsgf(LOGMMU, "MMU: Neither TLB0 and TLB1 match. Preforming IPT search.\n");
		realAddr = findIPT(genAddrTag, TLBNum, virtPageIdx, segment);
	}

	if (TLBused == 2) {
		updateMERandMEAR(MERTLBSpec);
	}
	
	inIPTSearch = 0;
	return realAddr;
}

uint32_t translateaddr(uint32_t segment) {
	uint32_t virtPageIdx;
	uint32_t pageDisp;
	uint8_t TLBNum;
	uint32_t realAddr;
	
	if ((iommuregs->TranslationCtrl & TRANSCTRLSegReg0VirtEqReal) && (effectiveAddr & 0xF0000000) == 0) {
		// Memory protection and Lockbit processing disabled for Virtual Equal Real accesses
		return effectiveAddr & 0x00FFFFFF;
	} else if (iommuregs->TranslationCtrl & TRANSCTRLPageSize) {
		// 4K Pages
		virtPageIdx = (effectiveAddr & 0x0FFFF000) >> 11;
		pageDisp = effectiveAddr & 0x00000FFF;
		TLBNum = virtPageIdx & 0x0000000F;
		
		realAddr = checkTLB(segment, virtPageIdx, TLBNum) | pageDisp;
	} else {
		// 2K Pages
		virtPageIdx = (effectiveAddr & 0x0FFFF800) >> 11;
		pageDisp = effectiveAddr & 0x000007FF;
		TLBNum = virtPageIdx & 0x0000000F;

		realAddr = checkTLB(segment, virtPageIdx, TLBNum) | pageDisp;
	}
	return realAddr;
}

uint32_t translatecheck() {
	uint32_t realAddr;
	uint32_t segment = iommuregs->_direct[(effectiveAddr & 0xF0000000) >> 28];
	
	if (segment & SEGREGPresent) {
		if ((segment & SEGREGProcAcc) && tag == TAG_PROC) {
			// Segment protected from PROC access
			updateMERandMEAR(MERSegProtV);
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from Processor accesses.\n", (effectiveAddr & 0xF0000000) >> 28);
		} else if ((segment & SEGREGIOAcc) && tag == TAG_IO) {
			// Segment protected from IO access
			updateMERandMEAR(MERSegProtV);
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from I/O accesses.\n", (effectiveAddr & 0xF0000000) >> 28);
		} else {
			realAddr = translateaddr(segment);
			// TODO: Update R/C bits
			logmsgf(LOGMMU, "MMU: Address (0x%08X) translated to: 0x%08X\n", effectiveAddr, realAddr);
		}
	} else {
		// Segment disabled.
		// Supposedly we just ignore the request allowing multiple MMUs/devices on the Proc channel.
		// But I assume that some failure of an access happens on the processor side... TODO
		logmsgf(LOGMMU, "MMU: Error Segment %d is disabled.\n", (effectiveAddr & 0xF0000000) >> 28);
	}
	return realAddr;
}

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode, uint8_t inputtag) {
	//logmsgf(LOGMMU, "MMU: Write 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = STORE;
	if (bytes == INST) {instFetch = 1;} else {instFetch = 0;}

	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			realwrite(translatecheck(), data, bytes);
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

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode, uint8_t inputtag) {
	uint32_t data = 0;
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = LOAD;
	if (bytes == INST) {instFetch = 1;} else {instFetch = 0;}

	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			data = realread(translatecheck(), bytes);
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
				MEARlocked = 0;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO read outside valid ranges 0x%08X\n", addr);
			data = 0;
		}
	}
	//logmsgf(LOGMMU, "MMU: Read 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}

uint32_t proctsh (uint32_t addr, uint8_t bytes, uint8_t inputtag) {
	uint32_t data = 0;
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = STORE;

	// TSH is treated as a STORE pg. 11-109
	if (*ICSptr & ICS_MASK_TranslateMode) {
		data = realread(translatecheck(), bytes);
		realwrite(translatecheck(), 0xFF, BYTE);
	} else {
		data = realread(addr, bytes);
		realwrite(addr, 0xFF, BYTE);
	}
	//logmsgf(LOGMMU, "MMU: Test & Set 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}