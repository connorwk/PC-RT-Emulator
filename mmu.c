// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"
#include "romp.h"
#include "memory.h"
#include "io.h"
#include "logfac.h"

uint8_t* memory;
uint8_t* rom;

union MMUIOspace* iommuregs;
uint8_t MEARlocked = 0;
uint8_t RMDRlocked = 0;
uint32_t* ICSptr;
uint8_t lastUsedTLB[16] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Lookup tables
static const uint32_t specsizelookup[16] = {0, 65535, 65535, 65535, 65535, 65535, 65535, 65535, 131071, 262143, 524287, 1048575, 2097151, 4194303, 8388607, 16777215};
// Used in HAT/IPT Base address calculation, the value is how far left to shift and thus "multiply" the result
static const uint32_t HATIPTBaseAddrMultLookup[32] = {0, 9, 9, 9, 9, 9, 9, 9, 10, 11, 12, 13, 14, 15, 16, 17, 0, 8, 8, 8, 8, 8, 8, 8, 9, 10, 11, 12, 13, 14, 15, 16};
static const uint32_t HATAddrGenMask[32] = {0, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF, 0x00001FFF, 0, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000000F, 0x0000001F, 0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF, 0x00000FFF};
// ECC check lookup tables
static const uint8_t ECCPat10101010[256] = {0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0};
static const uint8_t ECCPat01010101[256] = {0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0};
static const uint8_t ECCPat11111111[256] = {0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0};
static const uint8_t ECCPat11000000[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t ECCPat00110000[256] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
static const uint8_t ECCPat00001100[256] = {0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0};
static const uint8_t ECCPat00000011[256] = {0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0};

static const uint32_t syndromeLookup[256] = {0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000002, 0x00000000, 0x00000000, 0x00000100, 0x00000200, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000004, 0x00000008, 0x00000000, 0x00000000, 0x00000400, 0x00000800, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000010, 0x00000020, 0x00000000, 0x00000000, 0x00001000, 0x00002000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00010000, 0x00040000, 0x00000000, 0x00100000, 0x00000000, 0x00000000, 0x00000000, 0x00400000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x01000000, 0x04000000, 0x00000000, 0x10000000, 0x00000000, 0x00000000, 0x00000000, 0x40000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000040, 0x00000080, 0x00000000, 0x00000000, 0x00004000, 0x00008000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00020000, 0x00080000, 0x00000000, 0x00200000, 0x00000000, 0x00000000, 0x00000000, 0x00800000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x02000000, 0x08000000, 0x00000000, 0x20000000, 0x00000000, 0x00000000, 0x00000000, 0x80000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000};

// Globals for MMU
uint32_t effectiveAddr;
uint8_t tag;
uint8_t loadOrStore;
uint8_t instFetch;
uint8_t inIPTSearch = 0;
uint8_t ECCbits;
uint32_t correctedData;

void rominit (const char *file) {
	FILE *f = fopen(file, "rb");
	rom = (uint8_t*)malloc(ROMSIZE);
	if (f) {
		fread(rom, ROMSIZE, 1, f);
	} else {
		logmsgf(LOGMMU, "MMU: Error could open file (%s) to init proc diag ROM.", file);
	}
}

void mmuinit (uint8_t* memptr, uint32_t* SCRICSptr) {
	memory = memptr;
	iommuregs = malloc(MMUCONFIGSIZE*4);
	memset(iommuregs->_direct, 0, MMUCONFIGSIZE);
	ICSptr = SCRICSptr;
}

void loadMEAR(void) {
	if (!MEARlocked && !RMDRlocked) {
		iommuregs->MemExceptionAddr = effectiveAddr;
	}
}

void updateMERandMEAR (uint32_t merBit) {
	logmsgf(LOGMMU, "MMU: Error MERbits to set: 0x%08X MER: 0x%08X Effective Addr: 0x%08X\n", merBit, iommuregs->MemException, effectiveAddr);
	// If override tag don't set any exception bits...
	if (tag == TAG_OVERRIDE) {return;}

	if (tag == TAG_PROC) {
		// If SegProt, IPT Spec, Page Fault, Protection, or Data Error is hit
		if (merBit & MERMultiCheck) {
			// If any other exceptions are already set, set Multiple Exception flag
			if ((iommuregs->MemException & MERMultiCheck) || MEARlocked) {
				iommuregs->MemException |= MERMultExcp;
			}
			if (!instFetch) {
				loadMEAR();
				MEARlocked = 1;
			}
		}

		iommuregs->MemException |= merBit;

		// if !RASDiagMode
					// Exception reply

		if (loadOrStore) {
			iommuregs->MemException |= MERAccType;
		} else {
			iommuregs->MemException &= ~MERAccType;
		}
	}

	// Set Extern Device Exception accordingly
	if ((merBit & MERMultiCheck) && tag == TAG_IO) {
		iommuregs->MemException |= MERExtDevExcp;
	} else {
		iommuregs->MemException &= ~MERExtDevExcp;
	}


	switch (merBit) {
		case MERInvMemAddr:
			iommuregs->MemException |= MERInvMemAddr;
			// Exception reply for Read and Trans Write
			// Prog Check Interrupt for non-Trans Write
			loadMEAR();
			break;
		case MERInvIOAddr:
			iommuregs->MemException |= MERInvIOAddr;
			// if !RASDiagMode
				// Exception reply for I/O Read
			// Prog Check Interrupt for I/O Write
			loadMEAR();
			break;
		case MERUnCorrECC:
			iommuregs->MemException |= MERUnCorrECC;
			loadMEAR();
			MEARlocked = 1;
			if (RMDRlocked != 2) {
				iommuregs->RASModeDiag = (iommuregs->RASModeDiag & RMDR_AltChkBits) | (ECCbits << 8);
				RMDRlocked = 2;
			}
			if (!(iommuregs->TranslationCtrl & TRANSCTRLEnblRasDiag)) {
				// Exception reply for Read or Translated Write
				if (!loadOrStore || (loadOrStore && (*ICSptr & ICS_MASK_TranslateMode))) {
					progcheck(0);
				}
				machcheck(0);
			}
			break;
		case MERCorrECC:
			if (!(iommuregs->MemException & MERUnCorrECC)) {
				iommuregs->MemException |= MERCorrECC;
				loadMEAR();
				MEARlocked = 1;
				if ((iommuregs->TranslationCtrl & TRANSCTRLIntOnCorrECCErr) || (iommuregs->TranslationCtrl & TRANSCTRLEnblRasDiag)) {
					iommuregs->RASModeDiag = (iommuregs->RASModeDiag & RMDR_AltChkBits) | (ECCbits << 8);
					RMDRlocked = 1;
				}
				if ((iommuregs->TranslationCtrl & TRANSCTRLIntOnCorrECCErr) && !(iommuregs->TranslationCtrl & TRANSCTRLEnblRasDiag)) {
					machcheck(0);
				}
			}
			break;
		case MERTLBReload:
			// if TRANSCTRLIntOnSuccTLBReload
				// set bit
				// Exception reply if !RASDiagMode
			break;
		case MERWriteROM:
			iommuregs->MemException |= MERWriteROM;
			if (!MEARlocked) {
				iommuregs->MemExceptionAddr = effectiveAddr;
			}
			break;
		case MERTLBSpec:
			iommuregs->MemException |= MERTLBSpec;
			// if !RASDiagMode
				// Exception reply
				// Machine Check Interrupt
			break;
	}
}

// TODO Check ICS Memory Protect bit?
int invalidAddrCheck (uint32_t addr, uint32_t end_addr, uint8_t bytes) {
	if ((addr >= end_addr || (addr == (end_addr - 1) && bytes > BYTE) || (addr == (end_addr - 2) && bytes > HALFWORD) || (addr == (end_addr - 3) && bytes > HALFWORD)) && bytes != INST) {
		logmsgf(LOGMEM, "MEM: Error attempted access (0x%08X) outside of valid range (0x%08X)\n", addr, end_addr);
		if (inIPTSearch) {
			updateMERandMEAR(MERPageFault);
		} else {
			updateMERandMEAR(MERInvMemAddr);
		}
		return 0;
	}
	return 1;
}

uint8_t genECC(uint32_t data) {
	if ((iommuregs->TranslationCtrl & TRANSCTRLEnblRasDiag) && loadOrStore != LOAD) {
		return iommuregs->RASModeDiag & RMDR_AltChkBits;
	}

	uint8_t byte0 = (data & 0xFF000000) >> 24;
	uint8_t byte1 = (data & 0x00FF0000) >> 16;
	uint8_t byte2 = (data & 0x0000FF00) >> 8;
	uint8_t byte3 = data & 0x000000FF;

	uint8_t ECC0 = ECCPat10101010[byte0] ^ ECCPat10101010[byte1] ^ ECCPat11000000[byte2] ^ ECCPat11000000[byte3];
	uint8_t ECC1 = ECCPat01010101[byte0] ^ ECCPat01010101[byte1] ^ ECCPat00110000[byte2] ^ ECCPat00110000[byte3];
	uint8_t ECC2 = ECCPat11111111[byte0] ^ 0                     ^ ECCPat00001100[byte2] ^ ECCPat00001100[byte3];
	uint8_t ECC3 = 0                     ^ ECCPat11111111[byte1] ^ ECCPat00000011[byte2] ^ ECCPat00000011[byte3];
	uint8_t ECC4 = ECCPat11000000[byte0] ^ ECCPat11000000[byte1] ^ ECCPat11111111[byte2] ^ 0                    ;
	uint8_t ECC5 = ECCPat00110000[byte0] ^ ECCPat00110000[byte1] ^ 0                     ^ ECCPat11111111[byte3];
	uint8_t ECC6 = ECCPat00001100[byte0] ^ ECCPat00001100[byte1] ^ ECCPat10101010[byte2] ^ ECCPat10101010[byte3];
	uint8_t ECC7 = ECCPat00000011[byte0] ^ ECCPat00000011[byte1] ^ ECCPat01010101[byte2] ^ ECCPat01010101[byte3];

	return (ECC0 << 7) | (ECC1 << 6) | (ECC2 << 5) | (ECC3 << 4) | (ECC4 << 3) | (ECC5 << 2) | (ECC6 << 1) | ECC7;
}

uint8_t checkECC(uint32_t data) {
	uint8_t checkECCbits = genECC(data);
	uint8_t syndrome = ECCbits ^ checkECCbits;
	
	if (!syndrome) {
		// No error
		return 0;
	}

	if (syndromeLookup[syndrome]) {
		// Single bit flip, correctable
		correctedData = data ^ syndromeLookup[syndrome];
		updateMERandMEAR(MERCorrECC);
		return 1;
	} else if (syndrome == 0x80 || syndrome == 0x40 || syndrome == 0x20 || syndrome == 0x10 || syndrome == 0x08 || syndrome == 0x04 || syndrome == 0x02 || syndrome == 0x01) {
		// Single bit ECC bit flip, ignore
	} else {
		// Multi bit flip, uncorrectable
		updateMERandMEAR(MERUnCorrECC);
	}
	return 0;
}

void realwrite (uint32_t addr, uint32_t data, uint8_t bytes) {
	uint32_t fetchedWord;
	uint32_t completeWord;
	uint32_t addrAdjusted = addr;

	if (((iommuregs->RAMSpec & RAMSPECSize) == 0 && (iommuregs->ROMSpec & ROMSPECSize) == 0) && addr <= MAXREALADDR) {
		switch (bytes) {
			case BYTE:
				fetchedWord = realread((addrAdjusted & 0xFFFFFFFC), WORD);
				switch (addrAdjusted & 0x00000003) {
					case 0:
						completeWord = (data & 0x000000FF) << 24;
						completeWord |= fetchedWord & 0x00FFFFFF;
						break;
					case 1:
						completeWord = (data & 0x000000FF) << 16;
						completeWord |= fetchedWord & 0xFF00FFFF;
						break;
					case 2:
						completeWord = (data & 0x000000FF) << 8;
						completeWord |= fetchedWord & 0xFFFF00FF;
						break;
					case 3:
						completeWord = (data & 0x000000FF);
						completeWord |= fetchedWord & 0xFFFFFF00;
						break;
				}
				break;
			case HALFWORD:
				fetchedWord = realread((addrAdjusted & 0xFFFFFFFC), WORD);
				if (addrAdjusted & 0x00000002) {
					completeWord = (data & 0x0000FFFF);
					completeWord |= fetchedWord & 0xFFFF0000;
				} else {
					completeWord = (data & 0x0000FFFF) << 16;
					completeWord |= fetchedWord & 0x0000FFFF;
				}
				break;
			case WORD:
				completeWord = data;
				break;
		}
		memeccwrite(memory, addrAdjusted, completeWord, genECC(completeWord));
	} else if ((addr >= (ROMSPECStartAddr)) && (addr <= ROMSPECEndAddr) && ((iommuregs->ROMSpec & ROMSPECSize) != 0)) {
		logmsgf(LOGMMU, "MMU: Error attempt to write to rom.\n");
		updateMERandMEAR(MERWriteROM);
	} else if ((addr >= (RAMSPECStartAddr)) && (addr <= RAMSPECEndAddr) && ((iommuregs->RAMSpec & RAMSPECSize) != 0)) {
		addrAdjusted = addr - (RAMSPECStartAddr);
		if (invalidAddrCheck(addrAdjusted, MEMORYSIZE, bytes)) {
			switch (bytes) {
				case BYTE:
					fetchedWord = realread((addrAdjusted & 0xFFFFFFFC), WORD);
					switch (addrAdjusted & 0x00000003) {
						case 0:
							completeWord = (data & 0x000000FF) << 24;
							completeWord |= fetchedWord & 0x00FFFFFF;
							break;
						case 1:
							completeWord = (data & 0x000000FF) << 16;
							completeWord |= fetchedWord & 0xFF00FFFF;
							break;
						case 2:
							completeWord = (data & 0x000000FF) << 8;
							completeWord |= fetchedWord & 0xFFFF00FF;
							break;
						case 3:
							completeWord = (data & 0x000000FF);
							completeWord |= fetchedWord & 0xFFFFFF00;
							break;
					}
					break;
				case HALFWORD:
					fetchedWord = realread((addrAdjusted & 0xFFFFFFFC), WORD);
					if (addrAdjusted & 0x00000002) {
						completeWord = (data & 0x0000FFFF);
						completeWord |= fetchedWord & 0xFFFF0000;
					} else {
						completeWord = (data & 0x0000FFFF) << 16;
						completeWord |= fetchedWord & 0x0000FFFF;
					}
					break;
				case WORD:
					completeWord = data;
					break;
			}
			memeccwrite(memory, addrAdjusted, completeWord, genECC(completeWord));
		}
	} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
		iowrite(addr, data, bytes);
	} else {
		logmsgf(LOGMMU, "MMU: Error Memory write outside valid ranges 0x%08X: 0x%08X\n", addr, data);
	}
}

uint32_t realread (uint32_t addr, uint8_t bytes) {
	uint32_t data;
	uint64_t ECCandData;

	if (((iommuregs->RAMSpec & RAMSPECSize) == 0 && (iommuregs->ROMSpec & ROMSPECSize) == 0) && addr <= MAXREALADDR) {
		data = memread(rom, addr & 0x0000FFFF, bytes);
	} else if ((addr >= (ROMSPECStartAddr)) && (addr <= ROMSPECEndAddr) && ((iommuregs->ROMSpec & ROMSPECSize) != 0)) {
		if (invalidAddrCheck((addr - (ROMSPECStartAddr)) & 0x0000FFFF, ROMSIZE, bytes)) {
			data = memread(rom, (addr - (ROMSPECStartAddr)) & 0x0000FFFF, bytes);
		}
	} else if ((addr >= (RAMSPECStartAddr)) && (addr <= RAMSPECEndAddr) && ((iommuregs->RAMSpec & RAMSPECSize) != 0)) {
		if (invalidAddrCheck(addr - (RAMSPECStartAddr), MEMORYSIZE, bytes)) {
			ECCandData = memeccread(memory, addr - (RAMSPECStartAddr));
			ECCbits = (ECCandData & 0x000000FF00000000) >> 32;
			data = ECCandData & 0x00000000FFFFFFFF;
			if (checkECC(data)) {
				data = correctedData;
			}
			switch (bytes) {
				case BYTE:
					switch (addr & 0x00000003) {
						case 0:
							data = (data & 0xFF000000) >> 24;
							break;
						case 1:
							data = (data & 0x00FF0000) >> 16;
							break;
						case 2:
							data = (data & 0x0000FF00) >> 8;
							break;
						case 3:
							data = (data & 0x000000FF);
							break;
					}
					break;
				case HALFWORD:
					if (addr & 0x00000002) {
						data = (data & 0x0000FFFF);
					} else {
						data = (data & 0xFFFF0000) >> 16;
					}
					break;
				case INST:
					// If instruction fetch we need to get a word on the halfword boundry possibly.
					// If this is the case we need to do a SECOND real read for the second halfword.
					if (addr & 0x00000002) {
						data = (data & 0x0000FFFF) << 16;
						data |= realread(addr + 0x2, HALFWORD);
					}
					break;
			}
		}
	} else if ((addr >= IOChanIOMapStartAddr) && (addr <= IOChanIOMapEndAddr)) {
		data = ioread(addr, bytes);
	} else {
		logmsgf(LOGMMU, "MMU: Error Memory read outside valid ranges 0x%08X\n", addr);
		data = 0;
	}
	return data;
}

uint8_t memProtectAndLockbitCheck(uint32_t segment, uint32_t TLBNum, uint32_t virtPageIdx, uint32_t RealPageNum_VBs_KBs, uint32_t WB_TransID_LBs) {
	uint32_t lockbitLine = virtPageIdx & 0x0000000F;
	uint32_t lockbit = WB_TransID_LBs & (0x00001000 >> lockbitLine);
	
	// Check for memory access exceptions.
	if (segment & SEGREGSpecial) {
		// Lockbit processing pg 11-110
		if (((WB_TransID_LBs & TLBTransactionID) >> 16) == iommuregs->TransactionID) {
			if (WB_TransID_LBs & TLBWriteBit) {
				if (loadOrStore == STORE && !lockbit) {
					// Lockbit Violation
					updateMERandMEAR(MERData);
					return 0;
				}
			} else {
				if (loadOrStore == STORE || !lockbit) {
					// Lockbit Violation
					updateMERandMEAR(MERData);
					return 0;
				}
			}
		} else {
			// TID Not equal
			updateMERandMEAR(MERData);
			return 0;
		}
	} else {
		// Memory protection processing pg 11-109
		switch (RealPageNum_VBs_KBs & TLBKeyBits) {
			case 0x00000000:
				// Key 0 Fetch-Protected
				if (segment & SEGREGKeyBit) {
					// Not Allowed
					updateMERandMEAR(MERProtect);
					return 0;
				}
				break;
			case 0x00000001:
				// Key 0 Read/Write
				if (segment & SEGREGKeyBit) {
					if (loadOrStore == STORE) {
						// Not Allowed
						updateMERandMEAR(MERProtect);
						return 0;
					}
				}
				break;
			case 0x00000002:
				// Public Read/Write
				// Always Allowed
				return 1;
				break;
			case 0x00000003:
				// Public Read-Only
				if (loadOrStore == STORE) {
					// Not Allowed
					updateMERandMEAR(MERProtect);
					return 0;
				}
				break;
		}
	}
	return 1;
}

uint32_t findIPT (uint32_t genAddrTag, uint32_t TLBNum, uint32_t virtPageIdx, uint32_t segment) {
	uint32_t segID = (segment & SEGREGSegID) >> 2;
	int page4k = (iommuregs->TranslationCtrl & TRANSCTRLPageSize) ? 1 : 0;
	uint32_t realPgMask = page4k? TLBRealPgNum4K : TLBRealPgNum2K;
	uint32_t lookupIdx = ((iommuregs->TranslationCtrl & TRANSCTRLPageSize) >> 4) | (iommuregs->RAMSpec & RAMSPECSize);
	uint32_t baseAddrHATIPT = page4k ? (iommuregs->TranslationCtrl & TRANSCTRLHATIPTBaseAddr4K) : (iommuregs->TranslationCtrl & TRANSCTRLHATIPTBaseAddr2K);
	baseAddrHATIPT = baseAddrHATIPT << HATIPTBaseAddrMultLookup[lookupIdx];
	uint32_t effectiveAddrBits = page4k ? (virtPageIdx >> 1) : virtPageIdx;
	uint32_t offsetHAT = ((effectiveAddrBits ^ segID) & HATAddrGenMask[lookupIdx]) << 4;

	uint32_t HATentry;
	uint32_t IPTentry;
	uint32_t Ctrlentry;
	uint32_t HATIPTaddr = baseAddrHATIPT + offsetHAT;

	uint32_t searchCnt = 0;
	inIPTSearch = 1;
	
	HATentry = realread(HATIPTaddr | 0x4, WORD);
	if (HATentry & HATIPT_EmptyBit) {
		updateMERandMEAR(MERIPTSpecErr & MERPageFault);
	} else {
		HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_HATPtr) >> 12);
		IPTentry = realread(HATIPTaddr, WORD);
		HATentry = realread(HATIPTaddr | 0x4, WORD);
		while (1) {
			searchCnt++;
			if ((IPTentry & HATIPT_AddrTag) == (genAddrTag | TLBNum)) {
				// TLB Reload
				logmsgf(LOGMMU, "MMU: IPT Entry found at: 0x%08X\n", HATIPTaddr);
				if (lastUsedTLB[TLBNum]) {
					logmsgf(LOGMMU, "MMU: Reloading TLB0[%d].\n", TLBNum);
					lastUsedTLB[TLBNum] = 0;
					iommuregs->TLB0_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB0_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x8, WORD);
					}
					if (memProtectAndLockbitCheck(segment, virtPageIdx, TLBNum, iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum], iommuregs->TLB0_WB_TransID_LBs[TLBNum])) {
						return ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
					} else {
						return 0xFFFFFFFF;
					}
					
				} else {
					logmsgf(LOGMMU, "MMU: Reloading TLB1[%d].\n", TLBNum);
					lastUsedTLB[TLBNum] = 1;
					iommuregs->TLB1_AddrTagField[TLBNum] = genAddrTag;
					// Uses HATIPTaddr because its using the previously fetched IPT pointer for the response, not the one in this IPT.
					iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] = ((HATIPTaddr & 0x00001FFF) >> 1) | TLBValidBit | ((IPTentry & HATIPT_Key) >> 30);
					if (segment & SEGREGSpecial) {
						iommuregs->TLB1_WB_TransID_LBs[TLBNum] = realread(HATIPTaddr | 0x8, WORD);
					}
					if (memProtectAndLockbitCheck(segment, virtPageIdx, TLBNum, iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum], iommuregs->TLB1_WB_TransID_LBs[TLBNum])) {
						return ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
					} else {
						return 0xFFFFFFFF;
					}
				}
			} else {
				if (HATentry & HATIPT_LastBit) {
					updateMERandMEAR(MERIPTSpecErr & MERPageFault);
					break;
				} else {
					if (searchCnt == 127 && (iommuregs->TranslationCtrl & TRANSCTRLTermLongIPTSearch)) {
						updateMERandMEAR(MERIPTSpecErr & MERPageFault);
						break;
					}
					HATIPTaddr = baseAddrHATIPT + ((HATentry & HATIPT_IPTPtr) << 4);
					IPTentry = realread(HATIPTaddr, WORD);
					HATentry = realread(HATIPTaddr | 0x4, WORD);
				}
			}
		};
	}
	return 0xFFFFFFFF;
}

uint32_t checkTLB (uint32_t segment, uint32_t virtPageIdx, uint8_t TLBNum) {
	uint32_t segID = (segment & SEGREGSegID) >> 2;
	uint32_t realPgMask = (iommuregs->TranslationCtrl & TRANSCTRLPageSize) ? TLBRealPgNum4K : TLBRealPgNum2K;
	uint32_t realAddr = 0;
	uint32_t hashAnchorTableEntry;
	uint32_t genAddrTag = ((segID << 17) | (virtPageIdx & 0x0001FFF0));
	uint8_t TLBused = 0;

	if (iommuregs->TLB0_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 0;
			realAddr |= ((iommuregs->TLB0_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
			TLBused++;
		}
	}
	
	if (iommuregs->TLB1_AddrTagField[TLBNum] == genAddrTag) {
		if (iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & TLBValidBit) {
			lastUsedTLB[TLBNum] = 1;
			realAddr |= ((iommuregs->TLB1_RealPageNum_VBs_KBs[TLBNum] & realPgMask) << 8);
			TLBused++;
		}
	}

	if (!TLBused) {
		logmsgf(LOGMMU, "MMU: Neither TLB0 and TLB1 match. Preforming IPT search.\n");
		realAddr = findIPT(genAddrTag, TLBNum, virtPageIdx, segment);
	}

	if (TLBused == 2) {
		updateMERandMEAR(MERTLBSpec);
	}
	
	inIPTSearch = 0;
	return realAddr;
}

uint32_t translateaddr(uint32_t segment) {
	uint32_t virtPageIdx;
	uint32_t pageDisp;
	uint8_t TLBNum;
	uint32_t realAddr;
	
	if ((iommuregs->TranslationCtrl & TRANSCTRLSegReg0VirtEqReal) && (effectiveAddr & 0xF0000000) == 0) {
		// Memory protection and Lockbit processing disabled for Virtual Equal Real accesses
		return effectiveAddr & 0x00FFFFFF;
	} else if (iommuregs->TranslationCtrl & TRANSCTRLPageSize) {
		// 4K Pages
		virtPageIdx = (effectiveAddr & 0x0FFFF000) >> 11;
		pageDisp = effectiveAddr & 0x00000FFF;
		TLBNum = virtPageIdx & 0x0000000F;
		
		realAddr = checkTLB(segment, virtPageIdx, TLBNum) | pageDisp;
	} else {
		// 2K Pages
		virtPageIdx = (effectiveAddr & 0x0FFFF800) >> 11;
		pageDisp = effectiveAddr & 0x000007FF;
		TLBNum = virtPageIdx & 0x0000000F;

		realAddr = checkTLB(segment, virtPageIdx, TLBNum) | pageDisp;
	}
	return realAddr;
}

uint32_t translatecheck() {
	uint32_t realAddr;
	uint32_t segment = iommuregs->_direct[(effectiveAddr & 0xF0000000) >> 28];
	
	if (segment & SEGREGPresent) {
		if ((segment & SEGREGProcAcc) && tag == TAG_PROC) {
			// Segment protected from PROC access
			updateMERandMEAR(MERSegProtV);
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from Processor accesses.\n", (effectiveAddr & 0xF0000000) >> 28);
		} else if ((segment & SEGREGIOAcc) && tag == TAG_IO) {
			// Segment protected from IO access
			updateMERandMEAR(MERSegProtV);
			logmsgf(LOGMMU, "MMU: Error Segment %d is protected from I/O accesses.\n", (effectiveAddr & 0xF0000000) >> 28);
		} else {
			realAddr = translateaddr(segment);
			// TODO: Update R/C bits
			logmsgf(LOGMMU, "MMU: Address (0x%08X) translated to: 0x%08X\n", effectiveAddr, realAddr);
		}
	} else {
		// Segment disabled.
		// Supposedly we just ignore the request allowing multiple MMUs/devices on the Proc channel.
		// But I assume that some failure of an access happens on the processor side... TODO
		logmsgf(LOGMMU, "MMU: Error Segment %d is disabled.\n", (effectiveAddr & 0xF0000000) >> 28);
	}
	return realAddr;
}

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode, uint8_t inputtag) {
	//logmsgf(LOGMMU, "MMU: Write 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = STORE;
	if (bytes == INST) {instFetch = 1;} else {instFetch = 0;}

	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			realwrite(translatecheck(), data, bytes);
		} else {
			realwrite(addr, data, bytes);
		}
	} else {
		// PIO
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			logmsgf(LOGMMU, "MMU: Write to IO Base Addr Reg DIRECT 0x%08X\n", data);
			iommuregs->IOBaseAddr = data;
		} else if (addr >= (iommuregs->IOBaseAddr << 16) && addr < ((iommuregs->IOBaseAddr << 16) + MMUCONFIGSIZE)){
			logmsgf(LOGMMU, "MMU: Write to IOMMU Regs Decoded 0x%08X: 0x%08X\n", addr & 0x0000FFFF, data);
			if ( ((addr & 0x0000FFFF) >= 0x1000) && ((addr & 0x0000FFFF) <= 0x2FFF) ) {
				// Only last two bits of Ref/Change regs are valid
				iommuregs->_direct[addr & 0x0000FFFF] = (data & 0x00000003);
			}
			switch(addr & 0x0000FFFF) {
				case 0x0018:
					// Only last byte of RDMR can be set
					iommuregs->_direct[addr & 0x0000FFFF] = (data & 0x000000FF);
				case 0x0080:
					// Invalidate Entire TLB
					for (int i=0; i < 16; i++) {
						iommuregs->TLB0_RealPageNum_VBs_KBs[i] &= ~TLBValidBit;
						iommuregs->TLB1_RealPageNum_VBs_KBs[i] &= ~TLBValidBit;
					}
					break;
				default:
					iommuregs->_direct[addr & 0x0000FFFF] = data;
					break;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO write outside valid ranges 0x%08X: 0x%08X\n", addr, data);
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode, uint8_t inputtag) {
	uint32_t data = 0;
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = LOAD;
	if (bytes == INST) {instFetch = 1;} else {instFetch = 0;}

	if (mode == MEMORY) {
		if (*ICSptr & ICS_MASK_TranslateMode) {
			data = realread(translatecheck(), bytes);
		} else {
			data = realread(addr, bytes);
		}
	} else {
		// PIO
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			data = iommuregs->IOBaseAddr;
			logmsgf(LOGMMU, "MMU: Read from IO Base Addr Reg DIRECT 0x%08X\n", data);
		} else if (addr >= (iommuregs->IOBaseAddr << 16) && addr < ((iommuregs->IOBaseAddr << 16) + MMUCONFIGSIZE)){
			data = iommuregs->_direct[addr & 0x0000FFFF];
			logmsgf(LOGMMU, "MMU: Read from IOMMU Regs Decoded 0x%08X: 0x%08X\n", addr & 0x0000FFFF, data);
			switch ((addr & 0x0000FFFF)) {
				case 0x0011:
					if (RMDRlocked != 2) {
						MEARlocked = 0;
					}
					break;
				case 0x0018:
					RMDRlocked = 0;
					break;
			}
		} else {
			logmsgf(LOGMMU, "MMU: Error PIO read outside valid ranges 0x%08X\n", addr);
			data = 0;
		}
	}
	//logmsgf(LOGMMU, "MMU: Read 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}

uint32_t proctsh (uint32_t addr, uint8_t bytes, uint8_t inputtag) {
	uint32_t data = 0;
	effectiveAddr = addr;
	tag = inputtag;
	loadOrStore = STORE;

	// TSH is treated as a STORE pg. 11-109
	if (*ICSptr & ICS_MASK_TranslateMode) {
		data = realread(translatecheck(), bytes);
		realwrite(translatecheck(), 0xFF, BYTE);
	} else {
		data = realread(addr, bytes);
		realwrite(addr, 0xFF, BYTE);
	}
	//logmsgf(LOGMMU, "MMU: Test & Set 0x%08X: 0x%08X  %d, %d\n", addr, data, bytes, mode);
	return data;
}