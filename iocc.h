// IO Emulation
#ifndef _IOCC
#define _IOCC

#define SYSBRDCNFGSIZE 262143

void ioinit (struct procBusStruct* procBusPointer);
void iocycle (void);
uint8_t* getMDAPtr (void);
void iowrite (uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t ioread (uint32_t addr, uint8_t bytes);
void iomemwrite (uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t iomemread (uint32_t addr, uint8_t bytes);

// Channel Control Register Format pg. 5-31
#define CCR_DMACtrlRegAcc		0x80
#define CCR_DMAIOAdptAcc		0x40
#define CCR_DMAKeyboardAcc	0x20
#define CCR_ProcAcc					0x18
#define CCR_RefreshCtrl			0x06

// Component Reset Reg A pg. 5-59
#define CCRA_IOSlot8	0x80	// 6150 Only
#define CCRA_IOSlot7	0x40	// 6150 Only
#define CCRA_IOSlot6	0x20
#define CCRA_IOSlot5	0x10
#define CCRA_IOSlot4	0x08
#define CCRA_IOSlot3	0x04
#define CCRA_IOSlot2	0x02
#define CCRA_IOSlot1	0x01

// Component Reset Reg B pg. 5-61
#define CCRB_Arbitor	0x20
#define CCRB_DMACtrl2	0x10
#define CCRB_DMACtrl1	0x08
#define CCRB_8051			0x04
#define CCRB_RS232IF	0x02
#define CCRB_8530			0x01

// TCW Format pg. 5-29
#define TCW_VirtAcc					0x8000
#define TCW_IOChanDest			0x4000
#define TCW_TransPrefixBits	0x1FFF

// CSR Format pg. 5-63
#define CSR_ExcepReported			0x80000000
#define CSR_IntPending				0x40000000
#define CSR_EPOW							0x10000000
#define CSR_SoftReset					0x08000000
#define CSR_SysAttn						0x04000000
#define CSR_PIOError					0x01000000
#define CSR_DMAErrorChanBits	0x00FF0000
#define CSR_PIODMA						0x00008000
#define CSR_ProtViolation			0x00004000
#define CSR_InvalidOp					0x00002000
#define CSR_IOChanCheck				0x00001000
#define CSR_DMAExcep					0x00000800
#define CSR_ChannelResetCapt	0x00000400
#define CSR_SystemBoardBsy		0x00000200
#define CSR_PIOReqPending			0x00000100
#define CSR_ReservedBits			0x404000FF

// Mem Config Register Format pg. 5-73
#define MEMCNFG_Reserved			0x80
#define MEMCNFG_MemBrdSlotD		0x70
#define MEMCNFG_RefreshRate		0x08
#define MEMCNFG_MemBrdSlotC		0x07

/*
 * I/O Address Assignments pg. 11-136
 */
struct SysBrdConfig {
	uint8_t		Z8530[4];						//F0008000-F0008003
	uint8_t		CHAExtReg;					//F0008020
	uint8_t		CHBExtReg;					//F0008040
	uint8_t		Z8530Intack;				//F0008060
	uint8_t		IODelayReg;					//F00080E0
	uint8_t		KeyboardAdpt[8];		//F0008400-F0008407
	uint8_t		RTC[64];						//F0008800-F000883F
	uint8_t		P8237_1[16];				//F0008840-F000884F
	uint8_t		P8237_2[16];				//F0008860-F000886F TODO: Confirm this is correct? Does the doc have a typo?
//	uint8_t		P8259_1[2];					//F0008880-F0008881
//	uint8_t		P8259_2[2];					//F00088A0-F00088A1
	uint8_t		DMARegDBR;					//F00088C0
	uint8_t		DMARegDMR;					//F00088E0
	uint8_t		CH8EnReg;						//F0008C00
	uint8_t		CtrlRegCCR;					//F0008C20
	uint8_t		CRRAReg;						//F0008C40
	uint8_t		CRRBReg;						//F0008C60
	uint8_t		MemCnfgReg;					//F0008C80
	uint8_t		DIAReg;							//F0008CA0
	uint16_t	TCW[1024];					//F0010000-F00107FE
	uint32_t	CSR;								//F0010800
};

#endif