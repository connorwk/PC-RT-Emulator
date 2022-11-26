// Memory
#ifndef _MEMORY
#define _MEMORY
#include <stdint.h>

// Memory size max is 16MB
#define MEMORYSIZEMB 8
#define MEMORYSIZE MEMORYSIZEMB*1048576

uint8_t* meminit (void);
void memwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t memread (uint8_t* ptr, uint32_t addr, uint8_t bytes);
void memeccwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t ECCbits);
uint64_t memeccread (uint8_t* ptr, uint32_t addr);
uint16_t memgethw (uint8_t *memptr, uint32_t addr);
uint32_t memgetw (uint8_t *memptr, uint32_t addr);
void memputhw (uint8_t *memptr, uint32_t addr, uint16_t data);
void memputw (uint8_t *memptr, uint32_t addr, uint32_t data);
#endif