// IBM PC RT Emulator GUI
#ifndef _GUI
#define _GUI
#include <SDL.h>
#include <SDL_ttf.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "romp.h"
#include "mmu.h"
#include "memory.h"

#define CHARSINFONT 256
#define CHARH 18
#define CHARW 10
#define TEXTMAX 64
#define TEXTBOXMAX 256

#define TEXT 0
#define TEXTBOX 1
#define UPDATETEXT 2

struct Text_Texture {
	SDL_Texture	*texture;
	SDL_Rect rect;
	SDL_Rect sqrrect;
	char text[TEXTBOXMAX];
};

void gui_init (void);
void gui_close (void);
void generateTextTexture (struct Text_Texture *textureptr, const char *text, SDL_Color textColor, int x, int y, int textbox);
void updateTextTexture (struct Text_Texture *textureptr, char character);
void render_all_text (struct Text_Texture *texturelistptr);
void render_all_textboxes (struct Text_Texture *texturelistptr);
void render_all_buttons (struct Text_Texture *texturelistptr);
void setup_text_textures (void);
void romp_pointers(uint32_t *GPRptr, union SCRs *SCRptr, uint8_t* memptr);
void render_GPRs(void);
uint32_t getBreakPoint (void);
int getSingleStep (void);
int getContinueBtn (void);
int gui_update (void);

#endif