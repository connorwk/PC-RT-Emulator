// ROMP CPU emulation
#include <stdio.h>
#include <stdint.h>

uint32_t GPR[16];
uint32_t SCR[16];

uint32_t fetch () {
	// Fetch next instruction and increment IAR
}

void decode (uint32_t inst) {
	uint8_t nibble0 = (inst & 0xF0000000) >> 28;
	uint8_t r1 = (inst & 0x0F000000) >> 24;
	uint8_t r2 = (inst & 0x00F00000) >> 20;
	uint8_t r3 = (inst & 0x000F0000) >> 16;
	uint8_t byte0 = (inst & 0xFF000000) >> 24;
	if (nibble0 < 8) {
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
				break;
			case 0xA1:
				// SARI16
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (r3+16));
				break;
			case 0xA4:
				// LIS
				
				break;
			case 0xA8:
				// SRI
				GPR[r2] = GPR[r2] >> r3;
				break;
			case 0xA9:
				// SRI16
				GPR[r2] = GPR[r2] >> (r3+16);
				break;
			case 0xAA:
				// SLI
				GPR[r2] = GPR[r2] << r3;
				break;
			case 0xAB:
				// SLI16
				GPR[r2] = GPR[r2] << (r3+16);
				break;
			case 0xAC:
				// SRPI
				GPR[r2^0x01] = GPR[r2] >> r3;
				break;
			case 0xAD:
				// SRPI16
				GPR[r2^0x01] = GPR[r2] >> (r3+16);
				break;
			case 0xAE:
				// SLPI
				GPR[r2^0x01] = GPR[r2] << r3;
				break;
			case 0xAF:
				// SLPI16
				GPR[r2^0x01] = GPR[r2] << (r3+16);
				break;
			case 0xB0:
				// SAR
				GPR[r2] = (uint32_t)((int32_t)GPR[r2] >> (GPR[r3] & 0x0000003F));
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
				
				break;
			case 0xB6:
				// D
				
				break;
			case 0xB8:
				// SR
				GPR[r2] = GPR[r2] >> (GPR[r3] & 0x0000003F);
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
				// Unexpected Instruction TRAP

				break;
		}
	}
}