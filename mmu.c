// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <mmu.h>
#include <memory.h>
#include <logfac.h>

uint8_t* memory;
union MMUIOspace* iommuregs;

void mmuinit (uint8_t* memptr) {
	memory = memptr;
	iommuregs = (uint8_t*)malloc(MMUCONFIGSIZE);
	memset(iommuregs->_direct, 0, MMUCONFIGSIZE);
}

void mmuwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes) {
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

uint32_t mmuread (uint8_t* ptr, uint32_t addr, uint8_t bytes) {
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

void procwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode) {
	if (mode == MEMORY) {
		mmuwrite(memory, addr, data, bytes);
	} else {
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			mmuwrite(iommuregs->IOBaseAddr, 0, data, bytes);
		} else if (addr > iommuregs->IOBaseAddr && addr < iommuregs->IOBaseAddr+MMUCONFIGSIZE){
			mmuwrite(iommuregs->_direct, addr, data, bytes);
		}
	}
}

uint32_t procread (uint32_t addr, uint8_t bytes, uint8_t mode) {
	uint32_t data;
	if (mode == MEMORY) {
		data = mmuread(memory, addr, bytes);
	} else {
		if (addr == 0x00808000) {
			// Special access to allow setup of IO Base Addr Reg directly
			data = mmuread(iommuregs->IOBaseAddr, 0, bytes);
		} else if (addr > iommuregs->IOBaseAddr && addr < iommuregs->IOBaseAddr+MMUCONFIGSIZE){
			data = mmuread(iommuregs->_direct, addr, bytes);
		}
	}
	return data;
}