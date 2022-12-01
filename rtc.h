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
};

void initRTC (struct structrtc* currrtc, struct ioBusStruct* ioBusPointer, uint32_t ioaddr, uint32_t ioaddrMask);
void accessRTC (struct structrtc* currrtc);
#endif