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

void z_lt_eq_gt_flag_set (uint32_t val) {
	if (val == 0x00000000) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= 0x00000020;
	} else if (val & 0x80000000) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= 0x00000040;
	} else if (val & 0x7FFFFFFF) {
		SCR.CS &= 0xFFFFFF0F;
		SCR.CS |= 0x00000010;
	}
}

void progcheck (void) { 
	mmuwrite(PROG_STATUS_PC, SCR.IAR, WORD, DIRECT);
	mmuwrite(PROG_STATUS_PC+4, SCR.ICS, HALFWORD, DIRECT);
	mmuwrite(PROG_STATUS_PC+6, SCR.CS, HALFWORD, DIRECT);

}

uint32_t fetch (void) {
	uint32_t inst;
	inst = mmuread(SCR.IAR, WORD, TRANSLATE);
	SCR.IAR = decode(inst);
}

uint32_t decode (uint32_t inst) {
	uint8_t nibble0 = (inst & 0xF0000000) >> 28;
	uint8_t r1 = (inst & 0x0F000000) >> 24;
	uint8_t r2 = (inst & 0x00F00000) >> 20;
	uint8_t r3 = (inst & 0x000F0000) >> 16;
	uint8_t byte0 = (inst & 0xFF000000) >> 24;
	uint32_t r3_reg_or_0;
	if (r3 == 0) {r3_reg_or_0 = 0;} else {r3_reg_or_0 = GPR[r3];}
	uint16_t i16 = inst & 0x0000FFFF;

	uint32_t nextIAR;
	uint32_t addr;

	if (nibble0 < 8) {
		nextIAR = SCR.IAR+2;
		// JI, X, D-Short format Instructions
		switch(nibble0) {
			case 0:
				if (inst & 0x0800) {
					// JB
					
				} else {
					// JNB

				}
				break;
			case 1:
				// STCS

				break;
			case 2:
				// STHS

				break;
			case 3:
				// STS

				break;
			case 4:
				// LCS
				//GPR[r2] = 0x00000000 | r3_reg_or_0;
				break;
			case 5:
				// LHAS

				break;
			case 6:
				// CAS

				break;
			case 7:
				// LS
				
				break;
		}
	} else {
		nextIAR = SCR.IAR+4;
		// R, BI, BA, D format Instructions
		switch(byte0) {
			case 0x88:
				// BNB
				
				break;
			case 0x89:
				// BNBX
				
				break;
			case 0x8A:
				// BALA
				
				break;
			case 0x8B:
				// BALAX
				
				break;
			case 0x8C:
				// BALI
				
				break;
			case 0x8D:
				// BALIX
				
				break;
			case 0x8E:
				// BB
				
				break;
			case 0x8F:
				// BBX
				
				break;
			case 0x90:
				// AIS
				
				break;
			case 0x91:
				// INC
				
				break;
			case 0x92:
				// SIS
				
				break;
			case 0x93:
				// DEC
				
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
				
				break;
			case 0x99:
				// CLRBL
				
				break;
			case 0x9A:
				// SETBU
				
				break;
			case 0x9B:
				// SETBL
				
				break;
			case 0x9C:
				// MFTBIU
				
				break;
			case 0x9D:
				// MFTBIL
				
				break;
			case 0x9E:
				// MTTBIU
				
				break;
			case 0x9F:
				// MTTBIL
				
				break;
			case 0xA0:
				// SARI
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> r3);
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xA1:
				// SARI16
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (r3+16));
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xA4:
				// LIS
				
				break;
			case 0xA8:
				// SRI
				GPR[r2] = GPR[r2] >> r3;
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xA9:
				// SRI16
				GPR[r2] = GPR[r2] >> (r3+16);
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xAA:
				// SLI
				GPR[r2] = GPR[r2] << r3;
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xAB:
				// SLI16
				GPR[r2] = GPR[r2] << (r3+16);
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xAC:
				// SRPI
				GPR[r2^0x01] = GPR[r2] >> r3;
				z_lt_eq_gt_flag_set(GPR[r2^0x01]);
				break;
			case 0xAD:
				// SRPI16
				GPR[r2^0x01] = GPR[r2] >> (r3+16);
				z_lt_eq_gt_flag_set(GPR[r2^0x01]);
				break;
			case 0xAE:
				// SLPI
				GPR[r2^0x01] = GPR[r2] << r3;
				z_lt_eq_gt_flag_set(GPR[r2^0x01]);
				break;
			case 0xAF:
				// SLPI16
				GPR[r2^0x01] = GPR[r2] << (r3+16);
				z_lt_eq_gt_flag_set(GPR[r2^0x01]);
				break;
			case 0xB0:
				// SAR
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (GPR[r3] & 0x0000003F));
				z_lt_eq_gt_flag_set(GPR[r2]);
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
				if (r2 == 13) {logmsgf(LOGPROC, "WARNING: MTS SCR13 is unpredictable. IAR: 0x%08X", IAR);}
				SCR._direct[r2] = GPR[r3];
				break;
			case 0xB6:
				// D
				
				break;
			case 0xB8:
				// SR
				GPR[r2] = GPR[r2] >> (GPR[r3] & 0x0000003F);
				z_lt_eq_gt_flag_set(GPR[r2]);
				break;
			case 0xB9:
				// SRP
				
				break;
			case 0xBA:
				// SL
				
				break;
			case 0xBB:
				// SLP
				
				break;
			case 0xBC:
				// MFTB
				
				break;
			case 0xBD:
				// TGTE
				
				break;
			case 0xBE:
				// TLT
				
				break;
			case 0xBF:
				// MTTB
				
				break;
			case 0xC0:
				// SVC
				
				break;
			case 0xC1:
				// AI
				
				break;
			case 0xC2:
				// CAL16
				
				break;
			case 0xC3:
				// OIU
				
				break;
			case 0xC4:
				// OIL
				
				break;
			case 0xC5:
				// NILZ
				
				break;
			case 0xC6:
				// NILO
				
				break;
			case 0xC7:
				// XIL
				
				break;
			case 0xC8:
				// CAL
				
				break;
			case 0xC9:
				// LM
				
				break;
			case 0xCA:
				// LHA
				
				break;
			case 0xCB:
				// IOR
				addr = r3_reg_or_0 + i16;
				if (addr & 0xFF000000) {
					SCR.MCSPCS |= 0x00000082;
					progcheck();
				}
				GPR[r2] = ;
				break;
			case 0xCC:
				// TI
				
				break;
			case 0xCD:
				// L
				
				break;
			case 0xCE:
				// LC
				
				break;
			case 0xCF:
				// TSH
				
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
				
				break;
			case 0xD6:
				// NIUO
				
				break;
			case 0xD7:
				// XIU
				
				break;
			case 0xD8:
				// CAU
				
				break;
			case 0xD9:
				// STM
				
				break;
			case 0xDA:
				// LH
				
				break;
			case 0xDB:
				// IOW
				
				break;
			case 0xDC:
				// STH
				
				break;
			case 0xDD:
				// ST
				
				break;
			case 0xDE:
				// STC
				
				break;
			case 0xE0:
				// ABS
				
				break;
			case 0xE1:
				// A
				
				break;
			case 0xE2:
				// S
				
				break;
			case 0xE3:
				// O
				
				break;
			case 0xE4:
				// TWOC
				
				break;
			case 0xE5:
				// N
				
				break;
			case 0xE6:
				// M
				
				break;
			case 0xE7:
				// X
				
				break;
			case 0xE8:
				// BNBR
				
				break;
			case 0xE9:
				// BNBRX
				
				break;
			case 0xEB:
				// LHS
				
				break;
			case 0xEC:
				// BALR
				
				break;
			case 0xED:
				// BALRX
				
				break;
			case 0xEE:
				// BBR
				
				break;
			case 0xEF:
				// BBRX
				
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
				
				break;
			case 0xF4:
				// ONEC
				
				break;
			case 0xF5:
				// CLZ
				
				break;
			case 0xF9:
				// MC03
				
				break;
			case 0xFA:
				// MC13
				
				break;
			case 0xFB:
				// MC23
				
				break;
			case 0xFC:
				// MC33
				
				break;
			case 0xFD:
				// MC30
				
				break;
			case 0xFE:
				// MC31
				
				break;
			case 0xFF:
				// MC32
				
				break;
			default:
				// Unexpected Instruction Program-Check
				logmsgf(LOGPROC, "ERROR: Unexpected Instruction IAR: 0x%08X, Instruction Word: 0x%08X", SCR.IAR, inst);
				break;
		}
	}
	return nextIAR;
}