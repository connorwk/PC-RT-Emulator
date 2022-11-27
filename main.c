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
#include "gui.h"

uint8_t *memptr;
uint32_t *GPRptr;
union SCRs *SCRptr;

uint64_t ticks = 0;
int prevSS = 0;
int prevCont = 0;
int halt = 0;

int main (void) {
	gui_init();
	loginit("log.txt");
	//enlogtypes(LOGALL);
	memptr = meminit();
	rominit("bins/79X34xx.BIN");
	ioinit();
	SCRptr = getSCRptr();
	mmuinit(memptr, &SCRptr->ICS);
	GPRptr = procinit();

	romp_pointers(GPRptr, SCRptr, memptr);
	int close = 0;

	while(!close) {
		// Enable logging after certain address to save log file size...
		if (SCRptr->IAR == 0x008005A6) {
			enlogtypes(LOGALL);
		}
		if ((SDL_GetTicks64() - ticks) >= 16) {
			ticks = SDL_GetTicks64();
			close = gui_update();
		}
		if (prevSS != getSingleStep()) {
			prevSS = getSingleStep();
			fetch();
		}
		if (prevCont != getContinueBtn()) {
			prevCont = getContinueBtn();
			if (halt) {
				halt = 0;
				if (SCRptr->IAR == getBreakPoint()) {
					fetch();
				}
			} else {
				halt = 1;
			}
		}
		if (SCRptr->IAR == getBreakPoint()) {
			halt = 1;
		}
		if (!halt) {
			fetch();
		}
		//SDL_Delay(1000 / 60);
	}

	logend();
	gui_close();
	return 0;
}