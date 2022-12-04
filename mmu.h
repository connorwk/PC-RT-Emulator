// MMU Emulation
#ifndef _MMU
#define _MMU
#include <stdint.h>
#include "defs.h"

void rominit (const char *file);
uint8_t* mmuinit (uint8_t* memptr, struct procBusStruct* procBus);
void realwrite (uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t realread (uint32_t addr, uint8_t bytes);
int invalidAddrCheck (uint32_t addr, uint32_t end_addr, uint8_t bytes);
void mmuCycle (void);

#define MMUCONFIGSIZE 65536
#define ROMSIZE 65536
#define MAXREALADDR 16777214

// Memory Address Real/Virtual Map pg. 1-35
#define IOChanIOMapStartAddr	0xF0000000
#define IOChanIOMapEndAddr		0xF0FFFFFF
#define IOChanMemMapStartAddr	0xF4000000
#define IOChanMemMapEndAddr		0xF4FFFFFF
#define IOChanFPAStartAddr		0xFF000000
#define IOChanFPAEndAddr			0xFFFFFFFF

// Memory Exception Register Map pg. 11-120
#define MERSegProtV		0x00020000
#define MERProcACKD		0x00010000
#define MERProcNAKD		0x00008000
#define MERInvMemAddr	0x00004000
#define MERInvIOAddr	0x00002000
#define MERAccType		0x00001000
#define MERUnCorrECC	0x00000800
#define MERCorrECC		0x00000400
#define MERTLBReload	0x00000200
#define MERWriteROM		0x00000080
#define MERIPTSpecErr	0x00000040
#define MERExtDevExcp	0x00000020
#define MERMultExcp		0x00000010
#define MERPageFault	0x00000008
#define MERTLBSpec		0x00000004
#define MERProtect		0x00000002
#define MERData				0x00000001
#define MERMultiCheck	0x0002004B

// Segment Reg Format pg. 11-127
#define SEGREGPresent	0x00010000
#define SEGREGProcAcc	0x00008000
#define SEGREGIOAcc		0x00004000
#define SEGREGSegID		0x00003FFC
#define SEGREGSpecial	0x00000002
#define SEGREGKeyBit	0x00000001

// Rom Spec Reg Format pg. 11-115
#define ROMSPECParity	0x00001000
#define ROMSPECAddr		0x00000FF0
#define ROMSPECSize		0x0000000F
#define ROMSPECStartAddr	((iommuregs->ROMSpec & ROMSPECAddr) << 12) & (0xFFFFFE00 << (iommuregs->ROMSpec & ROMSPECSize))
#define ROMSPECEndAddr		((ROMSPECStartAddr) + specsizelookup[iommuregs->ROMSpec & ROMSPECSize])

// Ram Spec Reg Format pg. 11-113
#define RAMSPECAddr		0x00000FF0
#define RAMSPECSize		0x0000000F
#define RAMSPECStartAddr	((iommuregs->RAMSpec & RAMSPECAddr) << 12) & (0xFFFFFE00 << (iommuregs->RAMSpec & RAMSPECSize))
#define RAMSPECEndAddr		((RAMSPECStartAddr) + specsizelookup[iommuregs->RAMSpec & RAMSPECSize])

// Translation Ctrl Reg Format pg. 11-117
#define TRANSCTRLSegReg0VirtEqReal			0x00008000
#define TRANSCTRLIntOnSuccessPErrRetry	0x00004000
#define TRANSCTRLEnblRasDiag						0x00002000
#define TRANSCTRLTermLongIPTSearch			0x00001000
#define TRANSCTRLIntOnCorrECCErr				0x00000800
#define TRANSCTRLIntOnSuccTLBReload			0x00000400
#define TRANSCTRLPageSize								0x00000100
#define TRANSCTRLHATIPTBaseAddr2K				0x0000007F
#define TRANSCTRLHATIPTBaseAddr4K				0x000000FF

// TLB Real Page Num Format pg. 11-130
#define TLBRealPgNum2K	0x0000FFF8
#define TLBRealPgNum4K	0x0000FFF0
#define TLBValidBit			0x00000004
#define TLBKeyBits			0x00000003

// TLB Write Bit, Trans ID, Lockbits Format pg. 11-131
#define TLBWriteBit				0x01000000
#define TLBTransactionID	0x00FF0000
#define TLBLockBits				0x0000FFFF

// RAS Mode Diag Reg Format pg. 11-132
#define RMDR_ArrayChkBits	0x0000FF00
#define RMDR_AltChkBits		0x000000FF

// HAT/IPT Format pg. 11-102
#define HATIPT_Key				0xC0000000
#define HATIPT_AddrTag		0x1FFFFFFF

#define HATIPT_EmptyBit		0x80000000
#define HATIPT_HATPtr			0x1FFF0000
#define HATIPT_LastBit		0x00008000
#define HATIPT_IPTPtr			0x00001FFF

#define HATIPT_SpecWrProt	0x01000000
#define HATIPT_TID				0x00FF0000
#define HATIPT_LockBits		0x0000FFFF

/*
 * I/O Address Assignments pg. 11-136
 */
union MMUIOspace {
	uint32_t _direct[MMUCONFIGSIZE];
	struct {
		uint32_t SegmentRegs[16];
		uint32_t IOBaseAddr;
		uint32_t MemException;
		uint32_t MemExceptionAddr;
		uint32_t TranslatedRealAddr;
		uint32_t TransactionID;
		uint32_t TranslationCtrl;
		uint32_t RAMSpec;
		uint32_t ROMSpec;
		uint32_t RASModeDiag;
		uint32_t Reserved1[7];
		uint32_t TLB0_AddrTagField[16];
		uint32_t TLB1_AddrTagField[16];
		uint32_t TLB0_RealPageNum_VBs_KBs[16];
		uint32_t TLB1_RealPageNum_VBs_KBs[16];
		uint32_t TLB0_WB_TransID_LBs[16];
		uint32_t TLB1_WB_TransID_LBs[16];
		uint32_t InvalidateEntireTLB;
		uint32_t InvalidateTLBEntriesInSeg;
		uint32_t InvalidateTLBEntriesInEffectiveAddr;
		uint32_t LoadRealAddr;
		uint32_t Reserved2[3964];
		uint32_t Ref_ChangeBits[4096];
		uint32_t Reserved3[53248];
	};
};
#endif