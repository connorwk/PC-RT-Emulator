// 8259A Interrupt Controller Emulation
#ifndef _8259A
#define _8259A
#include <stdint.h>

struct struct8259 {
	uint8_t reset;
	uint8_t initreq;
	uint8_t icw1;
	uint8_t icw2;
	uint8_t icw3;
	uint8_t icw4;
	uint8_t ocw1;
	uint8_t ocw3;
	uint8_t intlines;
	uint8_t irr;
	uint8_t isr;
	uint8_t intreq;
};

#define ICW1_InterruptVectAddr	0xE0
#define ICW1_TriggerMode				0x08
#define ICW1_AddrInterval				0x04
#define ICW1_SingleMode					0x02
#define ICW1_ICW4Needed					0x01

#define OCW2_CMD		0xE0
#define OCW2_Level	0x07
#define OCW2_CMD_NonSpecEOI		0x20
#define OCW2_CMD_SpecEOI			0x60

#define OCW3_SpecialMaskMode	0x60
#define OCW3_PollingCmd				0x04
#define OCW3_ReadRegCmd				0x03

void init8259(struct struct8259* curr8259, struct procBusStruct* procBusPointer);
void write8259(struct struct8259* curr8259);
void read8259(struct struct8259* curr8259);
void access8259 (struct struct8259* curr8259);
void cycle8259 (struct struct8259* curr8259);

#endif