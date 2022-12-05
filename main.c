// IBM PC RT Emulator
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "defs.h"
#include "logfac.h"
#include "romp.h"
#include "mmu.h"
#include "iocc.h"
#include "memory.h"
#include "gui.h"

uint8_t *memptr;
uint32_t *GPRptr;
union SCRs *SCRptr;
uint8_t *displayCode;

// PROC Bus
// A pointer to this is handed to each unit who needs it (romp.c, mmu.c, iocc.c)
struct procBusStruct procBus;

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
	ioinit(&procBus);
	SCRptr = getSCRptr();
	displayCode = mmuinit(memptr, &procBus);
	GPRptr = procinit(&procBus);

	romp_pointers(GPRptr, SCRptr, memptr, getMDAPtr(), displayCode);
	int close = 0;

	while(!close) {
		// Enable logging after certain address to save log file size... 0x008021B2: Before SysBoard IO regs tests 0x00806724: Before KBADPT init.
		if (SCRptr->IAR == 0x008021B2) {
			enlogtypes(LOGALL);
		}
		if ((SDL_GetTicks64() - ticks) >= 16) {
			ticks = SDL_GetTicks64();
			close = gui_update();
		}
		if (prevSS != getSingleStep()) {
			prevSS = getSingleStep();
			fetch();
			iocycle();
		}
		if (prevCont != getContinueBtn()) {
			prevCont = getContinueBtn();
			if (halt) {
				halt = 0;
				if (SCRptr->IAR == getBreakPoint()) {
					fetch();
					iocycle();
				}
			} else {
				halt = 1;
				printInstCounter();
				dumpMemory(memptr);
			}
		}
		if (SCRptr->IAR == getBreakPoint() && !halt) {
			halt = 1;
			printInstCounter();
			dumpMemory(memptr);
		}
		if (!halt) {
			fetch();
			iocycle();
		}
		//SDL_Delay(1000 / 60);
	}

	logend();
	gui_close();
	return 0;
}