// ROMP CPU Emulation
#include <stdint.h>

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

#define CounterSrc		SCR[6]
#define Counter				SCR[7]
#define TS						SCR[8]
#define ExcepCtrlReg	SCR[9]
#define MultQuot			SCR[10]
#define MCSPCS				SCR[11]
#define IRB						SCR[12]
#define IAR						SCR[13]
#define ICS						SCR[14]
#define CS						SCR[15]

/*
struct OldPS {
	uint32_t OldIAR;
	uint16_t OldICS;
	uint16_t OldCS;
};
*/

void procinit (void);
void z_lt_eq_gt_flag_set (uint32_t val);
uint32_t fetch (void);
void decode (uint32_t inst);