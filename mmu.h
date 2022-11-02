// MMU Emulation

#define BYTE 1
#define HALFWORD 2
#define WORD 4

#define MEMORY 0
#define PIO 1

void mmuinit (uint8_t* memptr);
void mmuwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t mmuread (uint8_t* ptr, uint32_t addr, uint8_t bytes);

#define MMUCONFIGSIZE 65536

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
