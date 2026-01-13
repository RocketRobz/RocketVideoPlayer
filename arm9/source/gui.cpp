#include <nds.h>

#include "gl2d.h"
#include "graphics/lodepng.h"
#include "graphics/fontHandler.h"
#include "tonccpy.h"

#include "small_font.h"

#include "buttons.png_bin.h"
#include "large_buttons.png_bin.h"
#include "slider.h"
#include "userpal.h"

#include "rvidHeader.h"

extern bool videoPlaying;
extern int currentFrame;
extern int loadedFrames;

extern char filenameToDisplay[256];
extern bool filenameDisplayCentered;

extern char timeStamp[96];
#define timeStampCharCount 17

static int currentBarAdjust = 0;
static int barAdjust = 0;

extern u16* colorTable;
extern u16 whiteColor;
static u16 titleBarColor = RGB15(0/8, 176/8, 248/8);
static u16 titleBarEdgeColor = RGB15(160/8, 224/8, 248/8);
static u16 bottomGrayBarColor = RGB15(184/8, 184/8, 184/8);
static u16 playBarEdgeColor = RGB15(152/8, 152/8, 152/8);
static u16 playBarGrayPartColor = RGB15(96/8, 96/8, 96/8);
static u16 playBarSliderEdgeColor = RGB15(120/8, 120/8, 120/8);
static u16 playBarSliderShadeColor = RGB15(216/8, 216/8, 216/8);

static u16 playPauseButtons[112*(38*2)] = {0};
static u16 returnButton[158*30] = {0};

static u16* gfx[3+timeStampCharCount];

void loadGraphics(void) {
	extern bool useTwlCfg;
	extern u8 *twlCfgAddr;

	const int favoriteColor = (int)(useTwlCfg ? twlCfgAddr[0x44] : PersonalData->theme);

	titleBarColor = userPal[favoriteColor][0];
	titleBarEdgeColor = userPal[favoriteColor][1];

	if (colorTable) {
		titleBarColor = colorTable[titleBarColor];
		titleBarEdgeColor = colorTable[titleBarEdgeColor];
		bottomGrayBarColor = colorTable[bottomGrayBarColor];
		playBarEdgeColor = colorTable[playBarEdgeColor];
		playBarGrayPartColor = colorTable[playBarGrayPartColor];
		playBarSliderEdgeColor = colorTable[playBarSliderEdgeColor];
		playBarSliderShadeColor = colorTable[playBarSliderShadeColor];

		u16* palChange = (u16*)sliderPal;
		for (int i = 0; i < sliderPalLen/2; i++) {
			palChange[i] = colorTable[palChange[i]];
		}
	}

	extern u16 fileHighlightColor;
	fileHighlightColor = titleBarColor;

	std::vector<unsigned char> image;
	unsigned width, height;
	lodepng::decode(image, width, height, buttons_png_bin, buttons_png_bin_size);
	for(unsigned i=0;i<image.size()/4;i++) {
		const u16 color = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
		if (color != 0x8000) {
			playPauseButtons[i] = colorTable ? (colorTable[color % 0x8000] | BIT(15)) : color;
		}
	}

	image.clear();
	lodepng::decode(image, width, height, large_buttons_png_bin, large_buttons_png_bin_size);
	for(unsigned i=0;i<image.size()/4;i++) {
		const u16 color = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
		if (color != 0x8000) {
			returnButton[i] = colorTable ? (colorTable[color % 0x8000] | BIT(15)) : color;
		}
	}

	fontInit(favoriteColor);
}

bool updatePlayBar(void) {
	if (currentFrame >= rvidFrames) {
		barAdjust = 224;
	} else {
		barAdjust = (currentFrame * 224 * (0x800000 / rvidFrames) + 0x400000) >> 23;
	}
	if (currentBarAdjust == barAdjust) {
		return false;
	}

	if (barAdjust == 0) {
		for (int i = 142; i <= 145; i++) {
			toncset16(BG_GFX+(256*i)+16, playBarGrayPartColor | BIT(15), 224);
		}
	} else if (barAdjust == 224) {
		for (int i = 142; i <= 145; i++) {
			toncset16(BG_GFX+(256*i)+16, whiteColor | BIT(15), 224);
		}
	} else if (barAdjust > currentBarAdjust) {
		for (int i = 142; i <= 145; i++) {
			toncset16(BG_GFX+(256*i)+16+currentBarAdjust, whiteColor | BIT(15), barAdjust - currentBarAdjust);
		}
	} else {
		for (int i = 142; i <= 145; i++) {
			toncset16(BG_GFX+(256*i)+16+barAdjust, playBarGrayPartColor | BIT(15), currentBarAdjust - barAdjust);
		}
	}

	currentBarAdjust = barAdjust;
	return true;
}

void resetPlayBar(void) {
	barAdjust = 0;
}

void renderPlayPauseButton(void) {
	int x = 73, y = 76;
	const int iStart = videoPlaying ? 112*38 : 0;
	const int iEnd = videoPlaying ? 112*(38*2) : 112*38;
	for (int i = iStart; i < iEnd; i++) {
		if (playPauseButtons[i] != 0) {
			BG_GFX[(y * 256) + x] = playPauseButtons[i];
		}
		x++;
		if (x == 73+112) {
			x = 73;
			y++;
		}
	}
}

void renderGuiBg(void) {
	oamInit(&oamMain, SpriteMapping_1D_32, false);

	for (int i = 0; i < 3+timeStampCharCount; i++) {
		gfx[i] = oamAllocateGfx(&oamMain, SpriteSize_8x8, SpriteColorFormat_16Color);
	}
	oamSet(&oamMain, 0, 12, 134, 0, 0, SpriteSize_8x8, SpriteColorFormat_16Color, gfx[0], 0, false, false, false, false, false);
	oamSet(&oamMain, 1, 12, 134+8, 0, 0, SpriteSize_8x8, SpriteColorFormat_16Color, gfx[1], 0, false, false, false, false, false);
	oamSet(&oamMain, 2, 12, 134+16, 0, 0, SpriteSize_8x8, SpriteColorFormat_16Color, gfx[2], 0, false, false, false, false, false);
	{
		int x = 68;
		for (int i = 0; i < timeStampCharCount; i++) {
			oamSet(&oamMain, 3+i, x, 120, 0, 1, SpriteSize_8x8, SpriteColorFormat_16Color, gfx[3+i], 0, false, false, false, false, false);
			x += (((i % 3) == 2) && i != 8) ? 4 : 8;
		}
	}
	tonccpy(gfx[0], sliderTiles, sliderTilesLen);
	// tonccpy(gfx[1], sliderTiles+8, 4*8);
	// tonccpy(gfx[2], sliderTiles+16, 4*8);
	tonccpy(gfx[3+2], small_fontTiles+(11*8), 4*8); // ':' in timestamp
	tonccpy(gfx[3+5], small_fontTiles+(11*8), 4*8); // ':' in timestamp
	tonccpy(gfx[3+8], small_fontTiles, 4*8); // '/' in timestamp
	tonccpy(gfx[3+11], small_fontTiles+(11*8), 4*8); // ':' in timestamp
	tonccpy(gfx[3+14], small_fontTiles+(11*8), 4*8); // ':' in timestamp
	tonccpy(SPRITE_PALETTE, sliderPal, sliderPalLen);
	tonccpy(SPRITE_PALETTE+16, small_fontPal, small_fontPalLen);

	toncset16(BG_GFX+(256*60), whiteColor | BIT(15), 256*100); // BG
	toncset16(BG_GFX, titleBarColor | BIT(15), 256*60); // Title bar
	toncset16(BG_GFX+(256*58), titleBarEdgeColor | BIT(15), 256); // Title bar edge
	toncset16(BG_GFX+(256*160), bottomGrayBarColor | BIT(15), 256*32); // Bottom gray bar

	renderPlayPauseButton();

	{
		int x = 2, y = 162;
		for (int i = 0; i < 158*30; i++) {
			if (returnButton[i] != 0) {
				BG_GFX[(y * 256) + x] = returnButton[i];
			}
			x++;
			if (x == 2+158) {
				x = 2;
				y++;
			}
		}
	}

	// Play bar edges
	toncset16(BG_GFX+(256*140)+16, playBarEdgeColor | BIT(15), 224);
	toncset16(BG_GFX+(256*141)+15, playBarEdgeColor | BIT(15), 226);

	for (int i = 142; i <= 145; i++) {
		toncset16(BG_GFX+(256*i)+14, playBarEdgeColor | BIT(15), 2);
		toncset16(BG_GFX+(256*i)+16, playBarGrayPartColor | BIT(15), 224);
		toncset16(BG_GFX+(256*i)+240, playBarEdgeColor | BIT(15), 2);
	}

	toncset16(BG_GFX+(256*146)+15, playBarEdgeColor | BIT(15), 226);
	toncset16(BG_GFX+(256*147)+16, playBarEdgeColor | BIT(15), 224);

	if (filenameDisplayCentered) {
		printLargeCentered(20, filenameToDisplay);
	} else {
		printLarge(4, 20, filenameToDisplay);
	}
}

void renderGui(void) {
	// printSmallCentered(false, 120, timeStamp);
	// printSmall(false, 68, 120, timeStamp);
	for (int i = 0; i < timeStampCharCount; i++) {
		if ((i % 3) == 2) continue;
		tonccpy(gfx[3+i], small_fontTiles+((timeStamp[i]-0x2F)*8), 4*8);
	}

	oamSetXY(&oamMain, 0, 12+barAdjust, 134);
	oamSetXY(&oamMain, 1, 12+barAdjust, 134+8);
	oamSetXY(&oamMain, 2, 12+barAdjust, 134+16);
	oamUpdate(&oamMain);
}
