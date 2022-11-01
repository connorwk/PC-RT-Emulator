// IBM PC RT Emulator

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <logfac.h>
#include <romp.h>
#include <memory.h>

void main (void) {
	loginit("log.txt");
	mmuinit(meminit(8));

	logend();
}