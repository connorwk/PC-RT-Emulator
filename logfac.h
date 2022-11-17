// Logging Facility
#ifndef _LOGFAC
#define _LOGFAC
// Logging types
#define LOGALL 		0xFFFFFFFF
#define LOGINSTR	0x00000001
#define LOGPROC 	0x00000002
#define LOGMEM		0x00000004
#define LOGMMU		0x00000008

void loginit (const char *file);
void enlogtypes (unsigned int type);
int logmsgf (unsigned int type, const char *format, ...);

// Helper functions for logging to get human readable text.

const char* getCSname (unsigned int CSnum);
const char* gpr_or_0 (unsigned int r3);
#endif