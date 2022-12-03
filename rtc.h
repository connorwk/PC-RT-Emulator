// RTC Emulation
#ifndef _RTC
#define _RTC
#include <stdint.h>
#include "iocc.h"

struct structrtc {
	uint32_t ioAddress;
	uint32_t ioAddressMask;
	union {
		uint8_t _direct[64];
		struct {
			uint8_t seconds;
			uint8_t secondsAlarm;
			uint8_t minutes;
			uint8_t minutesAlarm;
			uint8_t hours;
			uint8_t hoursAlarm;
			uint8_t dayOfWeek;
			uint8_t dateOfMonth;
			uint8_t month;
			uint8_t year;
			uint8_t regA;
			uint8_t regB;
			uint8_t regC;
			uint8_t regD;
			uint8_t rtcMem[50];
		};
	};
	uint64_t prevticks;
	uint8_t sqwOut;
	uint8_t intReq;
};

#define REGB_SET							0x80
#define REGB_PeriodicIntEn		0x40
#define REGB_AlarmIntEn				0x20
#define REGB_UpdateEndIntEn		0x10
#define REGB_SquareWaveEn			0x08
#define REGB_DataMode					0x04
#define REGB_24_12_Mode				0x02
#define REGB_DaylightSavings	0x01

#define REGC_IRQFlag			0x80
#define REGC_PeriodicFlag	0x40
#define REGC_AlarmFlag		0x20
#define REGC_UpdateEnded	0x10

void initRTC (struct structrtc* currrtc, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask);
void accessRTC (struct structrtc* currrtc);
void cycleRTC (struct structrtc* currrtc);
#endif