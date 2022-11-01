// Memory
#include <stdint.h>

uint8_t* meminit (unsigned int memsizemb);

uint16_t memgethw (uint8_t *memptr, uint32_t addr);
uint32_t memgetw (uint8_t *memptr, uint32_t addr);
void memputhw (uint8_t *memptr, uint32_t addr, uint16_t data);
void memputw (uint8_t *memptr, uint32_t addr, uint32_t data);