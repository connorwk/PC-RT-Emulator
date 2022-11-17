// Logging Facitiy

#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

#include "logfac.h"

FILE *logfile = NULL;
unsigned int logtype = 0;

void loginit (const char *file) {
	logfile = fopen(file, "w");
	if (!logfile) {
		logfile = NULL;
	}
}

void logend (void) {
	if (logfile != NULL) {
		fclose(logfile);
	}
}

void enlogtypes (unsigned int type) {
	logtype = type;
}

int logmsgf (unsigned int type, const char *format, ...) {
	va_list args;
	int ret;
	if (logfile != NULL && logtype & type) {
		va_start(args, format);
		ret = vfprintf(logfile, format, args);
		fflush(logfile);
		va_end(args);
	}
}

const char* getCSname (unsigned int CSnum) {
	switch (CSnum) {
		case 0x8:
			return "0";
		case 0x9:
			return "LT";
		case 0xA:
			return "EQ";
		case 0xB:
			return "GT";
		case 0xC:
			return "C0";
		case 0xE:
			return "OV";
		case 0xF:
			return "TB";
		default:
			return "RESV";
	}
}

const char* gpr_or_0 (unsigned int r3) {
	switch (r3) {
		case 0x0:
			return "0";
		case 0x1:
			return "GPR1";
		case 0x2:
			return "GPR2";
		case 0x3:
			return "GPR3";
		case 0x4:
			return "GPR4";
		case 0x5:
			return "GPR5";
		case 0x6:
			return "GPR6";
		case 0x7:
			return "GPR7";
		case 0x8:
			return "GPR8";
		case 0x9:
			return "GPR10";
		case 0xA:
			return "GPR11";
		case 0xB:
			return "GPR12";
		case 0xC:
			return "GPR13";
		case 0xD:
			return "GPR14";
		case 0xE:
			return "GPR15";
		case 0xF:
			return "GPR16";
	}
}