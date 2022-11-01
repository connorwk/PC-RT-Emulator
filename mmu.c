// MMU Emulation
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <mmu.h>
#include <memory.h>
#include <logfac.h>

uint8_t* memory;

void mmuinit (uint8_t* memptr) {
	memory = memptr;
}

void mmuwrite (uint32_t addr, uint32_t data, uint8_t bytes, uint8_t mode) {
	if (mode == DIRECT) {
		// This "DIRECT" is for instructions or processor actions that must directly address memory
		// For example, when saving/loading program statuses
		switch (bytes) {
			case BYTE:
				memory[addr] = data;
				break;
			case HALFWORD:
				memputhw(memory, addr, data);
				break;
			case WORD:
				memputw(memory, addr, data);
				break;
		}
	} else {
		// TODO: Implement translation logic
		switch (bytes) {
			case BYTE:
				memory[addr] = data;
				break;
			case HALFWORD:
				memputhw(memory, addr, data);
				break;
			case WORD:
				memputw(memory, addr, data);
				break;
		}
	}
}

uint32_t mmuread (uint32_t addr, uint8_t bytes, uint8_t mode) {
	uint32_t data;
	if (mode == DIRECT) {
		// This "DIRECT" is for instructions or processor actions that must directly address memory
		// For example, when saving/loading program statuses
		switch (bytes) {
			case BYTE:
				data = memory[addr];
				break;
			case HALFWORD:
				data = memgethw(memory, addr);
				break;
			case WORD:
				data = memputw(memory, addr);
				break;
		}
	} else {
		// TODO: Implement translation logic
		switch (bytes) {
			case BYTE:
				data = memory[addr];
				break;
			case HALFWORD:
				data = memgethw(memory, addr);
				break;
			case WORD:
				data = memputw(memory, addr);
				break;
		}
	}
	return data;
}