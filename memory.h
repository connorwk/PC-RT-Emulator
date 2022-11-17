// Memory
#include <stdint.h>

uint8_t* meminit (unsigned int memsizemb);
void memwrite (uint8_t* ptr, uint32_t addr, uint32_t data, uint8_t bytes);
uint32_t memread (uint8_t* ptr, uint32_t addr, uint8_t bytes);
uint16_t memgethw (uint8_t *memptr, uint32_t addr);
uint32_t memgetw (uint8_t *memptr, uint32_t addr);
void memputhw (uint8_t *memptr, uint32_t addr, uint16_t data);
void memputw (uint8_t *memptr, uint32_t addr, uint32_t data);