// MMU Emulation
#ifndef _MMU
#define _MMU
#include <stdint.h>

#define BYTE 1
#define HALFWORD 2
#define WORD 4
#define INST 5 // Special for Instruction fetch to NOT ignore LSBs of addr.

#define MEMORY 0
#define PIO 1

void rominit (const char *file);
void mmuinit (uint8_t* memptr);
void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode);
uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode);

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
#define ROMSPECStartAddr	((iommuregs->ROMSpec & ROMSPECAddr) << 12) & (0xFFFFF700 << (iommuregs->ROMSpec & ROMSPECSize))
#define ROMSPECEndAddr		(ROMSPECStartAddr + specsizelookup[iommuregs->ROMSpec & ROMSPECSize])

// Ram Spec Reg Format pg. 11-113
#define RAMSPECAddr		0x00000FF0
#define RAMSPECSize		0x0000000F
#define RAMSPECStartAddr	((iommuregs->RAMSpec & RAMSPECAddr) << 12) & (0xFFFFF700 << (iommuregs->RAMSpec & RAMSPECSize))
#define RAMSPECEndAddr		(RAMSPECStartAddr + specsizelookup[iommuregs->RAMSpec & RAMSPECSize])

// Translation Ctrl Reg Format pg. 11-117
#define TRANSCTRLSegReg0VirtEqReal			0x00008000
#define TRANSCTRLIntOnSuccessPErrRetry	0x00004000
#define TRANSCTRLEnblRasDiag						0x00002000
#define TRANSCTRLTermLongIPTSearch			0x00001000
#define TRANSCTRLIntOnCorrECCErr				0x00000800
#define TRANSCTRLIntOnSuccTLBReload			0x00000400
#define TRANSCTRLPageSize								0x00000100
#define TRANSCTRLHATIPTBaseAddr					0x000000FF

/*
 * I/O Address Assignments pg. 11-136
 */
union MMUIOspace {
	uint8_t _direct[MMUCONFIGSIZE];
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
		uint32_t TLB0_AddrTagField_TLB0[16];
		uint32_t TLB1_AddrTagField_TLB0[16];
		uint32_t TLB0_RealPageNum_VBs_KBs_TLB0[16];
		uint32_t TLB1_RealPageNum_VBs_KBs_TLB0[16];
		uint32_t TLB0_WB_TransID_LBs_TLB0[16];
		uint32_t TLB1_WB_TransID_LBs_TLB0[16];
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