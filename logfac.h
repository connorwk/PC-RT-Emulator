// Logging Facility

// Logging types
#define LOGALL 		0xFFFFFFFF
#define LOGPROC 	0x00000001
#define LOGMEM		0x00000002

void loginit (const char *file);
void enlogtypes (unsigned int type);
int logmsgf (unsigned int type, const char *format, ...);