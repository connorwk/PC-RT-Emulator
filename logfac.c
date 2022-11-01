// Logging Facitiy

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <logfac.h>

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
		va_end(args);
	}
}