// ROMP CPU Emulation
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <romp.h>
#include <mmu.h>
#include <logfac.h>

uint32_t GPR[16];
union SCRs SCR;

void procinit (void) {
	for (uint8_t i=0; i < 16; i++) {
		GPR[i] = 0x00000000;
		SCR._direct[i] = 0x00000000;
	}
}

void z_lt_eq_gt_flag_check (uint32_t val) {
	if (val == 0x00000000) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= CS_MASK_EQ;
	} else if (val & 0x80000000) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= CS_MASK_LT;
	} else if (val & 0x7FFFFFFF) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= CS_MASK_GT;
	}
}

void c0_flag_check (uint64_t val) {
	if (val == 0x0000000100000000) {
		SCR.CS &= 0xFFFFFFF5;
		SCR.CS |= CS_MASK_C0;
	}
}

void ov_flag_check (uint64_t val) {
	if (val < -2147483648 || val > 2147483647) {
		SCR.CS &= 0xFFFFFFF5;
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
	inst = procread(SCR.IAR, WORD, MEMORY);
	SCR.IAR = decode(inst);
}

uint32_t decode (uint32_t inst) {
	uint8_t nibble0 = (inst & 0xF0000000) >> 28;
	uint8_t r1 = (inst & 0x0F000000) >> 24;
	uint8_t r2 = (inst & 0x00F00000) >> 20;
	uint8_t r3 = (inst & 0x000F0000) >> 16;
	uint8_t byte0 = (inst & 0xFF000000) >> 24;
	uint32_t r3_reg_or_0 = r3 ? GPR[r3] : 0;
	uint32_t BA = (inst & 0x00FFFFFE);
	uint16_t I16 = inst & 0x00007FFF;
	// From simple sign extention to int8_t for addr calculation
	int8_t JI = inst & 0x00800000 ? -((inst & 0x007F0000) >> 15): (inst & 0x007F0000) >> 15;
	// From simple sign extention to int16_t for addr calculation
	int16_t sI16 = inst & 0x00008000 ? -(inst & 0x00007FFF) : inst & 0x00007FFF;

	uint32_t nextIAR;
	uint32_t addr;
	int64_t arith_result;

	if (nibble0 < 8) {
		nextIAR = SCR.IAR+2;
		// JI, X, D-Short format Instructions
		switch(nibble0) {
			case 0:
				if (inst & 0x0800) {
					// JB
					if ( (SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						nextIAR = SCR.IAR + JI;
					}
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JB %s,%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
				} else {
					// JNB
					if ( !(SCR.CS & (0x80 >> (r1 & 0x7))) ) {
						nextIAR = SCR.IAR + JI;
					}
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		JNB %s,%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname((r1 & 0x7)+8), JI);
				}
				break;
			case 1:
				// STCS
					procwrite(r3_reg_or_0 + r1, GPR[r2], BYTE, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STCS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1, r2);
				break;
			case 2:
				// STHS
					procwrite(r3_reg_or_0 + (r1 << 1), GPR[r2], HALFWORD, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STHS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 1, r2);
				break;
			case 3:
				// STS
					procwrite(r3_reg_or_0 + (r1 << 2), GPR[r2], WORD, MEMORY);
					logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		STS %s+%d,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, gpr_or_0(r3), r1 << 2, r2);
				break;
			case 4:
				// LCS
				GPR[r2] = procread(r3_reg_or_0 + r1, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LCS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1);
				break;
			case 5:
				// LHAS
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (r1 << 1), HALFWORD, MEMORY);;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHAS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), r1 << 1);
				break;
			case 6:
				// CAS
				GPR[r1] = GPR[r2]+r3_reg_or_0;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CAS GPR%d, GPR%d+%s\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r1, r2, gpr_or_0(r3));
				break;
			case 7:
				// LS
				GPR[r2] = procread(r3_reg_or_0 + (r1 << 2), HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LS GPR%d, %s+%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, gpr_or_0(r3), (r1 << 2));
				break;
		}
	} else {
		nextIAR = SCR.IAR+4;
		// R, BI, BA, D format Instructions
		switch(byte0) {
			case 0x88:
				// BNB
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = SCR.IAR + (sI16 << 1);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNB %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x89:
				// BNBX
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = SCR.IAR + (sI16 << 1);
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BNBX %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x8A:
				// BALA
				GPR[15] = nextIAR;
				nextIAR = BA;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALA 0x%06X\n", SCR.IAR, inst, BA);
				break;
			case 0x8B:
				// BALAX
				GPR[15] = nextIAR+4;
				nextIAR = BA;
				// TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALAX 0x%06X\n	SUB", SCR.IAR, inst, BA);
				break;
			case 0x8C:
				// BALI
				GPR[r2] = nextIAR;
				nextIAR = sI16 << 1;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALI GPR%d, %d\n", SCR.IAR, inst, r2, sI16 << 1);
				break;
			case 0x8D:
				// BALIX
				GPR[r2] = nextIAR+4;
				nextIAR = sI16 << 1;
				// TODO: Subject Instruction Exec
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BALIX GPR%d, %d\n SUB", SCR.IAR, inst, r2, sI16 << 1);
				break;
			case 0x8E:
				// BB
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = SCR.IAR + (sI16 << 1);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BB %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x8F:
				// BBX
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = SCR.IAR + (sI16 << 1);
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	BBX %s,%d\n", SCR.IAR, inst, getCSname(r2), sI16 << 1);
				break;
			case 0x90:
				// AIS
				
				break;
			case 0x91:
				// INC
				GPR[r2] = GPR[r2] + r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		INC GPR%d, GPR%d+%02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0x92:
				// SIS
				
				break;
			case 0x93:
				// DEC
				GPR[r2] = GPR[r2] - r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		DEC GPR%d, GPR%d-%02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0x94:
				// CIS
				
				break;
			case 0x95:
				// CLRSB
				
				break;
			case 0x96:
				// MFS
				
				break;
			case 0x97:
				// SETSB
				
				break;
			case 0x98:
				// CLRBU
				GPR[r2] = GPR[r2] & ~(0x80000000 >> r3);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x99:
				// CLRBL
				GPR[r2] = GPR[r2] & ~(0x00008000 >> r3);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLRBL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9A:
				// SETBU
				GPR[r2] = GPR[r2] | (0x80000000 >> r3);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9B:
				// SETBL
				GPR[r2] = GPR[r2] | (0x00008000 >> r3);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SETBL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9C:
				// MFTBIU
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> r3)) | ((SCR.CS & CS_MASK_TB) << 31 - r3);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9D:
				// MFTBIL
				GPR[r2] = (GPR[r2] & ~(0x00008000 >> r3)) | ((SCR.CS & CS_MASK_TB) << 15 - r3);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTBIL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9E:
				// MTTBIU
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x80000000 >> r3) >> (31 - r3)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIU GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0x9F:
				// MTTBIL
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x00008000 >> r3) >> (15 - r3)));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTBIL GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA0:
				// SARI
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> r3);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA1:
				// SARI16
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (r3+16));
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SARI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA4:
				// LIS
				GPR[r2] = r3;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LIS GPR%d, %02X\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA8:
				// SRI
				GPR[r2] = GPR[r2] >> r3;
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xA9:
				// SRI16
				GPR[r2] = GPR[r2] >> (r3+16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAA:
				// SLI
				GPR[r2] = GPR[r2] << r3;
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAB:
				// SLI16
				GPR[r2] = GPR[r2] << (r3+16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAC:
				// SRPI
				GPR[r2^0x01] = GPR[r2] >> r3;
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAD:
				// SRPI16
				GPR[r2^0x01] = GPR[r2] >> (r3+16);
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRPI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAE:
				// SLPI
				GPR[r2^0x01] = GPR[r2] << r3;
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI GPR%d, %d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xAF:
				// SLPI16
				GPR[r2^0x01] = GPR[r2] << (r3+16);
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLPI16 GPR%d, %d+16\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xB0:
				// SAR
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (GPR[r3] & 0x0000003F));
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SAR GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xB1:
				// EXTS
				
				break;
			case 0xB2:
				// SF
				
				break;
			case 0xB3:
				// CL
				
				break;
			case 0xB4:
				// C
				
				break;
			case 0xB5:
				// MTS
				if (r2 == 13) {logmsgf(LOGPROC, "WARNING: MTS SCR13 is unpredictable. IAR: 0x%08X\n", SCR.IAR);}
				SCR._direct[r2] = GPR[r3];
				break;
			case 0xB6:
				// D
				
				break;
			case 0xB8:
				// SR
				GPR[r2] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SR GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xB9:
				// SRP
				GPR[r2^0x01] = GPR[r2] >> GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SRP GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xBA:
				// SL
				GPR[r2] = GPR[r2] << GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SL GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xBB:
				// SLP
				GPR[r2^0x01] = GPR[r2] << GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2^0x01]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		SLP GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xBC:
				// MFTB
				GPR[r2] = (GPR[r2] & ~(0x80000000 >> (GPR[r3] & 0x0000001F))) | ((SCR.CS & 0x00000001) << 31 - (GPR[r3] & 0x0000001F));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MFTB GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xBD:
				// TGTE
				
				break;
			case 0xBE:
				// TLT
				
				break;
			case 0xBF:
				// MTTB
				SCR.CS = (SCR.CS & CS_MASK_Clear_TB) | ((GPR[r2] & (0x80000000 >> (GPR[r3] & 0x0000001F))) >> 31 - (GPR[r3] & 0x0000001F));
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MTTB GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xC0:
				// SVC
				
				break;
			case 0xC1:
				// AI
				
				break;
			case 0xC2:
				// CAL16
				GPR[r2] = (GPR[r2] & 0xFFFF0000) | ((r3_reg_or_0 + I16) & 0x0000FFFF);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL16 GPR%d, %s+%04X\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				break;
			case 0xC3:
				// OIU
				GPR[r2] = GPR[r3] | (I16 << 16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIU GPR%d, GPR%d | 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xC4:
				// OIL
				GPR[r2] = GPR[r3] | I16;
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	OIL GPR%d, GPR%d | 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xC5:
				// NILZ
				GPR[r2] = GPR[r3] & I16;
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILZ GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xC6:
				// NILO
				GPR[r2] = GPR[r3] & ( 0xFFFF0000 | I16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NILO GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xC7:
				// XIL
				GPR[r2] = GPR[r3] ^ I16;
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIL GPR%d, GPR%d ^ 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xC8:
				// CAL
				GPR[r2] = r3_reg_or_0 + I16;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAL GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				break;
			case 0xC9:
				// LM
				for (int i = r2; i < 16; i++) {
					GPR[i] = procread(r3_reg_or_0 + sI16, WORD, MEMORY);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LM GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				break;
			case 0xCA:
				// LHA
				GPR[r2] = (int16_t)procread(r3_reg_or_0 + (sI16 << 1), HALFWORD, MEMORY);;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X		LHA GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				break;
			case 0xCB:
				// IOR
				addr = r3_reg_or_0 + sI16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				GPR[r2] = procread(addr, WORD, PIO);
				break;
			case 0xCC:
				// TI
				
				break;
			case 0xCD:
				// L
				GPR[r2] = procread(r3_reg_or_0 + sI16, WORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	L GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				break;
			case 0xCE:
				// LC
				GPR[r2] = procread(r3_reg_or_0 + sI16, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LC GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				break;
			case 0xCF:
				// TSH
				GPR[r2] = procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY);
				procwrite(r3_reg_or_0 + sI16 - 1, 0xFF, BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		TSH GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16 << 1);
				break;
			case 0xD0:
				// LPS
				
				break;
			case 0xD1:
				// AEI
				
				break;
			case 0xD2:
				// SFI
				
				break;
			case 0xD3:
				// CLI
				
				break;
			case 0xD4:
				// CI
				
				break;
			case 0xD5:
				// NIUZ
				GPR[r2] = GPR[r3] & (I16 << 16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUZ GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xD6:
				// NIUO
				GPR[r2] = GPR[r3] & ((I16 << 16) | 0x0000FFFF);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	NIUO GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xD7:
				// XIU
				GPR[r2] = GPR[r3] ^ (I16 << 16);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	XIU GPR%d, GPR%d & 0x%04X\n", SCR.IAR, inst, r2, r3, I16);
				break;
			case 0xD8:
				// CAU
				GPR[r2] = r3_reg_or_0 + (I16 << 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	CAU GPR%d, %s+0x%04X\n", SCR.IAR, inst, r2, gpr_or_0(r3), I16);
				break;
			case 0xD9:
				// STM
				for (int i = r2; i < 16; i++) {
					procwrite(r3_reg_or_0 + sI16, GPR[i], WORD, MEMORY);
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STM %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				break;
			case 0xDA:
				// LH
				GPR[r2] = (GPR[r2] & 0xFFFF0000) | procread(r3_reg_or_0 + sI16, HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	LH GPR%d, %s+%d\n", SCR.IAR, inst, r2, gpr_or_0(r3), sI16);
				break;
			case 0xDB:
				// IOW
				addr = r3_reg_or_0 + sI16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				procwrite(addr, GPR[r2], WORD, PIO);
				break;
			case 0xDC:
				// STH
				procwrite(r3_reg_or_0 + sI16, GPR[r2], HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STH %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				break;
			case 0xDD:
				// ST
				procwrite(r3_reg_or_0 + sI16, GPR[r2], WORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	ST %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				break;
			case 0xDE:
				// STC
				procwrite(r3_reg_or_0 + sI16, GPR[r2], BYTE, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%08X	STC %s+%d,GPR%d\n", SCR.IAR, inst, gpr_or_0(r3), sI16, r2);
				break;
			case 0xE0:
				// ABS
				
				break;
			case 0xE1:
				// A
				arith_result = (int32_t)GPR[r2] + (int32_t)GPR[r3];
				GPR[r2] = arith_result & 0x00000000FFFFFFFF;
				c0_flag_check(arith_result);
				ov_flag_check(arith_result);
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		A GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xE2:
				// S
				
				break;
			case 0xE3:
				// O
				GPR[r2] = GPR[r2] | GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		O GPR%d, GPR%d | GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0xE4:
				// TWOC
				
				break;
			case 0xE5:
				// N
				GPR[r2] = GPR[r2] & GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		N GPR%d, GPR%d & GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0xE6:
				// M
				
				break;
			case 0xE7:
				// X
				GPR[r2] = GPR[r2] ^ GPR[r3];
				z_lt_eq_gt_flag_check(GPR[r2]);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		X GPR%d, GPR%d ^ GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0xE8:
				// BNBR
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = GPR[r3] & 0xFFFFFFFE;
				}
				break;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBR %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
			case 0xE9:
				// BNBRX
				if ( !(SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = GPR[r3] & 0xFFFFFFFE;
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BNBRX %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xEB:
				// LHS
				GPR[r2] = procread(GPR[r3], HALFWORD, MEMORY);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		LHS GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xEC:
				// BALR
				GPR[r2] = nextIAR;
				nextIAR = GPR[r3] & 0xFFFFFFFE;
				break;
			case 0xED:
				// BALRX
				GPR[r2] = nextIAR+4;
				nextIAR = GPR[r3] & 0xFFFFFFFE;
				// TODO: Subject Instruction Exec
				break;
			case 0xEE:
				// BBR
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = GPR[r3] & 0xFFFFFFFE;
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBR %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xEF:
				// BBRX
				if ( (SCR.CS & (0x8000 >> r2)) ) {
					nextIAR = GPR[r3] & 0xFFFFFFFE;
					// TODO: Subject Instruction Exec
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X	BBRX %s,GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, getCSname(r2), r3);
				break;
			case 0xF0:
				// WAIT
				
				break;
			case 0xF1:
				// AE
				
				break;
			case 0xF2:
				// SE
				
				break;
			case 0xF3:
				// CA16
				GPR[r2] = (GPR[r3] & 0xFFFF0000) | (GPR[r2] & 0x0000FFFF) + (GPR[r3] & 0x0000FFFF);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CA16 GPR%d, GPR%d+GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r2, r3);
				break;
			case 0xF4:
				// ONEC
				
				break;
			case 0xF5:
				// CLZ
				for (int i = 0; i < 16; i++) {
					if ( ~GPR[r3] & (0x00008000 >> i) ) {
						GPR[r2] = i;
						break;
					}
				}
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		CLZ GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xF9:
				// MC03
				GPR[r2] = (GPR[r2] & 0x00FFFFFF) | (GPR[r3] & 0x000000FF << 24);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC03 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFA:
				// MC13
				GPR[r2] = (GPR[r2] & 0xFF00FFFF) | (GPR[r3] & 0x000000FF << 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC13 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFB:
				// MC23
				GPR[r2] = (GPR[r2] & 0xFFFF00FF) | (GPR[r3] & 0x000000FF << 8);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC23 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFC:
				// MC33
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | GPR[r3] & 0x000000FF;
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC33 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFD:
				// MC30
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | (GPR[r3] & 0xFF000000 >> 24);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC30 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFE:
				// MC31
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | (GPR[r3] & 0x00FF0000 >> 16);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC31 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			case 0xFF:
				// MC32
				GPR[r2] = (GPR[r2] & 0xFFFFFF00) | (GPR[r3] & 0x0000FF00 >> 8);
				logmsgf(LOGINSTR, "INSTR: 0x%08X: 0x%04X		MC32 GPR%d, GPR%d\n", SCR.IAR, (inst & 0xFFFF0000) >> 16, r2, r3);
				break;
			default:
				// Unexpected Instruction Program-Check
				logmsgf(LOGPROC, "ERROR: Unexpected Instruction IAR: 0x%08X, Instruction Word: 0x%08X\n", SCR.IAR, inst);
				break;
		}
	}
	return nextIAR;
}