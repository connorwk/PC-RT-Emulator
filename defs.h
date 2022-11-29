// Global defines
#ifndef _DEFS
#define _DEFS
#include <stdint.h>

// PROC Bus
struct procBusStruct {
	uint32_t addr;	// Address for access
	uint32_t data;	// Data given or received from access
	uint8_t width;	// 1, 2, 4 bytes or a test and set (instruction fetch mode snuck in here too)
	uint8_t rw;			// 1 if a STORE operation
	uint8_t pio;		// PIO access for I/O Registers (not to be confused with the register addresses for I/O cards)
	uint8_t tag;		// Source of access TAG_PROC or TAG_IO (TAG_OVERRIDE provides special functionality for debug)
	uint8_t intrpt;	// Interrupt lines 0-4
	uint8_t flags;	// Exception, Trap
	uint8_t priv;		// Privileged State
};

// All defines for Proccessor Bus
#define WIDTH_BYTE			0
#define WIDTH_HALFWORD	1
#define WIDTH_WORD			2
#define WIDTH_TESTSET		3
#define WIDTH_INST			4 // Special for Instruction fetch to NOT ignore LSBs of addr.

#define RW_LOAD		0
#define RW_STORE	1

#define PIO_REAL		1
#define PIO_TRANS		2
#define PIO_PIO			3

#define TAG_PROC			0
#define TAG_IO				1
#define TAG_OVERRIDE	2

// pg 1-37
#define INTRPT_0_SysAttnKeyboard	0x80
#define INTRPT_1_RealTimeClock		0x40
#define INTRPT_2_IOCCErrors				0x20
#define INTRPT_2_MMUProgCheck			0x20
#define INTRPT_3_IOChan						0x10
#define INTRPT_4_IOChan						0x08

#define FLAGS_Trap			0x01
#define FLAGS_Exception 0x02 

#define PRIV_Privileged	0
#define PRIV_UnPrivileged 1

// ISA Bus
struct isaBusStruct {
	uint32_t addr;	// Address (24-Bit max)
	uint32_t data;	// Data (8 or 16 Bit)
	uint8_t width;	// 1, 2, 4 bytes or a test and set (instruction fetch mode snuck in here too)
	uint8_t rw;			// 1 if a STORE operation
	uint8_t pio;		// PIO access for I/O Registers (not to be confused with the register addresses for I/O cards)
	uint8_t tag;		// Source of access TAG_PROC or TAG_IO (TAG_OVERRIDE provides special functionality for debug)
	uint8_t intrpt;	// Interrupt lines 0-4
	uint8_t flags;	// Exception, Trap
	uint8_t priv;		// Privileged State
};


#endif