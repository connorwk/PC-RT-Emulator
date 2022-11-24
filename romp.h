// ROMP CPU Emulation
#ifndef _ROMP
#define _ROMP
#include <stdint.h>

#define NORMEXEC 0
#define DIRECTEXEC 1

#define PROG_STATUS_0		0x00000100
#define PROG_STATUS_1		0x00000110
#define PROG_STATUS_2		0x00000120
#define PROG_STATUS_3		0x00000130
#define PROG_STATUS_4		0x00000140
#define PROG_STATUS_5		0x00000150
#define PROG_STATUS_6		0x00000160
#define PROG_STATUS_MC	0x00000170
#define PROG_STATUS_PC	0x00000180
#define PROG_STATUS_SVC	0x00000190

union SCRs {
	uint32_t _direct[16];
	struct {
		uint32_t Resvered0;
		uint32_t Resvered1;
		uint32_t Resvered2;
		uint32_t Resvered3;
		uint32_t Resvered4;
		uint32_t Resvered5;
		uint32_t CounterSrc;
		uint32_t Counter;
		uint32_t TS;
		uint32_t ExcepCtrlReg;
		uint32_t MultQuot;
		uint32_t MCSPCS;
		uint32_t IRB;
		uint32_t IAR;
		uint32_t ICS;
		uint32_t CS;
	};
};

// CS Masks
#define CS_MASK_Perm_Zero 0x00000080
#define CS_MASK_Clear_Perm_Zero 0xFFFFFF7F

#define CS_MASK_LT 0x00000040
#define CS_MASK_Clear_LT 0xFFFFFFBF

#define CS_MASK_EQ 0x00000020
#define CS_MASK_Clear_EQ 0xFFFFFFDF

#define CS_MASK_GT 0x00000010
#define CS_MASK_Clear_GT 0xFFFFFFEF

#define CS_MASK_C0 0x00000008
#define CS_MASK_Clear_C0 0xFFFFFFF7

#define CS_MASK_OV 0x00000002
#define CS_MASK_Clear_OV 0xFFFFFFFD

#define CS_MASK_TB 0x00000001
#define CS_MASK_Clear_TB 0xFFFFFFFE

// Machine/Program Check Status Masks pg. 11-86
#define MCS_MASK_ProcChanChk			0x00008000
#define MCS_MASK_ParityChk				0x00002000
#define MCS_MASK_InstTimeout			0x00001000
#define MCS_MASK_DataTimeout			0x00000800
#define MCS_MASK_ProcChanTimeout	0x00000400
#define MCS_MASK_IOTrap						0x00000200
#define PCS_MASK_PCKnownOrig			0x00000080
#define PCS_MASK_PCUnknownOrig		0x00000040
#define PCS_MASK_ProgTrap					0x00000020
#define PCS_MASK_PrivInstExcp			0x00000010
#define PCS_MASK_IllegalOpCode		0x00000008
#define PCS_MASK_InstAddrExcp			0x00000004
#define PCS_MASK_DataAddrExcp			0x00000002

// ICS Masks pg. 11-24
#define ICS_MASK_ParityErrRetry		0x00001000
#define ICS_MASK_MemProtect				0x00000800
#define ICS_MASK_UnprivState			0x00000400
#define ICS_MASK_TranslateState		0x00000200
#define ICS_MASK_IntMask					0x00000100
#define ICS_MASK_CheckStopMask		0x00000080
#define ICS_MASK_ProcPriority			0x00000007

/*
struct OldPS {
	uint32_t OldIAR;
	uint16_t OldICS;
	uint16_t OldCS;
};
*/

uint32_t* procinit (void);
union SCRs* getSCRptr (void);
void progcheck (void);
void lt_eq_gt_flag_check (uint32_t val);
void algebretic_cmp (uint32_t val1, uint32_t val2);
void logical_cmp (uint32_t val1, uint32_t val2);
void c0_flag_check (uint64_t val);
void ov_flag_check (uint64_t val);
uint32_t fetch (void);
void decode (uint32_t inst, uint8_t mode);
#endif