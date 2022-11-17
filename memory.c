// Memory
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <mmu.h>
#include <logfac.h>

uint32_t memmax;

uint8_t* meminit (unsigned int memsizemb) {
	uint8_t* memptr;
	if (memsizemb > 16) { return NULL; }
	memmax = memsizemb*1048576;
	memptr = (uint8_t*)malloc(memmax);
	return memptr;
}

void memwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes) {
	switch (bytes) {
		case BYTE:
			ptr[addr] = data;
			break;
		case HALFWORD:
			memputhw(ptr, addr & 0xFFFFFFFE, data);
			break;
		case WORD:
			memputw(ptr, addr & 0xFFFFFFFC, data);
			break;
	}
}

uint32_t memread (uint8_t* ptr, uint32_t addr, uint8_t bytes) {
	uint32_t data;
	switch (bytes) {
		case BYTE:
			data = ptr[addr];
			break;
		case HALFWORD:
			data = memgethw(ptr, addr & 0xFFFFFFFE);
			break;
		case WORD:
			data = memgetw(ptr, addr & 0xFFFFFFFC);
			break;
	}
	return data;
}

uint16_t memgethw (uint8_t *memptr, uint32_t addr) {
	if (addr > memmax) {logmsgf(LOGMEM, "ERROR: Attempted mem access (0x%08X) outside of valid memory range (0x%08X)", addr, memmax); return 0;}
	return (memptr[addr] << 8) | memptr[addr+1];
}

uint32_t memgetw (uint8_t *memptr, uint32_t addr) {
	if (addr > memmax) {logmsgf(LOGMEM, "ERROR: Attempted mem access (0x%08X) outside of valid memory range (0x%08X)", addr, memmax); return 0;}
	return (memptr[addr] << 24) | (memptr[addr+1] << 16) | (memptr[addr+2] << 8) | memptr[addr+3];
}

void memputhw (uint8_t *memptr, uint32_t addr, uint16_t data) {
	if (addr > memmax) {logmsgf(LOGMEM, "ERROR: Attempted mem access (0x%08X) outside of valid memory range (0x%08X)", addr, memmax); return;}
	memptr[addr] == (data & 0x0000FF00) >> 8;
	memptr[addr+1] == (data & 0x000000FF);
}

void memputw (uint8_t *memptr, uint32_t addr, uint32_t data) {
	if (addr > memmax) {logmsgf(LOGMEM, "ERROR: Attempted mem access (0x%08X) outside of valid memory range (0x%08X)", addr, memmax); return;}
	memptr[addr] == (data & 0xFF000000) >> 24;
	memptr[addr+1] == (data & 0x00FF0000) >> 16;
	memptr[addr+2] == (data & 0x0000FF00) >> 8;
	memptr[addr+3] == (data & 0x000000FF);
}