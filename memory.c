// Memory
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "mmu.h"
#include "logfac.h"
#include "memory.h"

uint8_t* eccmemptr;

uint8_t* meminit (void) {
	uint8_t* memptr;
	memptr = (uint8_t*)malloc(MEMORYSIZE);
	eccmemptr = (uint8_t*)malloc(MEMORYSIZE/4);
	return memptr;
}

uint16_t memgethw (uint8_t *memptr, uint32_t addr) {
	return (memptr[addr] << 8) | memptr[addr+1];
}

uint32_t memgetw (uint8_t *memptr, uint32_t addr) {
	return (memptr[addr] << 24) | (memptr[addr+1] << 16) | (memptr[addr+2] << 8) | memptr[addr+3];
}

void memputhw (uint8_t *memptr, uint32_t addr, uint16_t data) {
	memptr[addr] = (data & 0x0000FF00) >> 8;
	memptr[addr+1] = (data & 0x000000FF);
}

void memputw (uint8_t *memptr, uint32_t addr, uint32_t data) {
	memptr[addr] = (data & 0xFF000000) >> 24;
	memptr[addr+1] = (data & 0x00FF0000) >> 16;
	memptr[addr+2] = (data & 0x0000FF00) >> 8;
	memptr[addr+3] = (data & 0x000000FF);
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
		case INST:
			data = memgetw(ptr, addr);
			break;
	}
	return data;
}

void memeccwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t ECCbits) {
	memputw(ptr, addr & 0xFFFFFFFC, data);
	eccmemptr[(addr & 0xFFFFFFFC) >> 2] = ECCbits;
}

uint64_t memeccread (uint8_t* ptr, uint32_t addr) {
	uint64_t data;
	data = eccmemptr[(addr & 0xFFFFFFFC) >> 2];
	data = data << 32;
	data |= memgetw(ptr, addr & 0xFFFFFFFC);
	return data;
}