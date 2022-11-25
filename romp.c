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

uint32_t* procinit (void) {
	wait = 0;
	for (uint8_t i=0; i < 16; i++) {
		GPR[i] = 0x00000000;
		SCR._direct[i] = 0x00000000;
	}
	// Initial IAR from 000000? pg. 11-140
	SCR.IAR = procread(SCR.IAR, WORD, MEMORY, TAG_PROC);
	return &GPR[0];
}

union SCRs* getSCRptr (void) {
	return &SCR;
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
	logmsgf(LOGPROC, "PROC: Error Program Check.\n");
	procwrite(PROG_STATUS_PC, SCR.IAR, WORD, MEMORY, TAG_PROC);
	procwrite(PROG_STATUS_PC+4, SCR.ICS, HALFWORD, MEMORY, TAG_PROC);
	procwrite(PROG_STATUS_PC+6, SCR.CS, HALFWORD, MEMORY, TAG_PROC);
	// TODO
	return;
}

uint32_t fetch (void) {
	uint32_t inst;
	// TODO: Interrupt, error, POR to clear wait state.
	if (wait) {return 1;}
	inst = procread(SCR.IAR, INST, MEMORY, TAG_PROC);
	decode(inst, NORMEXEC);
	return 0;
}

void decode (uint32_t inst, uint8_t mode) {
	uint8_t nibble0 = (inst & 0xF0000000) >> 28;
	uint8_t r1 = (inst & 0x0F000000) >> 24;
	uint8_t r2 = (inst & 0x00F00000) >> 20;
	uint8_t r3 = (inst & 0x000F0000) >> 16;
	uint8_t byte0 = (inst & 0xFF000000) >> 24;
	uint32_t r3_reg_or_0 = r3 ? GPR[r3] : 0;
	uint32_t BA = (inst & 0x00FFFFFE);
	uint16_t I16 = inst & 0x0000FFFF;
	// From simple sign extention to int8_t for addr calculation shifted left one bit
	int32_t JI = inst & 0x00800000 ? (((inst & 0x007F0000) >> 15) | 0xFFFFFF00) : (inst & 0x007F0000) >> 15;
	// From simple sign extention to int16_t for addr calculation
	int16_t sI16 = inst & 0x00008000 ? inst | 0xFFFF0000 : inst & 0x00007FFF;

	uint32_t addr;
	uint32_t prevVal;
	uint32_t instIAR = SCR.IAR;
	int64_t arith_result;

	if (nibble0 < 8) {
		// JI, X, D-Short format Instructions
		switch(nibble0) {
			case 0:
				if (inst & 0x08000000) {
					// JB
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JB %s,%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
					if (mode == DIRECTEXEC) {
						progcheck();
						return;
					}
					if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
					if ( (SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						SCR.IAR = instIAR + JI;
					}
				} else {
					// JNB
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JNB %s,%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
					if (mode == DIRECTEXEC) {
						progcheck();
						return;
					}
					if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
					if ( !(SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						SCR.IAR = instIAR + JI;
					}
				}
				break;
			case 1:
				// STCS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STCS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				procwrite(r3_reg_or_0 + r1, GPR[r2], BYTE, MEMORY, TAG_PROC);
				break;
			case 2:
				// STHS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STHS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 1, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1 << 1, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				procwrite(r3_reg_or_0 + (r1 << 1), GPR[r2], HALFWORD, MEMORY, TAG_PROC);
				break;
			case 3:
				// STS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 2, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d: 0x%08X\n", r3_reg_or_0, r1 << 2, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				procwrite(r3_reg_or_0 + (r1 << 2), GPR[r2], WORD, MEMORY, TAG_PROC);
				break;
			case 4:
				// LCS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LCS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = procread(r3_reg_or_0 + r1, BYTE, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1);
				break;
			case 5:
				// LHAS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHAS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1 << 1);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (r1 << 1), HALFWORD, MEMORY, TAG_PROC);;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1 << 1);
				break;
			case 6:
				// CAS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CAS GPR%d, GPR%d+%s\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r1, r2, gpr_or_0(r3));
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r1] = GPR[r2]+r3_reg_or_0;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r1], GPR[r2], r3_reg_or_0);
				break;
			case 7:
				// LS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), (r1 << 2));
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = procread(r3_reg_or_0 + (r1 << 2), WORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], r3_reg_or_0, r1 << 2);
				break;
		}
	} else {
		// R, BI, BA, D format Instructions
		switch(byte0) {
			case 0x88:
				// BNB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNB %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = instIAR + (sI16 << 1);
				}
				break;
			case 0x89:
				// BNBX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNBX %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					logmsgf(LOGINSTR, " SUB");
					decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
					SCR.IAR = instIAR + (sI16 << 1);
				}
				break;
			case 0x8A:
				// BALA
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALA 0x%06X\n", SCR.IAR, inst, BA);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[15] = SCR.IAR;
				SCR.IAR = BA;
				logmsgf(LOGINSTR, "			GPR15: 0x%08X\n", GPR[15]);
				break;
			case 0x8B:
				// BALAX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALAX 0x%06X\n	SUB", SCR.IAR, inst, BA);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[15] = SCR.IAR+4;
				logmsgf(LOGINSTR, "			GPR15: 0x%08X\n", GPR[15]);
				decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
				SCR.IAR = BA;
				break;
			case 0x8C:
				// BALI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALI GPR%d, %d\n", SCR.IAR, inst, r2, sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = SCR.IAR;
				SCR.IAR = instIAR + (sI16 << 1);
				logmsgf(LOGINSTR, "			GPR%d: 0x%08X\n", r2, GPR[r2]);
				break;
			case 0x8D:
				// BALIX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALIX GPR%d, %d\n SUB", SCR.IAR, inst, r2, sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = SCR.IAR;
				logmsgf(LOGINSTR, "			GPR%d: 0x%08X\n", r2, GPR[r2]);
				decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
				SCR.IAR = instIAR + (sI16 << 1);
				break;
			case 0x8E:
				// BB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BB %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = instIAR + (sI16 << 1);
				}
				break;
			case 0x8F:
				// BBX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BBX %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					logmsgf(LOGINSTR, " SUB");
					decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
					SCR.IAR = instIAR + (sI16 << 1);
				}
				break;
			case 0x90:
				// AIS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		AIS GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + r3;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x91:
				// INC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		INC GPR%d, GPR%d+%02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] + r3;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x92:
				// SIS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SIS GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] - r3;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0x93:
				// DEC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		DEC GPR%d, GPR%d-%02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] - r3;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - %d\n", GPR[r2], prevVal, r3);
				break;
			case 0x94:
				// CIS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CIS GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				algebretic_cmp(GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0x95:
				// CLRSB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRSB SCR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = SCR._direct[r2];
				SCR._direct[r2] = SCR._direct[r2] & ~(0x00008000 >> r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", SCR._direct[r2], prevVal, ~(0x00008000 >> r3));
				break;
			case 0x96:
				// MFS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFS SCR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r3] = SCR._direct[r2];
				logmsgf(LOGINSTR, "			0x%08X\n", GPR[r3]);
				break;
			case 0x97:
				// SETSB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETSB SCR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = SCR._direct[r2];
				SCR._direct[r2] = SCR._direct[r2] | (0x00008000 >> r3);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", SCR._direct[r2], prevVal, (0x00008000 >> r3));
				break;
			case 0x98:
				// CLRBU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & ~(0x80000000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, ~(0x80000000 >> r3));
				break;
			case 0x99:
				// CLRBL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & ~(0x00008000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, ~(0x00008000 >> r3));
				break;
			case 0x9A:
				// SETBU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | (0x80000000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, (0x80000000 >> r3));
				break;
			case 0x9B:
				// SETBL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | (0x00008000 >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, (0x00008000 >> r3));
				break;
			case 0x9C:
				// MFTBIU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> r3)) | ((SCR.CS & CS_MASK_TB) << (31 - r3));
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & CS_MASK_TB) << (31 - r3)));
				break;
			case 0x9D:
				// MFTBIL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x00008000 >> r3)) | ((SCR.CS & CS_MASK_TB) << (15 - r3));
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & CS_MASK_TB) << (15 - r3)));
				break;
			case 0x9E:
				// MTTBIU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x80000000 >> r3));
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | (((GPR[r2] & (0x80000000 >> r3)) >> (31 - r3)));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0x9F:
				// MTTBIL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x00008000 >> r3));
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | (((GPR[r2] & (0x00008000 >> r3)) >> (15 - r3)));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0xA0:
				// SARI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> r3);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA1:
				// SARI16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (r3+16));
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA4:
				// LIS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LIS GPR%d, %02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = r3;
				break;
			case 0xA8:
				// SRI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> r3;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xA9:
				// SRI16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> (r3+16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAA:
				// SLI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << r3;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAB:
				// SLI16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << (r3+16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAC:
				// SRPI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] >> r3;
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAD:
				// SRPI16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] >> (r3+16);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAE:
				// SLPI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] << r3;
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], r3);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xAF:
				// SLPI16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] << (r3+16);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], r3+16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB0:
				// SAR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SAR GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (GPR[r3] & 0x0000003F));
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB1:
				// EXTS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		EXTS GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = GPR[r3] & 0x00008000 ? GPR[r3] | 0xFFFF0000 : GPR[r3] & 0x00007FFF;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB2:
				// SF
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SF GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r3] - (int32_t)GPR[r2];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], GPR[r3], prevVal);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB3:
				// CL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CL GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				logical_cmp(GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB4:
				// C
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		C GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				algebretic_cmp(GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB5:
				// MTS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTS SCR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X\n", GPR[r3]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				if (r2 == 13) {logmsgf(LOGPROC, "PROC: Warning MTS SCR13 is unpredictable. IAR: 0x%08X\n", SCR.IAR);}
				SCR._direct[r2] = GPR[r3];
				break;
			case 0xB6:
				// D
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				//logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		D GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xB8:
				// SR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SR GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xB9:
				// SRP
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRP GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X >> %d\n", GPR[r2^0x01], GPR[r2], (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBA:
				// SL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SL GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] << (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2], prevVal, (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBB:
				// SLP
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLP GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2^0x01] = GPR[r2] << (GPR[r3] & 0x0000003F);
				lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X << %d\n", GPR[r2^0x01], GPR[r2], (GPR[r3] & 0x0000003F));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xBC:
				// MFTB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTB GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> (GPR[r3] & 0x0000001F))) | ((SCR.CS & 0x00000001) << (31 - (GPR[r3] & 0x0000001F)));
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, ((SCR.CS & 0x00000001) << (31 - (GPR[r3] & 0x0000001F))));
				break;
			case 0xBD:
				// TGTE
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xBE:
				// TLT
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xBF:
				// MTTB
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTB GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], (0x80000000 >> (GPR[r3] & 0x0000001F)));
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x80000000 >> (GPR[r3] & 0x0000001F))) >> (31 - (GPR[r3] & 0x0000001F)));
				logmsgf(LOGINSTR, "			Flags: TB:%d\n", (SCR.CS & CS_MASK_TB));
				break;
			case 0xC0:
				// SVC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	SVC %s+%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning SVC Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				procwrite(0x00000190, SCR.IAR, WORD, MEMORY, TAG_PROC);
				procwrite(0x00000194, SCR.ICS, HALFWORD, MEMORY, TAG_PROC);
				procwrite(0x00000196, SCR.CS, HALFWORD, MEMORY, TAG_PROC);
				procwrite(0x0000019E, r3_reg_or_0 + I16, HALFWORD, MEMORY, TAG_PROC);
				SCR.IAR = procread(0x00000198, WORD, MEMORY, TAG_PROC);
				SCR.ICS = procread(0x0000019C, HALFWORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			Regs: IAR: 0x%08X ICS: 0x%08X\n", SCR.IAR, SCR.ICS);
				break;
			case 0xC1:
				// AI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AI GPR%d, GPR%d+%d\n", SCR.IAR, inst, r2, r3, sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				arith_result = (int32_t)GPR[r3] + (int32_t)sI16;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + %d\n", GPR[r2], GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC2:
				// CAL16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL16 GPR%d, %s+%04X\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				prevVal = GPR[r2];
				GPR[r2] = (r3_reg_or_0 & 0xFFFF0000) | (((r3_reg_or_0 & 0x0000FFFF) + I16) & 0x0000FFFF);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X + 0x%08X\n", GPR[r2], prevVal, (r3_reg_or_0 & 0x0000FFFF), I16);
				break;
			case 0xC3:
				// OIU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIU GPR%d, GPR%d | 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] | (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], GPR[r3], I16 << 16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC4:
				// OIL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIL GPR%d, GPR%d | 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] | I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC5:
				// NILZ
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILZ GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] & I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC6:
				// NILO
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILO GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] & (0xFFFF0000 | I16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], (0xFFFF0000 | I16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC7:
				// XIL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIL GPR%d, GPR%d ^ 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] ^ I16;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], GPR[r3], I16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xC8:
				// CAL
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = r3_reg_or_0 + sI16;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, sI16);
				break;
			case 0xC9:
				// LM
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LM GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				for (int i = r2; i < 16; i++) {
					GPR[i] = procread(r3_reg_or_0 + sI16 + ((i - r2) << 2), WORD, MEMORY, TAG_PROC);
					logmsgf(LOGINSTR, "			0x%08X, 0x%08X + 0x%08X + %d\n", GPR[i], r3_reg_or_0, I16, ((i - r2) << 2));
				}
				break;
			case 0xCA:
				// LHA
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LHA GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (sI16 << 1), HALFWORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], r3_reg_or_0 + (sI16 << 1));
				break;
			case 0xCB:
				// IOR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	IOR GPR%d, %s+0x%04X\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				addr = r3_reg_or_0 + I16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				GPR[r2] = procread(addr, WORD, PIO, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0, I16);
				break;
			case 0xCC:
				// TI
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				break;
			case 0xCD:
				// L
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	L GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = procread(r3_reg_or_0 + sI16, WORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				break;
			case 0xCE:
				// LC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LC GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = procread(r3_reg_or_0 + sI16, BYTE, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				break;
			case 0xCF:
				// TSH
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		TSH GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + %d\n", GPR[r2], r3_reg_or_0,  sI16);
				procwrite(r3_reg_or_0 + sI16 - 1, 0xFF, BYTE, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			SET: 0x%08X + %d, 0xFF\n", r3_reg_or_0,  sI16-1);
				break;
			case 0xD0:
				// LPS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LPS 0x%X, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				logmsgf(LOGINSTR, "			0x%08X + %d\n", r3_reg_or_0,  sI16);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (r2 & 0xC) {logmsgf(LOGPROC, "PROC: Warning LPS Nibble2 upper bits should be zero. IAR: 0x%08X\n", SCR.IAR);}
				SCR.IAR = procread(r3_reg_or_0 + sI16, WORD, MEMORY, TAG_PROC);
				SCR.ICS = procread(r3_reg_or_0 + sI16 + 4, HALFWORD, MEMORY, TAG_PROC);
				SCR.CS = procread(r3_reg_or_0 + sI16 + 6, HALFWORD, MEMORY, TAG_PROC);
				SCR.MCSPCS = 0;
				logmsgf(LOGINSTR, "			Regs: IAR: 0x%08X ICS: 0x%08X CS: 0x%08X\n", SCR.IAR, SCR.ICS, SCR.CS);
				// TODO: if machine check level, MCS content set to 0
				// TODO: if bit 10, pending mem operations restarted before instr execution resumed, ECR (SCR 9) contains count and mem addr.
				// If bit 11, interrupts remain pending until target instr executed
				if (inst & 0x00100000) {
					// Execute the next Instr immediately to avoid taking an interrupt inbetween.
					decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), NORMEXEC);
				}
				break;
			case 0xD1:
				// AEI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AEI GPR%d, GPR%d+%d\n", SCR.IAR, inst, r2, r3, sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				arith_result = (int32_t)GPR[r3] + (int32_t)sI16 + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X + CO:%d\n", GPR[r2], GPR[r3], sI16, ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD2:
				// SFI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	AEI GPR%d, GPR%d+%d\n", SCR.IAR, inst, r2, r3, sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				arith_result = (int32_t)GPR[r3] - (int32_t)sI16;
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD3:
				// CLI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CLI GPR%d, %d\n", SCR.IAR, inst, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r3], sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning CLI Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				logical_cmp(GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD4:
				// CI
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CI GPR%d, %d\n", SCR.IAR, inst, r3, sI16);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r3], sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				if (r2 != 0x0) {logmsgf(LOGPROC, "PROC: Warning CI Nibble2 should be zero. IAR: 0x%08X\n", SCR.IAR);}
				algebretic_cmp(GPR[r3], sI16);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD5:
				// NIUZ
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUZ GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] & (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], (I16 << 16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD6:
				// NIUO
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUO GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] & ((I16 << 16) | 0x0000FFFF);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], GPR[r3], ((I16 << 16) | 0x0000FFFF));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD7:
				// XIU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIU GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = GPR[r3] ^ (I16 << 16);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], GPR[r3], (I16 << 16));
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xD8:
				// CAU
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAU GPR%d, %s+0x%04X\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = r3_reg_or_0 + (I16 << 16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, (I16 << 16));
				break;
			case 0xD9:
				// STM
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STM %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				for (int i = r2; i < 16; i++) {
					logmsgf(LOGINSTR, "			0x%08X + 0x%08X + %d, 0x%08X\n", r3_reg_or_0, I16, ((i - r2) << 2), GPR[i]);
					procwrite(r3_reg_or_0 + sI16 + ((i - r2) << 2), GPR[i], WORD, MEMORY, TAG_PROC);
				}
				break;
			case 0xDA:
				// LH
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LH GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X + 0x%08X\n", GPR[r2], r3_reg_or_0, sI16);
				break;
			case 0xDB:
				// IOW
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	IOW %s+0x%04X, GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), I16, r2);
				logmsgf(LOGINSTR, "			0x%08X + 0x%08X, 0x%08X\n", r3_reg_or_0, I16, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				addr = r3_reg_or_0 + I16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				procwrite(addr, GPR[r2], WORD, PIO, TAG_PROC);
				break;
			case 0xDC:
				// STH
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STH %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				procwrite(r3_reg_or_0 + sI16, GPR[r2], HALFWORD, MEMORY, TAG_PROC);
				break;
			case 0xDD:
				// ST
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	ST %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				procwrite(r3_reg_or_0 + sI16, GPR[r2], WORD, MEMORY, TAG_PROC);
				break;
			case 0xDE:
				// STC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STC %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				logmsgf(LOGINSTR, "			0x%08X + %d, 0x%08X\n", r3_reg_or_0, sI16, GPR[r2]);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				procwrite(r3_reg_or_0 + sI16, GPR[r2], BYTE, MEMORY, TAG_PROC);
				break;
			case 0xE0:
				// ABS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		ABS GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
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
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE1:
				// A
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		A GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + (int32_t)GPR[r3];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE2:
				// S
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		S GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] - (int32_t)GPR[r3];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X - 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE3:
				// O
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		O GPR%d, GPR%d | GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] | GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE4:
				// TWOC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		TWOC GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = ~GPR[r3] + 1;
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = ~0x%08X + 1\n", GPR[r2], GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE5:
				// N
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		N GPR%d, GPR%d & GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] & GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X & 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE6:
				// M
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				logmsgf(LOGPROC, "PROC: Error instruction to be implemented. IAR: 0x%08X\n", SCR.IAR);
				//logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		M GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xE7:
				// X
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		X GPR%d, GPR%d ^ GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = GPR[r2] ^ GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X ^ 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				logmsgf(LOGINSTR, "			Flags: LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xE8:
				// BNBR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBR %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
			case 0xE9:
				// BNBRX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBRX %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					logmsgf(LOGINSTR, " SUB");
					decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
			case 0xEB:
				// LHS
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHS GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = procread(GPR[r3], HALFWORD, MEMORY, TAG_PROC);
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xEC:
				// BALR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		BALR GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = SCR.IAR;
				SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				logmsgf(LOGINSTR, "			GPR%d: 0x%08X\n", r2, GPR[r2]);
				break;
			case 0xED:
				// BALRX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		BALRX GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+4; }
				GPR[r2] = SCR.IAR;
				logmsgf(LOGINSTR, "			GPR%d: 0x%08X\n SUB", r2, GPR[r2]);
				decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
				SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				break;
			case 0xEE:
				// BBR
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBR %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
			case 0xEF:
				// BBRX
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBRX %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					logmsgf(LOGINSTR, " SUB");
					decode(procread(SCR.IAR, INST, MEMORY, TAG_PROC), DIRECTEXEC);
					SCR.IAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
			case 0xF0:
				// WAIT
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	WAIT\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				if (mode == DIRECTEXEC) {
					progcheck();
					return;
				}
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				wait = 1;
				break;
			case 0xF1:
				// AE
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		AE GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				arith_result = (int32_t)GPR[r2] + (int32_t)GPR[r3] + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + 0x%08X + CO:%d\n", GPR[r2], prevVal, GPR[r3], ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xF2:
				// SE
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SE GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				arith_result = (int32_t)GPR[r2] + (int32_t)(~GPR[r3]) + ((SCR.CS & CS_MASK_C0) >> 3);
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X + ~0x%08X + CO:%d\n", GPR[r2], prevVal, GPR[r3], ((SCR.CS & CS_MASK_C0) >> 3));
				logmsgf(LOGINSTR, "			Flags: C0:%d OV:%d LT:%d EQ:%d GT:%d\n", (SCR.CS & CS_MASK_C0) >> 3, (SCR.CS & CS_MASK_OV) >> 1, (SCR.CS & CS_MASK_LT) >> 6, (SCR.CS & CS_MASK_EQ) >> 5, (SCR.CS & CS_MASK_GT) >> 4);
				break;
			case 0xF3:
				// CA16
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CA16 GPR%d, GPR%d+GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r3] & 0xFFFF0000) | (GPR[r2] & 0x0000FFFF) + (GPR[r3] & 0x0000FFFF);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X | 0x%08X + 0x%08X\n", GPR[r2], prevVal, (GPR[r2] & 0x0000FFFF), (GPR[r3] & 0x0000FFFF));
				break;
			case 0xF4:
				// ONEC
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		ONEC GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				GPR[r2] = ~GPR[r3];
				lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "			0x%08X = ~0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xF5:
				// CLZ
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLZ GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				for (int i = 0; i < 16; i++) {
					if ( ~GPR[r3] & (0x00008000 >> i) ) {
						GPR[r2] = i;
						break;
					}
				}
				logmsgf(LOGINSTR, "			0x%08X, 0x%08X\n", GPR[r2], GPR[r3]);
				break;
			case 0xF9:
				// MC03
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC03 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0x00FFFFFF) | ((GPR[r3] & 0x000000FF) << 24);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFA:
				// MC13
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC13 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFF00FFFF) | ((GPR[r3] & 0x000000FF) << 16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFB:
				// MC23
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC23 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFF00FF) | ((GPR[r3] & 0x000000FF) << 8);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFC:
				// MC33
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC33 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | GPR[r3] & 0x000000FF;
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFD:
				// MC30
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC30 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0xFF000000) >> 24);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFE:
				// MC31
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC31 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0x00FF0000) >> 16);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			case 0xFF:
				// MC32
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC32 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				if (mode == NORMEXEC) { SCR.IAR = SCR.IAR+2; }
				prevVal = GPR[r2];
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | ((GPR[r3] & 0x0000FF00) >> 8);
				logmsgf(LOGINSTR, "			0x%08X = 0x%08X 0x%08X\n", GPR[r2], prevVal, GPR[r3]);
				break;
			default:
				// Unexpected Instruction Program-Check
				logmsgf(LOGPROC, "PROC: Error unexpected Instruction IAR: 0x%08X, Instruction Word: 0x%08X\n", SCR.IAR, inst);
				break;
		}
	}
}