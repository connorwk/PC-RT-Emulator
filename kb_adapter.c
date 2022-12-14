// Keyboard Adapter Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "defs.h"
#include "kb_adapter.h"
#include "iocc.h"
#include "mmu.h"
#include "logfac.h"

struct ioBusStruct* ioBusPtr;

void initSharedRam (struct structkbadpt* currkbadpt) {
	currkbadpt->sharedRam[0x00] = 0xFA; // Keyboard ack byte
	currkbadpt->sharedRam[0x01] = 0x00; // Pending speaker durration ticks
	currkbadpt->sharedRam[0x02] = 0x00; //
	currkbadpt->sharedRam[0x03] = 0xFE; // Keyboard resend command
	currkbadpt->sharedRam[0x04] = 0xF0; // Keyboard break code
	currkbadpt->sharedRam[0x05] = 0x08; // Max retry before hard keyboard error
	currkbadpt->sharedRam[0x06] = 0xEE; // Keyboard echo command
	currkbadpt->sharedRam[0x07] = 0x36; // Click durration
	currkbadpt->sharedRam[0x08] = 0x12; // Click suppress scan code 1
	currkbadpt->sharedRam[0x09] = 0x59; // Click suppress scan code 2
	currkbadpt->sharedRam[0x0A] = 0x39; // Click suppress scan code 3
	currkbadpt->sharedRam[0x0B] = 0x08; // Click freqency high/low
	currkbadpt->sharedRam[0x0C] = 0x96; // 
	currkbadpt->sharedRam[0x0D] = 0x11; // Keystroke sys atten scan 1, click suppress scan 4
	currkbadpt->sharedRam[0x0E] = 0x19; // Keystroke sys atten scan 2, click suppress scan 5
	currkbadpt->sharedRam[0x0F] = 0xFF; // Keystroke sys atten scan 3, (default = any scan match)
	currkbadpt->sharedRam[0x10] = 0x68; // See MODE0 defines
	currkbadpt->sharedRam[0x11] = 0x2A; // See MODE1 defines
	currkbadpt->sharedRam[0x12] = 0x00; // See STATUS defines
	currkbadpt->sharedRam[0x13] = 0x00; // Active speaker ticks remaining
	currkbadpt->sharedRam[0x14] = 0x00; // 
	currkbadpt->sharedRam[0x15] = 0x00; // Pending speaker freq high/low
	currkbadpt->sharedRam[0x16] = 0x00; // 
	currkbadpt->sharedRam[0x17] = 0x03; // Scan count for sys atten keystroke (max 3)
	currkbadpt->sharedRam[0x18] = 0x00; // Keystroke seq state (high 4 bits: sys atten, low 4 bits: sys reset)
	currkbadpt->sharedRam[0x19] = 0x84; // UART Framing (MSB odd(1)/even(0), low 3 bits: blocking factor (2-6 valid))
	currkbadpt->sharedRam[0x1A] = 0x00; // Sys atten scan code (actual third byte received in 3-byte seq)
	currkbadpt->sharedRam[0x1B] = 0xFB; // UART Baud rate (default: 9600)
	currkbadpt->sharedRam[0x1C] = 0xAE; // Actual 8051 self-test completion code
	currkbadpt->sharedRam[0x1D] = 0x00; // 8051 Abnormal end info
	currkbadpt->sharedRam[0x1E] = 0x00; // 
	currkbadpt->sharedRam[0x1F] = 0x00; // Error code for most recent Int ID 7
}

void initkbadpt (struct structkbadpt* currkbadpt, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask) {
	currkbadpt->ioAddress = ioaddr;
	currkbadpt->ioAddressMask = ioaddrMask;
	currkbadpt->irqEn = 0;
	currkbadpt->intReq = 0;
	currkbadpt->initReq = RESET_Delay;
	ioBusPtr = ioBusPointer;
}

void writekbadpt (struct structkbadpt* currkbadpt) {
	ioBusPtr->cs16 = 0; // Overridden by write to PA Output Buf 0x8400
	uint8_t data = (ioBusPtr->data & 0x00FF);
	switch (ioBusPtr->addr & 0x000007) {
		case 0x0:
			if (currkbadpt->PC & PC_PAOutBufEmpty) {
				if (!ioBusPtr->sbhe) {logmsgf(LOGKBADPT, "KBADPT: Warning write PA should be a word access.\n");};
				//if (ioBusPtr->data & 0xC000) {logmsgf(LOGKBADPT, "KBADPT: Warning write PA bits 15-14 should be 0.\n");};
				if (ioBusPtr->data & 0x00C0) {logmsgf(LOGKBADPT, "KBADPT: Warning write PA bits 15-14 should be 0.\n");};
				if (!(currkbadpt->PC & PC_PAOutBufEmpty)) {logmsgf(LOGKBADPT, "KBADPT: Error write to PA when PA Out Buffer not empty.\n");}
				//currkbadpt->cmdReg = (ioBusPtr->data & 0x3F00) >> 8;
				//currkbadpt->PA = data;
				currkbadpt->cmdReg = data & 0x3F;
				currkbadpt->PA = (ioBusPtr->data & 0xFF00) >> 8;
				currkbadpt->PC &= ~PC_PAOutBufEmpty;
				logmsgf(LOGKBADPT, "KBADPT: Write PA 0x%04X\n", ioBusPtr->data);
			} else {
				logmsgf(LOGKBADPT, "KBADPT: Error write PA attempted when PA Out Buf is full.\n");
			}
			ioBusPtr->cs16 = 1;
			break;
		case 0x7:
			if (data == 0xC3) {
				logmsgf(LOGKBADPT, "KBADPT: Config 8255 byte written.\n");
			} else if (data == 0x08) {
				logmsgf(LOGKBADPT, "KBADPT: Disable IRQ.\n");
				currkbadpt->irqEn = 0;
			} else if (data == 0x09) {
				logmsgf(LOGKBADPT, "KBADPT: Enable IRQ.\n");
				currkbadpt->irqEn = 1;
			}
			break;
	}
}

void readkbadpt (struct structkbadpt* currkbadpt) {
	switch (ioBusPtr->addr & 0x000007) {
		case 0x4:
			//if (!(currkbadpt->PC & PC_IntReq)) {logmsgf(LOGKBADPT, "KBADPT: Error Read from PA when data not valid.\n");}
			if (!(currkbadpt->PC & PC_PAInBufFull)) {logmsgf(LOGKBADPT, "KBADPT: Error Read from PA when data not valid.\n");}
			ioBusPtr->data = currkbadpt->PA;
			currkbadpt->PC &= ~(PC_IntReq | PC_PAInBufFull);
			logmsgf(LOGKBADPT, "KBADPT: Read from PA 0x%02X\n", ioBusPtr->data);
			break;
		case 0x5:
			ioBusPtr->data = currkbadpt->PB;
			// bit 3 may be squarewave output from RTC
			//ioBusPtr->data = ((currkbadpt->sharedRam[0x11] & MODE1_SpkVol) << 5);
			logmsgf(LOGKBADPT, "KBADPT: Read from PB 0x%02X\n", ioBusPtr->data);
			break;
		case 0x6:
			ioBusPtr->data = currkbadpt->PC;
			logmsgf(LOGKBADPT, "KBADPT: Read from PC 0x%02X\n", ioBusPtr->data);
			break;
	}
}

void accesskbadpt (struct structkbadpt* currkbadpt) {
	// Technically an interface to the 8255 hooked to the 8051 keyboard adapter.
	// So we dont prevent reads/writes from the 8255 when the 8051 is held in reset.
	// 8255 is not hooked to the reset line for the "keyboard adapter" just the 8051.
	if (ioBusPtr->io == 1) {
		if ((ioBusPtr->addr & currkbadpt->ioAddressMask) == currkbadpt->ioAddress) {
			if (ioBusPtr->rw) {
				writekbadpt(currkbadpt);
			} else {
				readkbadpt(currkbadpt);
				ioBusPtr->cs16 = 0;
			}
		}
	}
}

void setReturnVals (struct structkbadpt* currkbadpt, uint8_t valPA, uint8_t intID) {
	currkbadpt->PA = valPA;
	if (currkbadpt->irqEn) {
		currkbadpt->PC = (currkbadpt->PC & ~PC_IntLevel) | PC_PAInBufFull | PC_IntReq | intID;
	} else {
		currkbadpt->PC = (currkbadpt->PC & ~PC_IntLevel) | PC_PAInBufFull | intID;
	}
}

int circBufPush (struct circ_buf* buf, uint8_t data) {
	uint8_t next;
	next = buf->head + 1;
	if (next >= BUFMAX) {next = 0;}
	if (next == buf->tail) {return -1;}

	buf->buffer[buf->head] = data;
	buf->head = next;
	return 0;
}

int circBufPop(struct circ_buf* buf, uint8_t *data)
{
	uint8_t next;
	if (buf->head == buf->tail) {return -1;}
	next = buf->tail + 1;
	if(next >= BUFMAX) {next = 0;}

	*data = buf->buffer[buf->tail];
	buf->tail = next;
	return 0;
}

uint8_t circBufGetLen(struct circ_buf* buf) {
	uint8_t next;
	uint8_t length;
	if (buf->head == buf->tail) {return 0;}
	next = buf->tail + 1;
	if(next >= BUFMAX) {next = 0;}
	length++;
	while (next != buf->head) {
		next++;
		if(next >= BUFMAX) {next = 0;}
		length++;
	}
	return length;
}

void cyclekbadpt (struct structkbadpt* currkbadpt) {
	// This cycle acts like the 8051 running.
	if (!currkbadpt->reset) {
		currkbadpt->irqEn = 0;
		currkbadpt->intReq = 0;
		currkbadpt->initReq = RESET_Delay;
		return;
	}
	
	if (currkbadpt->initReq > 1 && currkbadpt->initReq <= RESET_Delay) {
		// Delay some to simulate 8051 initializing, may not be needed...
		currkbadpt->initReq--;
		return;
	} else if (currkbadpt->initReq == 1) {
		logmsgf(LOGKBADPT, "KBADPT: Initialized after reset released\n");
		initSharedRam(currkbadpt);
		setReturnVals(currkbadpt, 0xAE, INTID_8051SelfTest);
		currkbadpt->initReq--;
		return;
	}

	//if (currkbadpt->irqEn && (currkbadpt->PC & PC_IntReq)) {
		if (currkbadpt->PC & PC_IntReq) {
		currkbadpt->intReq = 1;
	} else {
		currkbadpt->intReq = 0;
	}

	if (currkbadpt->PC & PC_PAInBufFull) {
		// Wait for host to process what we have in the PA buff already.
		return;
	}

	// TODO: Process other keyboard commands in seperate function for readability probably...
	// Keyboard Command Processing pg. 9-5
	if (currkbadpt->kbCmdIn == 0xFF) {
		// Keyboard reset
		uint8_t ret = 0;
		ret += circBufPush(&currkbadpt->kbBuf, 0xAA);
		ret += circBufPush(&currkbadpt->kbBuf, 0xBF);
		ret += circBufPush(&currkbadpt->kbBuf, 0xB0); // Last nibble is keyboard type, what's valid here? pg. 5-143
		if (ret) {logmsgf(LOGKBADPT, "KBADPT: Error KB Buf full.\n");}
		currkbadpt->kbCmdIn = 0;
	}
	
	// Keyboard buffer not empty send byte
	if (currkbadpt->kbBuf.head != currkbadpt->kbBuf.tail) {
		uint8_t byte;
		circBufPop(&currkbadpt->kbBuf, &byte);
		setReturnVals(currkbadpt, byte, INTID_KBByteRX);
	}

	// TODO: Process UART (locator) commands in seperate function for readability probably...
	/*
	if (currkbadpt->uartCmdIn == 0x09) {
		// Locator disable stream mode
		uint8_t ret = 0;
		// No idea what they want us to return here! TODO
		// Returning reset values for now...
		ret += circBufPush(&currkbadpt->uartBuf, 0xFF);
		//ret += circBufPush(&currkbadpt->uartBuf, 0x80);
		//ret += circBufPush(&currkbadpt->uartBuf, 0x00);
		//ret += circBufPush(&currkbadpt->uartBuf, 0x00);
		if (ret) {logmsgf(LOGKBADPT, "KBADPT: Error UART Buf full.\n");}
		currkbadpt->uartCmdIn = 0;
	}
	*/

	// UART buffer not empty
	if (currkbadpt->sharedRam[0x10] & MODE0_BlockReceivedUARTBytes) {
		if (currkbadpt->reportLen) {
			uint8_t byte;
			circBufPop(&currkbadpt->uartBuf, &byte);
			setReturnVals(currkbadpt, byte, INTID_UARTByteRX);
			currkbadpt->reportLen--;
		} else if (circBufGetLen(&currkbadpt->uartBuf) >= (currkbadpt->sharedRam[0x19] & 0x0F)) {
			currkbadpt->reportLen = currkbadpt->sharedRam[0x19] & 0x0F;
		}
	} else {
		if (currkbadpt->uartBuf.head != currkbadpt->uartBuf.tail) {
			uint8_t byte;
			circBufPop(&currkbadpt->uartBuf, &byte);
			setReturnVals(currkbadpt, byte, INTID_UARTByteRX);
		}
	}

	// TODO: ramQueue processing

	if (currkbadpt->initReq > RESET_Delay) {
		// Soft reset pg. 5-107
		// Requires two additional bytes to be sent...
		setReturnVals(currkbadpt, 0x00, INTID_8051Error);
		currkbadpt->initReq--;
	}

	if (!(currkbadpt->PC & PC_PAOutBufEmpty)) {
		// We have a command to process
		currkbadpt->PC |= PC_PAOutBufEmpty;
		currkbadpt->PC &= ~PC_IntReq;
		currkbadpt->intReq = 0;
		if (currkbadpt->cmdReg & 0xE0) {logmsgf(LOGKBADPT, "KBADPT: Diag CMD bits not zero CMD:0x%02X PA:0x%02X\n", currkbadpt->cmdReg, currkbadpt->PA);}
		if (currkbadpt->cmdReg == 0) {
			// Extended commands pg. 5-99
			if (currkbadpt->PA >= 0x00 && currkbadpt->PA <= 0x1F) {
				// Read shared ram
				uint8_t offset = currkbadpt->PA;
				setReturnVals(currkbadpt, currkbadpt->sharedRam[offset], INTID_RetReqByte);
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD read ram 0x%02X: 0x%02X\n", offset, currkbadpt->PA);
			} else if (currkbadpt->PA >= 0x20 && currkbadpt->PA <= 0x2F) {
				// Reset mode bit
				if (!(currkbadpt->PA & 0x08)) {
					logmsgf(LOGKBADPT, "KBADPT: ExtCMD reset mode bit %d 0x%02X\n", currkbadpt->PA & 0x0F, currkbadpt->sharedRam[0x10]);
					currkbadpt->sharedRam[0x10] &= ~(0x01 << (currkbadpt->PA & 0x07));
					logmsgf(LOGKBADPT, "KBADPT:                          0x%02X\n", currkbadpt->sharedRam[0x10]);
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					if (currkbadpt->PA == 11) {
						if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
							logmsgf(LOGKBADPT, "KBADPT: ExtCMD reset mode bit %d 0x%02X\n", currkbadpt->PA & 0x0F, currkbadpt->sharedRam[0x11]);
							currkbadpt->sharedRam[0x11] &= ~(0x01 << (currkbadpt->PA & 0x07));
							logmsgf(LOGKBADPT, "KBADPT:                          0x%02X\n", currkbadpt->sharedRam[0x11]);
							setReturnVals(currkbadpt, 0x00, INTID_Info);
						} else {
							logmsgf(LOGKBADPT, "KBADPT: Error can't disable keyboard when not in Diag mode\n");
							setReturnVals(currkbadpt, 0x51, INTID_Info);
						}
					} else {
						currkbadpt->sharedRam[0x11] &= ~(0x01 << (currkbadpt->PA & 0x07));
						setReturnVals(currkbadpt, 0x00, INTID_Info);
					}
				}
			} else if (currkbadpt->PA >= 0x30 && currkbadpt->PA <= 0x3F) {
				// Set mode bit
				if (!(currkbadpt->PA & 0x08)) {
					logmsgf(LOGKBADPT, "KBADPT: ExtCMD set mode bit %d 0x%02X\n", currkbadpt->PA & 0x0F, currkbadpt->sharedRam[0x10]);
					currkbadpt->sharedRam[0x10] |= 0x01 << (currkbadpt->PA & 0x07);
					logmsgf(LOGKBADPT, "KBADPT:                        0x%02X\n", currkbadpt->sharedRam[0x10]);
				} else {
					logmsgf(LOGKBADPT, "KBADPT: ExtCMD set mode bit %d 0x%02X\n", currkbadpt->PA & 0x0F, currkbadpt->sharedRam[0x11]);
					currkbadpt->sharedRam[0x11] |= 0x01 << (currkbadpt->PA & 0x07);
					logmsgf(LOGKBADPT, "KBADPT:                        0x%02X\n", currkbadpt->sharedRam[0x11]);
				}
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			} else if (currkbadpt->PA >= 0x40 && currkbadpt->PA <= 0x43) {
				currkbadpt->sharedRam[0x11] = (currkbadpt->sharedRam[0x11] & ~MODE1_SpkVol) | currkbadpt->PA & MODE1_SpkVol;
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			} else if (currkbadpt->PA == 0x44) {
				// Terminate speaker and reset duration
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD terminate speaker\n");
				if (currkbadpt->sharedRam[0x12] & (STATUS_SpkTimerBusy | STATUS_TimeoutTimerBsy)) {
					currkbadpt->sharedRam[0x12] &= ~(STATUS_SpkQueueFull | STATUS_SpkTimerBusy | STATUS_TimeoutTimerBsy);
					currkbadpt->sharedRam[0x13] = 0x00;
					currkbadpt->sharedRam[0x14] = 0x00;
					setReturnVals(currkbadpt, 0x03, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x02, INTID_Info);
				}
			} else if (currkbadpt->PA >= 0x50 && currkbadpt->PA <= 0x5F) {
				// Set scan count for sys attn keystroke sequence
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD set scan count %d\n", currkbadpt->PA & 0x0F);
				if (currkbadpt->PA >= 0x51 && currkbadpt->PA <= 0x53) {
					currkbadpt->sharedRam[0x17] = currkbadpt->PA & 0x0F;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x50, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x60) {
				// Execute 8051 soft reset
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD 8051 soft reset\n");
				if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
					currkbadpt->initReq = SOFTRESET_Delay;
					setReturnVals(currkbadpt, 0xA0, INTID_8051Error);
				} else {
					setReturnVals(currkbadpt, 0x51, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x61) {
				// Force system reset
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD force system reset\n");
				if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
					// TODO: Reset system...
				} else {
					setReturnVals(currkbadpt, 0x51, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x62) {
				// Force system attn interrupt
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD force system attention interrupt\n");
				if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
					// TODO: System atten interrupt
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x51, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x62) {
				// Diagnostic sense keyboard/UART port pins
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD diagnostic sense keyboard/UART\n");
				if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
					// Probably don't have to return anything sensible here.
					setReturnVals(currkbadpt, 0x00, 0x3);
				} else {
					setReturnVals(currkbadpt, 0x51, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x80) {
				// Dump adapter shared 0x00-0x0F
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD dump adapter shared 0x00-0x0F\n");
				if (!currkbadpt->ramQueue) {
					currkbadpt->ramOffset = 0x00;
					currkbadpt->ramQueue = 0x0F;
					currkbadpt->ramClear = 0;
					//setReturnVals(currkbadpt, 0x00, INTID_Info);
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x81) {
				// Dump adapter shared 0x10-0x1F
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD dump adapter shared 0x10-0x1F\n");
				if (!currkbadpt->ramQueue) {
					currkbadpt->ramOffset = 0x10;
					currkbadpt->ramQueue = 0x0F;
					currkbadpt->ramClear = 0;
					//setReturnVals(currkbadpt, 0x00, INTID_Info);
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x82) {
				// Dump RAS logs 0x20-0x2B
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD dump RAS logs 0x20-0x2B\n");
				if (!currkbadpt->ramQueue) {
					currkbadpt->ramOffset = 0x20;
					currkbadpt->ramQueue = 0x0B;
					currkbadpt->ramClear = 0;
					//setReturnVals(currkbadpt, 0x00, INTID_Info);
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x83) {
				// Dump RAS logs 0x20-0x2B with clear
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD dump RAS logs with clear 0x20-0x2B\n");
				if (!currkbadpt->ramQueue) {
					currkbadpt->ramOffset = 0x20;
					currkbadpt->ramQueue = 0x0B;
					currkbadpt->ramClear = 1;
					//setReturnVals(currkbadpt, 0x00, INTID_Info);
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x60, INTID_Info);
				}
			} else if (currkbadpt->PA == 0x83) {
				// Restore initial conditions
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD restore initial conditions\n");
				initSharedRam(currkbadpt); // Really should only init to 0x1B, but this should be OK
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			} else if (currkbadpt->PA >= 0xE0 && currkbadpt->PA <= 0xEF) {
				// Read 8051 release marker
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD read 8051 release marker\n");
				setReturnVals(currkbadpt, 0x00, 0x3); // Should return valid data?
			} else if (currkbadpt->PA >= 0xF0 && currkbadpt->PA <= 0xFF) {
				// NOP
				logmsgf(LOGKBADPT, "KBADPT: ExtCMD NOP\n");
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			}
		} else {
			// Non-extended Commands pg. 5-102
			if ((currkbadpt->cmdReg & 0x1F) == 0x01) {
				// Write to keyboard
				logmsgf(LOGKBADPT, "KBADPT: CMD write to keyboard 0x%02X\n", currkbadpt->PA);
				if (currkbadpt->keylock && (currkbadpt->sharedRam[0x11] & MODE1_HonorKeylock)) {
					setReturnVals(currkbadpt, 0x42, INTID_Info);
				} else if (!(currkbadpt->sharedRam[0x11] & MODE1_KBInterfaceEn)) {
					setReturnVals(currkbadpt, 0x43, INTID_Info);
				} else if (currkbadpt->PA == currkbadpt->sharedRam[0x03]) {
					setReturnVals(currkbadpt, 0x44, INTID_Info);
				} else {
					// TODO
					currkbadpt->kbCmdIn = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x02) {
				// Write to speaker
				logmsgf(LOGKBADPT, "KBADPT: CMD write to speaker 0x%02X\n", currkbadpt->PA);
				// TODO
				setReturnVals(currkbadpt, 0x01, INTID_Info);
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x03) {
				// Write to UART - control
				logmsgf(LOGKBADPT, "KBADPT: CMD write to UART control 0x%02X\n", currkbadpt->PA);
				if (currkbadpt->keylock && (currkbadpt->sharedRam[0x11] & MODE1_HonorKeylock)) {
					setReturnVals(currkbadpt, 0x42, INTID_Info);
				} else if (!(currkbadpt->sharedRam[0x11] & MODE1_UARTInterfaceEn)) {
					setReturnVals(currkbadpt, 0x4B, INTID_Info);
				} else {
					currkbadpt->uartCmdIn = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x04) {
				// Write to UART - query
				logmsgf(LOGKBADPT, "KBADPT: CMD write to UART query 0x%02X\n", currkbadpt->PA);
				if (currkbadpt->keylock && (currkbadpt->sharedRam[0x11] & MODE1_HonorKeylock)) {
					setReturnVals(currkbadpt, 0x42, INTID_Info);
				} else if (!(currkbadpt->sharedRam[0x11] & MODE1_UARTInterfaceEn)) {
					setReturnVals(currkbadpt, 0x4B, INTID_Info);
				} else {
					// TODO: Implement locator
					//setReturnVals(currkbadpt, 0x00, INTID_Info);
					if (currkbadpt->cmdReg & 0x20) {
						// If bit 5 of the CMD byte is set we do a loopback on the UART
						circBufPush(&currkbadpt->uartBuf, currkbadpt->PA);
					} else {
						currkbadpt->uartCmdIn = currkbadpt->PA;
					}
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x05) {
				// Set UART baud rate
				logmsgf(LOGKBADPT, "KBADPT: CMD set UART baud rate 0x%02X\n", currkbadpt->PA);
				currkbadpt->sharedRam[0x1B] = currkbadpt->PA;
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x06) {
				// Init UART framing
				logmsgf(LOGKBADPT, "KBADPT: CMD init UART framing 0x%02X\n", currkbadpt->PA);
				if (!(currkbadpt->PA & 0x78) && (currkbadpt->PA & 0x07) > 1 && (currkbadpt->PA & 0x07) < 7) {
					currkbadpt->sharedRam[0x19] = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x4E, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x07) {
				// Set speaker duration
				logmsgf(LOGKBADPT, "KBADPT: CMD set speaker duration 0x%02X\n", currkbadpt->PA);
				if (!(currkbadpt->sharedRam[0x12] & STATUS_SpkQueueFull)) {
					currkbadpt->sharedRam[0x01] = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x4A, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x08) {
				// Set speaker freq high
				logmsgf(LOGKBADPT, "KBADPT: CMD set speaker freq high 0x%02X\n", currkbadpt->PA);
				if (!(currkbadpt->sharedRam[0x12] & STATUS_SpkQueueFull)) {
					currkbadpt->sharedRam[0x15] = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x4A, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x09) {
				// Set speaker freq low
				logmsgf(LOGKBADPT, "KBADPT: CMD set speaker freq low 0x%02X\n", currkbadpt->PA);
				if (!(currkbadpt->sharedRam[0x12] & STATUS_SpkQueueFull)) {
					currkbadpt->sharedRam[0x16] = currkbadpt->PA;
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x4A, INTID_Info);
				}
			} else if ((currkbadpt->cmdReg & 0x1F) == 0x0C) {
				// Diagnostic write keyboard port pins
				logmsgf(LOGKBADPT, "KBADPT: CMD diagnostic write keyboard port 0x%02X\n", currkbadpt->PA);
				if (currkbadpt->sharedRam[0x11] & MODE1_DiagMode) {
					setReturnVals(currkbadpt, 0x00, INTID_Info);
				} else {
					setReturnVals(currkbadpt, 0x51, INTID_Info);
				}
			} else if (currkbadpt->cmdReg & 0x10) {
				// Write shared ram
				logmsgf(LOGKBADPT, "KBADPT: CMD write shared ram 0x%02X: 0x%02X\n", currkbadpt->cmdReg & 0x0F, currkbadpt->PA);
				currkbadpt->sharedRam[currkbadpt->cmdReg & 0x0F] = currkbadpt->PA;
				setReturnVals(currkbadpt, 0x00, INTID_Info);
			}
		}
	}
}