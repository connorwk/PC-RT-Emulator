// ROMP CPU Emulation
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "romp.h"
#include "mmu.h"
#include "logfac.h"

uint32_t GPR[16];
union SCRs SCR;

uint32_t wait;

void printregs(void) {
	printf("GPR0:		0x%08X		GPR1:		0x%08X\n", GPR[0], GPR[1]);
	printf("GPR2:		0x%08X		GPR3:		0x%08X\n", GPR[2], GPR[3]);
	printf("GPR4:		0x%08X		GPR5:		0x%08X\n", GPR[4], GPR[5]);
	printf("GPR6:		0x%08X		GPR7:		0x%08X\n", GPR[6], GPR[7]);
	printf("GPR8:		0x%08X		GPR9:		0x%08X\n", GPR[8], GPR[9]);
	printf("GPR10:		0x%08X		GPR11:		0x%08X\n", GPR[10], GPR[11]);
	printf("GPR12:		0x%08X		GPR13:		0x%08X\n", GPR[12], GPR[13]);
	printf("GPR14:		0x%08X		GPR15:		0x%08X\n", GPR[14], GPR[15]);

	printf("SCR0:		0x%08X		SCR1:		0x%08X\n", SCR._direct[0], SCR._direct[1]);
	printf("SCR2:		0x%08X		SCR3:		0x%08X\n", SCR._direct[2], SCR._direct[3]);
	printf("SCR4:		0x%08X		SCR5:		0x%08X\n", SCR._direct[4], SCR._direct[5]);
	printf("SCR6:		0x%08X		SCR7:		0x%08X\n", SCR._direct[6], SCR._direct[7]);
	printf("SCR8:		0x%08X		SCR9:		0x%08X\n", SCR._direct[8], SCR._direct[9]);
	printf("SCR10:		0x%08X		SCR11:		0x%08X\n", SCR._direct[10], SCR._direct[11]);
	printf("SCR12:		0x%08X		SCR13:		0x%08X\n", SCR._direct[12], SCR._direct[13]);
	printf("SCR14:		0x%08X		SCR15:		0x%08X\n", SCR._direct[14], SCR._direct[15]);
}

void procinit (void) {
	wait = 0;
	for (uint8_t i=0; i < 16; i++) {
		GPR[i] = 0x00000000;
		SCR._direct[i] = 0x00000000;
	}
	// Initial IAR from 000000? pg. 11-140
	SCR.IAR = procread(SCR.IAR, WORD, MEMORY);
}

void lt_eq_gt_flag_check (uint32_t val) {
	SCR.CS &= 0xFFFFFF0F;
	if (val == 0x00000000) {
		SCR.CS |= CS_MASK_EQ;
	} else if (val & 0x80000000) {
		SCR.CS |= CS_MASK_LT;
	} else if (val & 0x7FFFFFFF) {
		SCR.CS |= CS_MASK_GT;
	}
}

void algebretic_cmp (uint32_t val1, uint32_t val2) {
	SCR.CS &= 0xFFFFFF0F;
	if ( (int32_t)val1 == (int32_t)val2 ) {
		SCR.CS |= CS_MASK_EQ;
	} else if ( (int32_t)val1 < (int32_t)val2 ) {
		SCR.CS |= CS_MASK_LT;
	} else if ( (int32_t)val1 > (int32_t)val2 ) {
		SCR.CS |= CS_MASK_GT;
	}
}

void logical_cmp (uint32_t val1, uint32_t val2) {
	SCR.CS &= 0xFFFFFF0F;
	if ( val1 == val2 ) {
		SCR.CS |= CS_MASK_EQ;
	} else if ( val1 < val2 ) {
		SCR.CS |= CS_MASK_LT;
	} else if ( val1 > val2 ) {
		SCR.CS |= CS_MASK_GT;
	}
}


void c0_flag_check (uint64_t val) {
	if (val == 0x0000000100000000) {
		SCR.CS &= CS_MASK_Clear_C0;
		SCR.CS |= CS_MASK_C0;
	}
}

void ov_flag_check (uint64_t val) {
	if (val < -2147483648 || val > 2147483647) {
		SCR.CS &= CS_MASK_Clear_OV;
		SCR.CS |= CS_MASK_OV;
	}
}

void progcheck (void) { 
	procwrite(PROG_STATUS_PC, SCR.IAR, WORD, MEMORY);
	procwrite(PROG_STATUS_PC+4, SCR.ICS, HALFWORD, MEMORY);
	procwrite(PROG_STATUS_PC+6, SCR.CS, HALFWORD, MEMORY);

}

uint32_t fetch (void) {
	uint32_t inst;
	// TODO: Interrupt, error, POR to clear wait state.
	if (wait) {return 1;}
	inst = procread(SCR.IAR, INST, MEMORY);
	decode(inst);
	return 0;
}

void decode (uint32_t inst) {
	uint8_t nibble0 = (inst & 0xF0000000) >> 28;
	uint8_t r1 = (inst & 0x0F000000) >> 24;
	uint8_t r2 = (inst & 0x00F00000) >> 20;
	uint8_t r3 = (inst & 0x000F0000) >> 16;
	uint8_t byte0 = (inst & 0xFF000000) >> 24;
	uint32_t r3_reg_or_0 = r3 ? GPR[r3] : 0;
	uint32_t BA = (inst & 0x00FFFFFE);
	uint16_t I16 = inst & 0x0000FFFF;
	// From simple sign extention to int8_t for addr calculation
	int8_t JI = inst & 0x00800000 ? ((inst >> 16) | 0xFFFFFF00): (inst & 0x007F0000) >> 16;
	// From simple sign extention to int16_t for addr calculation
	int16_t sI16 = inst & 0x00008000 ? inst | 0xFFFF0000 : inst & 0x00007FFF;

	uint32_t addr;
	uint32_t prevVal;
	int64_t arith_result;
	uint32_t prevIAR = SCR.IAR;

	if (nibble0 < 8) {
		SCR.IAR = SCR.IAR+2;
		// JI, X, D-Short format Instructions
		switch(nibble0) {
			case 0:
				if (inst & 0x0800) {
					// JB
					if ( (SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						SCR.IAR = SCR.IAR + JI;
					}
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JB %s,%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
				} else {
					// JNB
					if ( !(SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						SCR.IAR = SCR.IAR + JI;
					}
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JNB %s,%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
				}
				break;
			case 1:
				// STCS
					procwrite(r3_reg_or_0 + r1, GPR[r2], BYTE, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STCS %s+%d,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1, r2);
					logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1, GPR[r2]);
				break;
			case 2:
				// STHS
					procwrite(r3_reg_or_0 + (r1 << 1), GPR[r2], HALFWORD, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STHS %s+%d,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 1, r2);
					logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1 << 1, GPR[r2]);
				break;
			case 3:
				// STS
					procwrite(r3_reg_or_0 + (r1 << 2), GPR[r2], WORD, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STS %s+%d,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 2, r2);
					logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1 << 2, GPR[r2]);
				break;
			case 4:
				// LCS
				GPR[r2] = procread(r3_reg_or_0 + r1, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LCS GPR%d, %s+%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1);
				break;
			case 5:
				// LHAS
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (r1 << 1), HALFWORD, MEMORY);;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHAS GPR%d, %s+%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1 << 1);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1 << 1);
				break;
			case 6:
				// CAS
				GPR[r1] = GPR[r2]+r3_reg_or_0;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CAS GPR%d, GPR%d+%s\n", prevIAR, (inst & 0xFFFF0000) >> 16, r1, r2, gpr_or_0(r3));
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r1], GPR[r2], r3_reg_or_0);
				break;
			case 7:
				// LS
				GPR[r2] = procread(r3_reg_or_0 + (r1 << 2), WORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LS GPR%d, %s+%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), (r1 << 2));
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1 << 2);
				break;
		}
	} else {
		// R, BI, BA, D format Instructions
		switch(byte0) {
			case 0x88:
				// BNB
				SCR.IAR = SCR.IAR+4;
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = SCR.IAR + (sI16 << 1);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNB %s,%d\n", prevIAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x89:
				// BNBX
				SCR.IAR = SCR.IAR+4;
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = SCR.IAR + (sI16 << 1);
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNBX %s,%d\n", prevIAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x8A:
				// BALA
				SCR.IAR = SCR.IAR+4;
				GPR[15] = SCR.IAR;
				SCR.IAR = BA;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALA 0x%06X\n", prevIAR, inst, BA);
				break;
			case 0x8B:
				// BALAX
				SCR.IAR = SCR.IAR+4;
				GPR[15] = SCR.IAR+4;
				SCR.IAR = BA;
				// TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALAX 0x%06X\n	SUB", prevIAR, inst, BA);
				break;
			case 0x8C:
				// BALI
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = SCR.IAR;
				SCR.IAR = sI16 << 1;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALI GPR%d, %d\n", prevIAR, inst, r2, sI16 << 1);
				break;
			case 0x8D:
				// BALIX
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = SCR.IAR+4;
				SCR.IAR = sI16 << 1;
				// TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALIX GPR%d, %d\n SUB", prevIAR, inst, r2, sI16 << 1);
				break;
			case 0x8E:
				// BB
				SCR.IAR = SCR.IAR+4;
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = SCR.IAR + (sI16 << 1);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BB %s,%d\n", prevIAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x8F:
				// BBX
				SCR.IAR = SCR.IAR+4;
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = SCR.IAR + (sI16 << 1);
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BBX %s,%d\n", prevIAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x90:
				// AIS
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + r3;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		AIS GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x91:
				// INC
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] + r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		INC GPR%d, GPR%d+%02X\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x92:
				// SIS
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] - r3;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SIS GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0x93:
				// DEC
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] - r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		DEC GPR%d, GPR%d-%02X\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x94:
				// CIS
				SCR.IAR = SCR.IAR+2;
				algebretic_cmp(GPR[r2], r3);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CIS GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0x95:
				// CLRSB
				SCR.IAR = SCR.IAR+2;
				prevVal = SCR._direct[r2];
				SCR._direct[r2] = SCR._direct[r2] & ~(0x00008000 >> r3);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRSB SCR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", SCR._direct[r2], prevVal, ~(0x00008000 >> r3));
				break;
			case 0x96:
				// MFS
				SCR.IAR = SCR.IAR+2;
				GPR[r3] = SCR._direct[r2];
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFS SCR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X\n", GPR[r3]);
				break;
			case 0x97:
				// SETSB
				SCR.IAR = SCR.IAR+2;
				prevVal = SCR._direct[r2];
				SCR._direct[r2] = SCR._direct[r2] | (0x00008000 >> r3);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETSB SCR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", SCR._direct[r2], prevVal, (0x00008000 >> r3));
				break;
			case 0x98:
				// CLRBU
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & ~(0x80000000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBU GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, ~(0x80000000 >> r3));
				break;
			case 0x99:
				// CLRBL
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & ~(0x00008000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBL GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, ~(0x00008000 >> r3));
				break;
			case 0x9A:
				// SETBU
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | (0x80000000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBU GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, (0x80000000 >> r3));
				break;
			case 0x9B:
				// SETBL
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | (0x00008000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBL GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, (0x00008000 >> r3));
				break;
			case 0x9C:
				// MFTBIU
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> r3)) | ((SCR.CS & CS_MASK_TB) << (31 - r3));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIU GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & CS_MASK_TB) << (31 - r3)));
				break;
			case 0x9D:
				// MFTBIL
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x00008000 >> r3)) | ((SCR.CS & CS_MASK_TB) << (15 - r3));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIL GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & CS_MASK_TB) << (15 - r3)));
				break;
			case 0x9E:
				// MTTBIU
				SCR.IAR = SCR.IAR+2;
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x80000000 >> r3) >> (31 - r3)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIU GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x80000000 >> r3) >> (31 - r3));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0x9F:
				// MTTBIL
				SCR.IAR = SCR.IAR+2;
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x00008000 >> r3) >> (15 - r3)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIL GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x00008000 >> r3) >> (31 - r3));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0xA0:
				// SARI
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA1:
				// SARI16
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (r3+16));
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI16 GPR%d, %d+16\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA4:
				// LIS
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LIS GPR%d, %02X\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA8:
				// SRI
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> r3;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA9:
				// SRI16
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> (r3+16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI16 GPR%d, %d+16\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAA:
				// SLI
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << r3;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAB:
				// SLI16
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << (r3+16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI16 GPR%d, %d+16\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAC:
				// SRPI
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] >> r3;
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAD:
				// SRPI16
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] >> (r3+16);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI16 GPR%d, %d+16\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAE:
				// SLPI
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] << r3;
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI GPR%d, %d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAF:
				// SLPI16
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] << (r3+16);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI16 GPR%d, %d+16\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB0:
				// SAR
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (GPR[r3] & 0x0000003F));
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SAR GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB1:
				// EXTS
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = GPR[r3] & 0x00008000 ? GPR[r3] | 0xFFFF0000 : GPR[r3] & 0x00007FFF;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		EXTS GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB2:
				// SF
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r3] - (int32_t)GPR[r2];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SF GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], GPR[r3], prevVal);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB3:
				// CL
				SCR.IAR = SCR.IAR+2;
				logical_cmp(GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CL GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB4:
				// C
				SCR.IAR = SCR.IAR+2;
				algebretic_cmp(GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		C GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB5:
				// MTS
				SCR.IAR = SCR.IAR+2;
				if (r2 == 13) {logmsgf(LOGPROC, "PROC: Warning MTS SCR13 is unpredictable. IAR: 0x%08X\n", SCR.IAR);}
				SCR._direct[r2] = GPR[r3];
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTS SCR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X\n", GPR[r3]);
				break;
			case 0xB6:
				// D
				SCR.IAR = SCR.IAR+2;
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				//logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		D GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xB8:
				// SR
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SR GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB9:
				// SRP
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRP GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBA:
				// SL
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SL GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBB:
				// SLP
				SCR.IAR = SCR.IAR+2;
				GPR[r2^0x01] = GPR[r2] << (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLP GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBC:
				// MFTB
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> (GPR[r3] & 0x0000001F))) | ((SCR.CS & 0x00000001) << (31 - (GPR[r3] & 0x0000001F)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTB GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & 0x00000001) << (31 - (GPR[r3] & 0x0000001F))));
				break;
			case 0xBD:
				// TGTE
				SCR.IAR = SCR.IAR+2;
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xBE:
				// TLT
				SCR.IAR = SCR.IAR+2;
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xBF:
				// MTTB
				SCR.IAR = SCR.IAR+2;
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x80000000 >> (GPR[r3] & 0x0000001F))) >> (31 - (GPR[r3] & 0x0000001F)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTB GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x80000000 >> (GPR[r3] & 0x0000001F)));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0xC0:
				// SVC
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning SVC Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				procwrite(0x00000190, SCR.IAR, WORD, MEMORY);
				procwrite(0x00000194, SCR.ICS, HALFWORD, MEMORY);
				procwrite(0x00000196, SCR.CS, HALFWORD, MEMORY);
				procwrite(0x0000019E, r3_reg_or_0 + sI16, HALFWORD, MEMORY);
				SCR.IAR = procread(0x00000198, WORD, MEMORY);
				SCR.ICS = procread(0x0000019C, HALFWORD, MEMORY);
				// TODO: if machine check level, MCS content set to 0
				// TODO: if bit 10, pending mem operations restarted before instr execution resumed, ECR (SCR 9) contains count and mem addr.
				// TODO: if bit 11, intrrupts remain pending until target instr executed
				//      TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	SVC %s+%d\n", prevIAR, inst, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			Regs: IAR: 0x%08X ICS: 0x%08X\n", SCR.IAR, SCR.ICS);
				break;
			case 0xC1:
				// AI
				SCR.IAR = SCR.IAR+4;
				arith_result = (int32_t)GPR[r3] + (int32_t)sI16;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AI GPR%d, GPR%d+%d\n", prevIAR, inst, r2, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC2:
				// CAL16
				SCR.IAR = SCR.IAR+4;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFF0000) | (((r3_reg_or_0 & 0x0000FFFF) + I16) & 0x0000FFFF);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL16 GPR%d, %s+%04X\n", prevIAR, inst, r2, gpr_or_0(r3), I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X + 0x%08X\n", GPR[r2], prevVal, (r3_reg_or_0 & 0x0000FFFF), I16);
				break;
			case 0xC3:
				// OIU
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] | (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIU GPR%d, GPR%d | 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], GPR[r3], I16 << 16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC4:
				// OIL
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] | I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIL GPR%d, GPR%d | 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC5:
				// NILZ
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] & I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILZ GPR%d, GPR%d & 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC6:
				// NILO
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] & (0xFFFF0000 | I16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILO GPR%d, GPR%d & 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], (0xFFFF0000 | I16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC7:
				// XIL
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] ^ I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIL GPR%d, GPR%d ^ 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC8:
				// CAL
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = r3_reg_or_0 + sI16;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC9:
				// LM
				SCR.IAR = SCR.IAR+4;
				for (int i = r2; i < 16; i++) {
					GPR[i] = procread(r3_reg_or_0 + sI16 + i - r2, WORD, MEMORY);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LM GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				for (int i = r2; i < 16; i++) {
					logmsgf(LOGINSTR, "			0x%08X, 0x%08X + 0x%08X + %d\n", GPR[i], r3_reg_or_0, I16, i - r2);
				}
				break;
			case 0xCA:
				// LHA
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (sI16 << 1), HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LHA GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], r3_reg_or_0 + (sI16 << 1));
				break;
			case 0xCB:
				// IOR
				SCR.IAR = SCR.IAR+4;
				addr = r3_reg_or_0 + I16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				GPR[r2] = procread(addr, WORD, PIO);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	IOR GPR%d, %s+0x%04X\n", prevIAR, inst, r2, gpr_or_0(r3), I16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0, I16);
				break;
			case 0xCC:
				// TI
				SCR.IAR = SCR.IAR+4;
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xCD:
				// L
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = procread(r3_reg_or_0 + sI16, WORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	L GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				break;
			case 0xCE:
				// LC
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = procread(r3_reg_or_0 + sI16, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LC GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				break;
			case 0xCF:
				// TSH
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY);
				procwrite(r3_reg_or_0 + sI16 - 1, 0xFF, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		TSH GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				logmsgf(LOGINSTR, "			SET: 0xFF, 0x%08X + %d\n", r3_reg_or_0,  sI16-1);
				break;
			case 0xD0:
				// LPS
				if (r2 & 0xC) {logmsgf(LOGPROC, "PROC: Warning LPS Nibble2 upper bits should be zero. IAR: 0x%08X\n", SCR.IAR);}
				SCR.IAR = procread(r3_reg_or_0 + sI16, WORD, MEMORY);
				SCR.ICS = procread(r3_reg_or_0 + sI16 + 4, HALFWORD, MEMORY);
				SCR.CS = procread(r3_reg_or_0 + sI16 + 6, HALFWORD, MEMORY);
				// TODO: if machine check level, MCS content set to 0
				// TODO: if bit 10, pending mem operations restarted before instr execution resumed, ECR (SCR 9) contains count and mem addr.
				// TODO: if bit 11, intrrupts remain pending until target instr executed
				//      TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LPS 0x%X, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X + %d\n", r3_reg_or_0,  sI16);
				logmsgf(LOGINSTR, "			Regs: IAR: 0x%08X ICS: 0x%08X CS: 0x%08X\n", SCR.IAR, SCR.ICS, SCR.CS);
				break;
			case 0xD1:
				// AEI
				SCR.IAR = SCR.IAR+4;
				arith_result = (int32_t)GPR[r3] + (int32_t)sI16 + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AEI GPR%d, GPR%d+%d\n", prevIAR, inst, r2, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X + CO:%d\n", GPR[r2], GPR[r3], sI16, ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD2:
				// SFI
				SCR.IAR = SCR.IAR+4;
				arith_result = (int32_t)GPR[r3] - (int32_t)sI16;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AEI GPR%d, GPR%d+%d\n", prevIAR, inst, r2, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD3:
				// CLI
				SCR.IAR = SCR.IAR+4;
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning CI Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				logical_cmp(GPR[r3], sI16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CI GPR%d, %d\n", prevIAR, inst, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD4:
				// CI
				SCR.IAR = SCR.IAR+4;
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning CI Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				algebretic_cmp(GPR[r3], sI16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CI GPR%d, %d\n", prevIAR, inst, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD5:
				// NIUZ
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] & (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUZ GPR%d, GPR%d & 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], (I16 << 16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD6:
				// NIUO
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] & ((I16 << 16) | 0x0000FFFF);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUO GPR%d, GPR%d & 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], ((I16 << 16) | 0x0000FFFF));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD7:
				// XIU
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = GPR[r3] ^ (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIU GPR%d, GPR%d & 0x%04X\n", prevIAR, inst, r2, r3, I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], GPR[r3], (I16 << 16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD8:
				// CAU
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = r3_reg_or_0 + (I16 << 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAU GPR%d, %s+0x%04X\n", prevIAR, inst, r2, gpr_or_0(r3), I16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, (I16 << 16));
				break;
			case 0xD9:
				// STM
				SCR.IAR = SCR.IAR+4;
				for (int i = r2; i < 16; i++) {
					procwrite(r3_reg_or_0 + sI16 + i - r2, GPR[i], WORD, MEMORY);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STM %s+%d,GPR%d\n", prevIAR, inst, gpr_or_0(r3), sI16, r2);
				for (int i = r2; i < 16; i++) {
					logmsgf(LOGINSTR, "			0x%08X + 0x%08X + %d, 0x%08X\n", r3_reg_or_0, I16, i - r2, GPR[i]);
				}
				break;
			case 0xDA:
				// LH
				SCR.IAR = SCR.IAR+4;
				GPR[r2] = procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LH GPR%d, %s+%d\n", prevIAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, sI16);
				break;
			case 0xDB:
				// IOW
				SCR.IAR = SCR.IAR+4;
				addr = r3_reg_or_0 + I16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				procwrite(addr, GPR[r2], WORD, PIO);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	IOW %s+0x%04X, GPR%d\n", prevIAR, inst, gpr_or_0(r3), I16, r2);
				logmsgf(LOGINSTR, "			0x%08X + 0x%08X, 0x%08X\n", r3_reg_or_0, I16, GPR[r2]);
				break;
			case 0xDC:
				// STH
				SCR.IAR = SCR.IAR+4;
				procwrite(r3_reg_or_0 + sI16, GPR[r2], HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STH %s+%d,GPR%d\n", prevIAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				break;
			case 0xDD:
				// ST
				SCR.IAR = SCR.IAR+4;
				procwrite(r3_reg_or_0 + sI16, GPR[r2], WORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	ST %s+%d,GPR%d\n", prevIAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				break;
			case 0xDE:
				// STC
				SCR.IAR = SCR.IAR+4;
				procwrite(r3_reg_or_0 + sI16, GPR[r2], BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STC %s+%d,GPR%d\n", prevIAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				break;
			case 0xE0:
				// ABS
				SCR.IAR = SCR.IAR+2;
				SCR.CS &= CS_MASK_Clear_OV;
				SCR.CS &= CS_MASK_Clear_C0;
				if ( GPR[r3] == 0x80000000 ) {
					GPR[r2] = ~GPR[r3] + 1;
					SCR.CS |= CS_MASK_OV;
				} else if ( GPR[r3] & 0x80000000 ) {
					GPR[r2] = ~GPR[r3] + 1;
				} else {
					GPR[r2] = GPR[r3];
				}
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		ABS GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE1:
				// A
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + (int32_t)GPR[r3];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		A GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE2:
				// S
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] - (int32_t)GPR[r3];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		S GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE3:
				// O
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		O GPR%d, GPR%d | GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE4:
				// TWOC
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = ~GPR[r3] + 1;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		TWOC GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = ~0x%08X + 1\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE5:
				// N
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		N GPR%d, GPR%d & GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE6:
				// M
				SCR.IAR = SCR.IAR+2;
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				//logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		M GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xE7:
				// X
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] ^ GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		X GPR%d, GPR%d ^ GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE8:
				// BNBR
				SCR.IAR = SCR.IAR+2;
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBR %s,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
			case 0xE9:
				// BNBRX
				SCR.IAR = SCR.IAR+2;
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBRX %s,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xEB:
				// LHS
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = procread(GPR[r3], HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHS GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xEC:
				// BALR
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = SCR.IAR;
				SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				break;
			case 0xED:
				// BALRX
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = SCR.IAR+4;
				SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				// TODO: Subject Instruction Exec
				break;
			case 0xEE:
				// BBR
				SCR.IAR = SCR.IAR+2;
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBR %s,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xEF:
				// BBRX
				SCR.IAR = SCR.IAR+2;
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBRX %s,GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xF0:
				// WAIT
				SCR.IAR = SCR.IAR+2;
				wait = 1;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	WAIT\n", prevIAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xF1:
				// AE
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + (int32_t)GPR[r3] + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		AE GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X + CO:%d\n", GPR[r2], prevVal, GPR[r3], ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xF2:
				// SE
				SCR.IAR = SCR.IAR+2;
				arith_result = (int32_t)GPR[r2] + (int32_t)(~GPR[r3]) + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SE GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + ~0x%08X + CO:%d\n", GPR[r2], prevVal, GPR[r3], ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xF3:
				// CA16
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r3] & 0xFFFF0000) | (GPR[r2] & 0x0000FFFF) + (GPR[r3] & 0x0000FFFF);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CA16 GPR%d, GPR%d+GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X + 0x%08X\n", GPR[r2], prevVal, (GPR[r2] & 0x0000FFFF), (GPR[r3] & 0x0000FFFF));
				break;
			case 0xF4:
				// ONEC
				SCR.IAR = SCR.IAR+2;
				GPR[r2] = ~GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		ONEC GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = ~0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xF5:
				// CLZ
				SCR.IAR = SCR.IAR+2;
				for (int i = 0; i < 16; i++) {
					if ( ~GPR[r3] & (0x00008000 >> i) ) {
						GPR[r2] = i;
						break;
					}
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLZ GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xF9:
				// MC03
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0x00FFFFFF) | ((GPR[r3] & 0x000000FF) << 24);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC03 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFA:
				// MC13
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFF00FFFF) | ((GPR[r3] & 0x000000FF) << 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC13 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFB:
				// MC23
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFF00FF) | ((GPR[r3] & 0x000000FF) << 8);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC23 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFC:
				// MC33
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | GPR[r3] & 0x000000FF;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC33 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFD:
				// MC30
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0xFF000000) >> 24);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC30 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFE:
				// MC31
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0x00FF0000) >> 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC31 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFF:
				// MC32
				SCR.IAR = SCR.IAR+2;
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0x0000FF00) >> 8);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC32 GPR%d, GPR%d\n", prevIAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			default:
				// Unexpected Instruction Program-Check
				logmsgf(LOGPROC, "PROC: Error unexpected Instruction IAR: 0x%08X, Instruction Word: 0x%08X\n", SCR.IAR, inst);
				break;
		}
	}
}