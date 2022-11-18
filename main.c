// IBM PC RT Emulator

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "logfac.h"
#include "romp.h"
#include "mmu.h"
#include "io.h"
#include "memory.h"

int main (void) {
	loginit("log.txt");
	enlogtypes(LOGALL);
	mmuinit(meminit(8));
	rominit("79X34xx.BIN");
	ioinit();
	procinit();

	while(1) {
		//if (getchar() == 'q') {break;}
		//printregs();
		//for (int i=0; i < 64; i++) {
		fetch();
	}

	logend();
	return 0;
}