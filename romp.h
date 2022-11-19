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

/*
struct OldPS {
	uint32_t OldIAR;
	uint16_t OldICS;
	uint16_t OldCS;
};
*/

void procinit (void);
void printregs(void);
void progcheck (void);
void lt_eq_gt_flag_check (uint32_t val);
void algebretic_cmp (uint32_t val1, uint32_t val2);
void logical_cmp (uint32_t val1, uint32_t val2);
void c0_flag_check (uint64_t val);
void ov_flag_check (uint64_t val);
uint32_t fetch (void);
void decode (uint32_t inst, uint8_t mode);
#endif