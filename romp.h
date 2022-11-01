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