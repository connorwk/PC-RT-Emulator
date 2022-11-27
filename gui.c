// IBM PC RT Emulator GUI
#include "gui.h"

SDL_Window *window;
SDL_Renderer *rend;
SDL_Surface *surface;
TTF_Font *font;
SDL_Texture *fontTexture[CHARSINFONT];
SDL_Color textColor = {0, 255, 0, SDL_ALPHA_OPAQUE};
struct Text_Texture textlist[TEXTMAX];
struct Text_Texture textboxlist[TEXTMAX];
struct Text_Texture buttonlist[TEXTMAX];
int selectedbox = TEXTMAX+1;
uint64_t prevTicks = 0;
int blinkOn = 0;

uint32_t iarbreakptval;
uint32_t memaddrval;
int continuebtn = 0;
int singlestep = 0;

uint32_t *GPRlocptr;
union SCRs *SCRlocptr;
uint8_t *memlocptr;
uint8_t *mdalocptr;

uint16_t unicodeMappings[256] = {0x00a0, 0x0001, 0x0002, 0x0003, 0x0004, 0x0005, 0x0006, 0x0007, 0x0008, 0x0009, 0x000a, 0x000b, 0x000c, 0x000d, 0x000e, 0x000f, 0x0010, 0x0011, 0x0012, 0x0013, 0x0014, 0x0015, 0x0016, 0x0017, 0x0018, 0x0019, 0x001a, 0x001b, 0x001c, 0x001d, 0x001e, 0x001f, 0x0020, 0x0021, 0x0022, 0x0023, 0x0024, 0x0025, 0x0026, 0x0027, 0x0028, 0x0029, 0x002a, 0x002b, 0x002c, 0x002d, 0x002e, 0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036, 0x0037, 0x0038, 0x0039, 0x003a, 0x003b, 0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0041, 0x0042, 0x0043, 0x0044, 0x0045, 0x0046, 0x0047, 0x0048, 0x0049, 0x004a, 0x004b, 0x004c, 0x004d, 0x004e, 0x004f, 0x0050, 0x0051, 0x0052, 0x0053, 0x0054, 0x0055, 0x0056, 0x0057, 0x0058, 0x0059, 0x005a, 0x005b, 0x005c, 0x005d, 0x005e, 0x005f, 0x0060, 0x0061, 0x0062, 0x0063, 0x0064, 0x0065, 0x0066, 0x0067, 0x0068, 0x0069, 0x006a, 0x006b, 0x006c, 0x006d, 0x006e, 0x006f, 0x0070, 0x0071, 0x0072, 0x0073, 0x0074, 0x0075, 0x0076, 0x0077, 0x0078, 0x0079, 0x007a, 0x007b, 0x007c, 0x007d, 0x007e, 0x007f, 0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7, 0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5, 0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9, 0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192, 0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba, 0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb, 0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556, 0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510, 0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f, 0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567, 0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b, 0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580, 0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4, 0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229, 0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248, 0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0};

void gui_init (void) {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("Error initializing SDL: %s\n", SDL_GetError());
	}

	window = SDL_CreateWindow("IBM PC RT Emulator",
															SDL_WINDOWPOS_CENTERED,
															SDL_WINDOWPOS_CENTERED,
															800, 600, 0);
	rend = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawColor(rend, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(rend);

	if (TTF_Init() != 0){
		printf("Error initializing SDL TTF: %s\n", SDL_GetError());
	}
	font = TTF_OpenFont("fonts/Px437_IBM_MDA.ttf", CHARH);
	if (font == NULL) {
		printf("Error loading font file: %s\n", TTF_GetError());
	}
	SDL_Surface *char_surf;
	SDL_Color char_color = {0, 200, 0, 0};
	for (int i = 0; i < CHARSINFONT; i++) {
		char_surf = TTF_RenderGlyph_Solid(font, unicodeMappings[i], char_color);
		if (char_surf != NULL){
			fontTexture[i] = SDL_CreateTextureFromSurface(rend, char_surf);
			if (fontTexture[i] == NULL) {
				printf("Error creating texture from surface: %s\n", SDL_GetError());
			}
		} else {
			printf("Error rendering char surface: %s\n", TTF_GetError());
		}
		SDL_FreeSurface(char_surf);
	}
	
	for (int i=0; i<TEXTMAX; i++) {
		textlist[i].texture = NULL;
		textboxlist[i].texture = NULL;
		buttonlist[i].texture = NULL;
	}

	setup_text_textures();
}

void gui_close (void) {
	TTF_CloseFont(font);
	for (int i = 0; i < CHARSINFONT; i++) {
		SDL_DestroyTexture(fontTexture[i]);
	}
	SDL_DestroyRenderer(rend);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void generateTextTexture (struct Text_Texture *textureptr, const char *text, SDL_Color textColor, int x, int y, int textbox) {
	if (textureptr->texture != NULL) {SDL_DestroyTexture(textureptr->texture); textureptr->texture = NULL;}
	SDL_Surface* textSurface = TTF_RenderText_Solid( font, text, textColor );
	if (textSurface != NULL){
		textureptr->texture = SDL_CreateTextureFromSurface(rend, textSurface);
		if (textureptr->texture == NULL) {
			printf("Error creating texture from surface: %s\n", SDL_GetError());
		} else {
			strcpy(textureptr->text, text);
			if (textbox == TEXTBOX) {
				textureptr->rect.x = x+2;
				textureptr->rect.y = y;
				textureptr->rect.w = textSurface->w;
				textureptr->rect.h = textSurface->h;
				textureptr->sqrrect.x = x;
				textureptr->sqrrect.y = y;
				textureptr->sqrrect.w = textSurface->w+4;
				textureptr->sqrrect.h = textSurface->h;
			} else if (textbox == TEXT) {
				textureptr->rect.x = x;
				textureptr->rect.y = y;
				textureptr->rect.w = textSurface->w;
				textureptr->rect.h = textSurface->h;
			}
		}
	} else {
		printf("Error rendering char surface: %s\n", TTF_GetError());
	}
	SDL_FreeSurface(textSurface);
}

void updateTextTexture (struct Text_Texture *textureptr, char character) {
	for (int i=0; i<TEXTBOXMAX; i++) {
		if (textureptr->text[i] == ' ' && character != '\b') {
			textureptr->text[i] = character;
			break;
		} else if (textureptr->text[i] == ' ' && character == '\b' && i != 0) {
			textureptr->text[i-1] = ' ';
			break;
		} else if (textureptr->text[i] == '\0' && character == '\b') {
			textureptr->text[i-1] = ' ';
			break;
		} else if (textureptr->text[i] == '\0') {
			break;
		}
	}

	if (textureptr->texture != NULL) {SDL_DestroyTexture(textureptr->texture); textureptr->texture = NULL;}
	SDL_Surface* textSurface = TTF_RenderText_Solid( font, textureptr->text, textColor );
	if (textSurface != NULL){
		textureptr->texture = SDL_CreateTextureFromSurface(rend, textSurface);
		if (textureptr->texture == NULL) {
			printf("Error creating texture from surface: %s\n", SDL_GetError());
		}
	} else {
		printf("Error rendering char surface: %s\n", TTF_GetError());
	}
	SDL_FreeSurface(textSurface);
}

void render_all_text (struct Text_Texture *texturelistptr) {
	for (int i=0; i<TEXTMAX; i++) {
		if (texturelistptr[i].texture == NULL) {break;}
		SDL_RenderCopy(rend, texturelistptr[i].texture, NULL, &(texturelistptr[i].rect));
	}
}

void render_all_textboxes (struct Text_Texture *texturelistptr) {
	for (int i=0; i<TEXTMAX; i++) {
		if (texturelistptr[i].texture == NULL) {break;}
		if (blinkOn && selectedbox == i) {
			SDL_SetRenderDrawColor(rend, 100, 255, 100, SDL_ALPHA_OPAQUE);
		}
		SDL_RenderDrawRect(rend, &(texturelistptr[i].sqrrect));
		SDL_RenderCopy(rend, texturelistptr[i].texture, NULL, &(texturelistptr[i].rect));
		SDL_SetRenderDrawColor(rend, 255, 255, 255, SDL_ALPHA_OPAQUE);
	}
}

void render_all_buttons (struct Text_Texture *texturelistptr) {
	for (int i=0; i<TEXTMAX; i++) {
		if (texturelistptr[i].texture == NULL) {break;}
		SDL_RenderDrawRect(rend, &(texturelistptr[i].sqrrect));
		SDL_RenderCopy(rend, texturelistptr[i].texture, NULL, &(texturelistptr[i].rect));
	}
}

void setup_text_textures (void) {
	generateTextTexture(&textlist[0], "IAR:", textColor, CHARW*36, CHARH*25, TEXT);
	generateTextTexture(&textlist[1], "0x00000000", textColor, CHARW*41, CHARH*25, TEXT);
	generateTextTexture(&textlist[2], "IAR Breakpoint: 0x", textColor, CHARW*52, CHARH*25, TEXT);
	generateTextTexture(&textlist[3], "GPR 0:", textColor, 0, CHARH*25, TEXT);
	generateTextTexture(&textlist[4], "GPR 2:", textColor, 0, CHARH*26, TEXT);
	generateTextTexture(&textlist[5], "GPR 4:", textColor, 0, CHARH*27, TEXT);
	generateTextTexture(&textlist[6], "GPR 6:", textColor, 0, CHARH*28, TEXT);
	generateTextTexture(&textlist[7], "GPR 8:", textColor, 0, CHARH*29, TEXT);
	generateTextTexture(&textlist[8], "GPR10:", textColor, 0, CHARH*30, TEXT);
	generateTextTexture(&textlist[9], "GPR12:", textColor, 0, CHARH*31, TEXT);
	generateTextTexture(&textlist[10], "GPR14:", textColor, 0, CHARH*32, TEXT);
	generateTextTexture(&textlist[11], "GPR 1:", textColor, CHARW*18, CHARH*25, TEXT);
	generateTextTexture(&textlist[12], "GPR 3:", textColor, CHARW*18, CHARH*26, TEXT);
	generateTextTexture(&textlist[13], "GPR 5:", textColor, CHARW*18, CHARH*27, TEXT);
	generateTextTexture(&textlist[14], "GPR 7:", textColor, CHARW*18, CHARH*28, TEXT);
	generateTextTexture(&textlist[15], "GPR 9:", textColor, CHARW*18, CHARH*29, TEXT);
	generateTextTexture(&textlist[16], "GPR11:", textColor, CHARW*18, CHARH*30, TEXT);
	generateTextTexture(&textlist[17], "GPR13:", textColor, CHARW*18, CHARH*31, TEXT);
	generateTextTexture(&textlist[18], "GPR15:", textColor, CHARW*18, CHARH*32, TEXT);
	generateTextTexture(&textlist[19], "0x00000000", textColor, CHARW*7, CHARH*25, TEXT);
	generateTextTexture(&textlist[20], "0x00000000", textColor, CHARW*25, CHARH*25, TEXT);
	generateTextTexture(&textlist[21], "0x00000000", textColor, CHARW*7, CHARH*26, TEXT);
	generateTextTexture(&textlist[22], "0x00000000", textColor, CHARW*25, CHARH*26, TEXT);
	generateTextTexture(&textlist[23], "0x00000000", textColor, CHARW*7, CHARH*27, TEXT);
	generateTextTexture(&textlist[24], "0x00000000", textColor, CHARW*25, CHARH*27, TEXT);
	generateTextTexture(&textlist[25], "0x00000000", textColor, CHARW*7, CHARH*28, TEXT);
	generateTextTexture(&textlist[26], "0x00000000", textColor, CHARW*25, CHARH*28, TEXT);
	generateTextTexture(&textlist[27], "0x00000000", textColor, CHARW*7, CHARH*29, TEXT);
	generateTextTexture(&textlist[28], "0x00000000", textColor, CHARW*25, CHARH*29, TEXT);
	generateTextTexture(&textlist[29], "0x00000000", textColor, CHARW*7, CHARH*30, TEXT);
	generateTextTexture(&textlist[30], "0x00000000", textColor, CHARW*25, CHARH*30, TEXT);
	generateTextTexture(&textlist[31], "0x00000000", textColor, CHARW*7, CHARH*31, TEXT);
	generateTextTexture(&textlist[32], "0x00000000", textColor, CHARW*25, CHARH*31, TEXT);
	generateTextTexture(&textlist[33], "0x00000000", textColor, CHARW*7, CHARH*32, TEXT);
	generateTextTexture(&textlist[34], "0x00000000", textColor, CHARW*25, CHARH*32, TEXT);
	generateTextTexture(&textlist[35], "Mem Addr: 0x", textColor, CHARW*36, CHARH*26, TEXT);
	generateTextTexture(&textlist[36], "0x00000000", textColor, CHARW*36, CHARH*27, TEXT);
	generateTextTexture(&textlist[37], "0x00000000", textColor, CHARW*47, CHARH*27, TEXT);
	generateTextTexture(&textlist[38], "0x00000000", textColor, CHARW*58, CHARH*27, TEXT);
	generateTextTexture(&textlist[39], "0x00000000", textColor, CHARW*69, CHARH*27, TEXT);
	generateTextTexture(&textlist[40], "0x00000000", textColor, CHARW*36, CHARH*28, TEXT);
	generateTextTexture(&textlist[41], "0x00000000", textColor, CHARW*47, CHARH*28, TEXT);
	generateTextTexture(&textlist[42], "0x00000000", textColor, CHARW*58, CHARH*28, TEXT);
	generateTextTexture(&textlist[43], "0x00000000", textColor, CHARW*69, CHARH*28, TEXT);
	generateTextTexture(&textlist[44], "0x00000000", textColor, CHARW*36, CHARH*29, TEXT);
	generateTextTexture(&textlist[45], "0x00000000", textColor, CHARW*47, CHARH*29, TEXT);
	generateTextTexture(&textlist[46], "0x00000000", textColor, CHARW*58, CHARH*29, TEXT);
	generateTextTexture(&textlist[47], "0x00000000", textColor, CHARW*69, CHARH*29, TEXT);
	generateTextTexture(&textlist[48], "0x00000000", textColor, CHARW*36, CHARH*30, TEXT);
	generateTextTexture(&textlist[49], "0x00000000", textColor, CHARW*47, CHARH*30, TEXT);
	generateTextTexture(&textlist[50], "0x00000000", textColor, CHARW*58, CHARH*30, TEXT);
	generateTextTexture(&textlist[51], "0x00000000", textColor, CHARW*69, CHARH*30, TEXT);
	generateTextTexture(&textlist[52], "0x00000000", textColor, CHARW*36, CHARH*31, TEXT);
	generateTextTexture(&textlist[53], "0x00000000", textColor, CHARW*47, CHARH*31, TEXT);
	generateTextTexture(&textlist[54], "0x00000000", textColor, CHARW*58, CHARH*31, TEXT);
	generateTextTexture(&textlist[55], "0x00000000", textColor, CHARW*69, CHARH*31, TEXT);
	generateTextTexture(&textlist[56], "0x00000000", textColor, CHARW*36, CHARH*32, TEXT);
	generateTextTexture(&textlist[57], "0x00000000", textColor, CHARW*47, CHARH*32, TEXT);
	generateTextTexture(&textlist[58], "0x00000000", textColor, CHARW*58, CHARH*32, TEXT);
	generateTextTexture(&textlist[59], "0x00000000", textColor, CHARW*69, CHARH*32, TEXT);

	generateTextTexture(&textboxlist[0], "00800238", textColor, CHARW*70, CHARH*25, TEXTBOX);
	generateTextTexture(&textboxlist[1], "00000000", textColor, CHARW*48, CHARH*26, TEXTBOX);
	iarbreakptval = strtol(textboxlist[0].text, NULL, 16);
	memaddrval = strtol(textboxlist[1].text, NULL, 16);
	generateTextTexture(&buttonlist[0], "Cont/Halt", textColor, CHARW*70, CHARH*26, TEXTBOX);
	generateTextTexture(&buttonlist[1], "S.S.", textColor, CHARW*65, CHARH*26, TEXTBOX);
}

void render_interface () {
	render_all_text(textlist);
	render_all_textboxes(textboxlist);
	render_all_buttons(buttonlist);
	SDL_RenderDrawLine(rend, 0, CHARH*25, 800, CHARH*25);
	SDL_RenderDrawLine(rend, 175, CHARH*25, 175, 600);
	SDL_RenderDrawLine(rend, 355, CHARH*25, 355, 600);
	SDL_RenderDrawLine(rend, 515, CHARH*25, 515, CHARH*26);
	SDL_RenderDrawLine(rend, 355, CHARH*26, 800, CHARH*26);
	SDL_RenderDrawLine(rend, 355, CHARH*27, 800, CHARH*27);
}

void romp_pointers(uint32_t *GPRptr, union SCRs *SCRptr, uint8_t* memptr, uint8_t* mdaptr) {
	GPRlocptr = GPRptr;
	SCRlocptr = SCRptr;
	memlocptr = memptr;
	mdalocptr = mdaptr;
}

void render_GPRs(void) {
	char string[12];
	for (int i=0; i < 16; i++) {
		sprintf(string, "0x%08X", *(GPRlocptr + i));
		generateTextTexture(&textlist[19+i], string, textColor, 0, 0, UPDATETEXT);
	}
}

void render_Mem_Panel(void) {
	char string[12];
	for (int i=0; i < 24; i++) {
		if (memaddrval > 0x00FFFFE7) {memaddrval = 0x00FFFFE7;}
		sprintf(string, "0x%08X", memread(memlocptr, (memaddrval + (i*4)), WORD));
		generateTextTexture(&textlist[36+i], string, textColor, 0, 0, UPDATETEXT);
	}
}

void render_MDA(void) {
	SDL_Rect charRect;
	charRect.h = CHARH;
	charRect.w = CHARW;
	for (int i=0; i < 2000; i++) {
		charRect.x = (i % 80) * CHARW;
		charRect.y = (i / 80) * CHARH;
		SDL_RenderCopy(rend, fontTexture[mdalocptr[(i << 1) + 1]], NULL, &charRect);
	}
}

uint32_t getBreakPoint (void) {
	return iarbreakptval;
}

int getSingleStep (void) {
	return singlestep;
}

int getContinueBtn (void) {
	return continuebtn;
}

int gui_update (void) {
	SDL_SetRenderDrawColor(rend, 0, 0, 0, SDL_ALPHA_OPAQUE);
	SDL_RenderClear(rend);
	SDL_SetRenderDrawColor(rend, 255, 255, 255, SDL_ALPHA_OPAQUE);
	int x, y, buttons;
	int ret = 0;
	if ((SDL_GetTicks64() - prevTicks) >= 500) {
		prevTicks = SDL_GetTicks64();
		if (blinkOn) {blinkOn = 0;} else {blinkOn = 1;}
	}

	char string[12];
	sprintf(string, "0x%08X", SCRlocptr->IAR);
	generateTextTexture(&textlist[1], string, textColor, 0, 0, UPDATETEXT);

	render_GPRs();
	render_Mem_Panel();
	render_MDA();

	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
			case SDL_QUIT:
				ret = 1;
				break;
			case SDL_MOUSEBUTTONDOWN:
				buttons = SDL_GetMouseState(&x, &y);
				if (buttons & SDL_BUTTON_LMASK) {
					for (int i=0; i<TEXTMAX; i++) {
						if (textboxlist[i].texture != NULL) {
							if (x > textboxlist[i].rect.x && x < (textboxlist[i].rect.x + textboxlist[i].rect.w) && y > textboxlist[i].rect.y && y < (textboxlist[i].rect.y + textboxlist[i].rect.h)) {
								selectedbox = i;
								break;
							} else {
								selectedbox = TEXTMAX+1;
							}
						} else {
							break;
						}
					}
					
					for (int i=0; i<TEXTMAX; i++) {
						if (buttonlist[i].texture != NULL) {
							if (x > buttonlist[i].rect.x && x < (buttonlist[i].rect.x + buttonlist[i].rect.w) && y > buttonlist[i].rect.y && y < (buttonlist[i].rect.y + buttonlist[i].rect.h)) {
								switch (i) {
									case 0:
										continuebtn++;
										break;
									case 1:
										singlestep++;
										break;
									default:
										break;
								}
							}
						} else {
							break;
						}
					}
				}
				break;
			case SDL_KEYDOWN:
				if (selectedbox != TEXTMAX+1) {
					updateTextTexture(&textboxlist[selectedbox], event.key.keysym.sym);
					iarbreakptval = strtol(textboxlist[0].text, NULL, 16);
					memaddrval = strtol(textboxlist[1].text, NULL, 16);
				}
			break;
		}
	}

	render_interface();

	SDL_RenderPresent(rend);
	return ret;
}
/*
int main (void) {
	gui_init();

	int close = 0;
	while(!close) {
		close = gui_update();
		SDL_Delay(1000 / 60);
	}
	gui_close();
	return 0;
}
*/