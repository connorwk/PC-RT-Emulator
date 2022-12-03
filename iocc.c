// IO Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "iocc.h"
#include "mmu.h"
#include "kb_adapter.h"
#include "8237.h"
#include "8259a.h"
#include "rtc.h"
#include "mda.h"
#include "logfac.h"

struct SysBrdConfig sysbrdcnfg;
struct structkbadpt kbAdapter;
struct struct8237 dmaCtrl1;
struct struct8237 dmaCtrl2;
struct struct8259 intCtrl1;
struct struct8259 intCtrl2;
struct structrtc sysRTC;
struct structmda mdaVideo;

struct procBusStruct* procBusPtr;
struct ioBusStruct ioBus;

uint8_t CSRlocked;

void ioinit (struct procBusStruct* procBusPointer) {
	procBusPtr = procBusPointer;
	//sysbrdcnfg.CSR = 0x220000FF;
	initkbadpt(&kbAdapter, &ioBus, 0x008400, 0xFFFFF8);
	initRTC(&sysRTC, &ioBus, 0x008800, 0xFFFFC0);
	init8237(&dmaCtrl1, &ioBus, 0x008840, 0xFFFFF0);
	init8237(&dmaCtrl2, &ioBus, 0x008860, 0xFFFFF0);
	init8259(&intCtrl1, &ioBus, 0x008880, 0xFFFFE0);
	init8259(&intCtrl2, &ioBus, 0x0088A0, 0xFFFFE0);
	initMDA(&mdaVideo, &ioBus, 0x0003B0, 0xFFFFF0, 0x0B0000, 0xFFF000);
}

uint8_t* getMDAPtr (void) {
	return &mdaVideo.videoMem[0];
}

void iocycle (void) {
	if (sysbrdcnfg.DIAReg) {
		intCtrl1.intLines = 0xFF;
		intCtrl2.intLines = 0xFF;
	} else {
		intCtrl1.intLines = 0;
		intCtrl2.intLines = 0;
	}

	dmaCtrl1.reset = (sysbrdcnfg.CRRBReg & CRRB_DMACtrl1) >> 3;
	dmaCtrl2.reset = (sysbrdcnfg.CRRBReg & CRRB_DMACtrl2) >> 4;
	kbAdapter.reset = (sysbrdcnfg.CRRBReg & CRRB_8051) >> 2;
	cyclekbadpt(&kbAdapter);
	intCtrl1.intLines |= (kbAdapter.intReq << 5);
	cycleRTC(&sysRTC);
	if (sysRTC.intReq) {
		procBusPtr->intrpt = INTRPT_1_RealTimeClock;
	}
	kbAdapter.PB = (sysRTC.sqwOut << 3);
	cycle8237(&dmaCtrl1);
	cycle8237(&dmaCtrl2);
	cycle8259(&intCtrl1);
	cycle8259(&intCtrl2);

	if (intCtrl1.intreq) { procBusPtr->intrpt |= INTRPT_3_IOChan; }
	if (intCtrl2.intreq) { procBusPtr->intrpt |= INTRPT_4_IOChan; }
}

void accessSysBrdRegs (void) {
	if (ioBus.io == 1) {
		ioBus.cs16 = 0; // Assuned 8-Bit, overwritten for TCWs
		if ((ioBus.addr & 0xFFFFE0) == 0x0088C0) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write DMA DBR Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.DMARegDBR = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.DMARegDBR;
				logmsgf(LOGIO, "IO: Read DMA DBR Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFE0) == 0x0088E0) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write DMA DMR Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.DMARegDMR = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.DMARegDMR;
				logmsgf(LOGIO, "IO: Read DMA DMR Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008C00) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write Ch8 Enable Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.CH8EnReg = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.CH8EnReg;
				logmsgf(LOGIO, "IO: Read Ch8 Enable Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008C20) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write Control CCR Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.CtrlRegCCR = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.CtrlRegCCR;
				logmsgf(LOGIO, "IO: Read Control CCR Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008C40) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write CRRA Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.CRRAReg = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.CRRAReg;
				logmsgf(LOGIO, "IO: Read CRRA Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008C60) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write CRRB Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.CRRBReg = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.CRRBReg;
				logmsgf(LOGIO, "IO: Read CRRB Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008C80) {
			if (!ioBus.rw) {
				ioBus.data = sysbrdcnfg.MemCnfgReg;
				logmsgf(LOGIO, "IO: Read Mem Cnfg Reg 0x%04X\n", ioBus.data);
			}
		}
		if ((ioBus.addr & 0xFFFFFC) == 0x008CA0) {
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write DIAG Reg 0x%04X\n", ioBus.data);
				sysbrdcnfg.DIAReg = ioBus.data & 0x01;
			}
		}
		if ((ioBus.addr & 0xFFF801) == 0x010000) {
			if (!ioBus.sbhe) {logmsgf(LOGIO, "IO: Warning TCW access should be a halfword access.\n");};
			ioBus.cs16 = 1;
			if (ioBus.rw) {
				logmsgf(LOGIO, "IO: Write TCW %d 0x%04X\n", ((ioBus.addr & 0x0007FE) >> 1), ioBus.data);
				sysbrdcnfg.TCW[((ioBus.addr & 0x0007FE) >> 1)] = ioBus.data;
			} else {
				ioBus.data = sysbrdcnfg.TCW[((ioBus.addr & 0x0007FE) >> 1)] ;
				logmsgf(LOGIO, "IO: Read TCW %d 0x%04X\n", ((ioBus.addr & 0x0007FE) >> 1), ioBus.data);
			}
		}
	}
}

void ioaccessAll (void) {
	accessSysBrdRegs();
	accesskbadpt(&kbAdapter);
	accessRTC(&sysRTC);
	access8237(&dmaCtrl1);
	access8237(&dmaCtrl2);
	access8259(&intCtrl1);
	access8259(&intCtrl2);
	accessMDA(&mdaVideo);
}

void ioaccess (void) {
	// Here to clean up logs... Probably not needed when done other than to simulate a delay.
	if (procBusPtr->addr == 0xF00080E0) {logmsgf(LOGIO, "IO: Delay Reg...\n"); return;}

	// CSR sits on processor bus
	if (procBusPtr->addr == 0xF0010800) {
		if (procBusPtr->rw) {
			logmsgf(LOGIO, "IO: Write CSR (clears)\n");
			//sysbrdcnfg.CSR = 0x220000FF;
			sysbrdcnfg.CSR = 0;
		} else {
			logmsgf(LOGIO, "IO: Read CSR 0x%08X\n", sysbrdcnfg.CSR);
			procBusPtr->data = sysbrdcnfg.CSR;
		}
		return;
	}

	if ((procBusPtr->addr & 0xFF000000) == IOChanIOMapStartAddr) {
		ioBus.io = 1;
		if (procBusPtr->addr > 0xF0010800) {
			procBusPtr->flags = FLAGS_Trap;
		}
	} else if ((procBusPtr->addr & 0xFF000000) == IOChanMemMapStartAddr) {
		ioBus.io = 0;
	}
	ioBus.addr = procBusPtr->addr & 0x00FFFFFF;
	ioBus.rw = procBusPtr->rw;
	if (!ioBus.rw) {ioBus.data = 0;}
	switch(procBusPtr->width) {
		case WIDTH_BYTE:
			ioBus.sbhe = SBHE_1byte;
			//if (!ioBus.io) {ioBus.addr = ioBus.addr ^ 0x000001;}
			if (ioBus.rw == RW_STORE) {
				ioBus.data = procBusPtr->data & 0x000000FF;
				logmsgf(LOGIO, "IO: Byte write      0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
				ioaccessAll();
			} else {
				ioaccessAll();
				procBusPtr->data = ioBus.data & 0x00FF;
				logmsgf(LOGIO, "IO: Byte read      0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
			}
			break;
		case WIDTH_HALFWORD:
			ioBus.sbhe = SBHE_2bytes;
			ioBus.addr &= 0xFFFFFE;
			if (ioBus.rw == RW_STORE) {
				//ioBus.data = ((procBusPtr->data & 0x000000FF) << 8) | ((procBusPtr->data & 0x0000FF00) >> 8);
				ioBus.data = procBusPtr->data;
				logmsgf(LOGIO, "IO: Halfword write  0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
				ioaccessAll();
				if (!ioBus.cs16) {
					//ioBus.data = (procBusPtr->data & 0x000000FF);
					if (!ioBus.io) {ioBus.data = ((procBusPtr->data & 0x0000FF00) >> 8);}
					//if (!ioBus.io) {ioBus.addr++;}
					ioBus.addr++;
					ioBus.sbhe = SBHE_1byte;
					logmsgf(LOGIO, "IO:      Byte write 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					ioaccessAll();
				}
			} else {
				ioaccessAll();
				logmsgf(LOGIO, "IO: Halfword read  0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
				if (!ioBus.cs16) {
					//procBusPtr->data = (ioBus.data & 0x00FF) << 8;
					procBusPtr->data = ioBus.data;
					ioBus.sbhe = SBHE_1byte;
					//if (!ioBus.io) {ioBus.addr++;}
					ioBus.addr++;
					ioaccessAll();
					logmsgf(LOGIO, "IO:      Byte read 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					//procBusPtr->data |= ioBus.data & 0x00FF;
					procBusPtr->data |= (ioBus.data & 0x00FF) << 8;
				} else {
					//procBusPtr->data = ((ioBus.data & 0x00FF) << 8) | ((ioBus.data & 0xFF00) >> 8);
					procBusPtr->data = ioBus.data;
				}
			}
			break;
		case WIDTH_WORD:
			ioBus.sbhe = SBHE_2bytes;
			ioBus.addr &= 0xFFFFFC;
			if (ioBus.rw == RW_STORE) {
				//ioBus.data = ((procBusPtr->data & 0x00FF0000) >> 8) | ((procBusPtr->data & 0xFF000000) >> 24);
				ioBus.data = procBusPtr->data;
				if (!ioBus.io) {ioBus.data = ((procBusPtr->data & 0xFFFF0000) >> 16);}
				logmsgf(LOGIO, "IO: Word write      0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
				ioaccessAll();
				if (ioBus.cs16) {
					//if (!ioBus.io) {ioBus.addr += 2;}
					ioBus.addr += 2;
					//ioBus.data = ((procBusPtr->data & 0x000000FF) << 8) | ((procBusPtr->data & 0x0000FF00) >> 8);
					if (!ioBus.io) {ioBus.data = procBusPtr->data;}
					ioBus.sbhe = SBHE_2bytes;
					logmsgf(LOGIO, "IO:  Halfword write 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					ioaccessAll();
				} else {
					//if (!ioBus.io) {ioBus.addr++;}
					ioBus.addr++;
					//ioBus.data = (procBusPtr->data & 0x00FF0000) >> 16;
					if (!ioBus.io) {ioBus.data = ((procBusPtr->data & 0xFF000000) >> 24);}
					ioBus.sbhe = SBHE_1byte;
					logmsgf(LOGIO, "IO:     Byte write 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					ioaccessAll();
					//if (!ioBus.io) {ioBus.addr++;}
					ioBus.addr++;
					//ioBus.data = (procBusPtr->data & 0x0000FF00) >> 8;
					if (!ioBus.io) {ioBus.data = (procBusPtr->data & 0x000000FF);}
					ioBus.sbhe = SBHE_1byte;
					logmsgf(LOGIO, "IO:     Byte write 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					ioaccessAll();
					//if (!ioBus.io) {ioBus.addr++;}
					ioBus.addr++;
					//ioBus.data = procBusPtr->data & 0x000000FF;
					if (!ioBus.io) {ioBus.data = (procBusPtr->data & 0x0000FF00) >> 8;}
					ioBus.sbhe = SBHE_1byte;
					logmsgf(LOGIO, "IO:     Byte write 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					ioaccessAll();
				}
			} else {
				ioaccessAll();
				logmsgf(LOGIO, "IO: Word read      0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
				if (ioBus.cs16) {
					//procBusPtr->data = ((ioBus.data & 0x00FF) << 24) | ((ioBus.data & 0xFF00) << 8);
					procBusPtr->data = (ioBus.data << 16);
					//if (!ioBus.io) {ioBus.addr += 2;}
					ioBus.addr += 2;
					ioaccessAll();
					logmsgf(LOGIO, "IO:  Halfword read 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					//procBusPtr->data |= ((ioBus.data & 0x00FF) << 8) | ((ioBus.data & 0xFF00) >> 8);
					procBusPtr->data |= ioBus.data;
				} else {
					//procBusPtr->data = (ioBus.data & 0x00FF) << 24;
					procBusPtr->data = (ioBus.data & 0x00FF) << 16;
					ioBus.sbhe = SBHE_1byte;
					if (!ioBus.io) {ioBus.addr++;}
					ioaccessAll();
					logmsgf(LOGIO, "IO:     Byte read 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					//procBusPtr->data |= (ioBus.data & 0x00FF) << 16;
					procBusPtr->data |= (ioBus.data & 0x00FF) << 24;
					if (!ioBus.io) {ioBus.addr++;}
					ioaccessAll();
					logmsgf(LOGIO, "IO:     Byte read 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					//procBusPtr->data |= (ioBus.data & 0x00FF) << 8;
					procBusPtr->data |= (ioBus.data & 0x00FF);
					if (!ioBus.io) {ioBus.addr++;}
					ioaccessAll();
					logmsgf(LOGIO, "IO:     Byte read 0x%06X : 0x%04X\n", ioBus.addr, ioBus.data);
					//procBusPtr->data |= (ioBus.data & 0x00FF);
					procBusPtr->data |= (ioBus.data & 0x00FF) << 8;
				}
			}
			break;
	}
}