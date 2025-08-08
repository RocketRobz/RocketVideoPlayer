/*-----------------------------------------------------------------
 Copyright (C) 2005 - 2013
	Michael "Chishm" Chisholm
	Dave "WinterMute" Murphy
	Claudio "sverx"

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

------------------------------------------------------------------*/
#include <nds.h>
#include <nds/arm9/dldi.h>
#include <stdio.h>
#include <fat.h>
#include <sys/stat.h>
#include <limits.h>

#include <string.h>
#include <unistd.h>

#include "file_browse.h"
#include "top_png_bin.h"
#include "gl2d.h"
#include "graphics/lodepng.h"
#include "gui.h"
#include "nitrofs.h"
#include "tonccpy.h"
#include "lz77.h"

#include "myDma.h"

#include "rvidHeader.h"

bool useTwlCfg     = false;
u32 *twlCfgPointer = (u32*)0x02FFFDFC;
u8 *twlCfgAddr     = NULL;

vu32* sharedAddr = (vu32*)0x02FFFD00;

u32 getFileSize(const char *fileName)
{
	FILE* fp = fopen(fileName, "rb");
	u32 fsize = 0;
	if (fp) {
		fseek(fp, 0, SEEK_END);
		fsize = ftell(fp);			// Get source file's size
		fseek(fp, 0, SEEK_SET);
	}
	fclose(fp);

	return fsize;
}

extern rvidHeaderCheckInfo rvidHeaderCheck;
// extern rvidHeaderInfo1 rvidHeader1;
extern rvidHeaderInfo2 rvidHeader2;

u8 compressedFrameBuffer[0xC000];
u16* compressedFrameSizes = NULL;

u16 palBuffer[60][256];
u8* frameBuffer = NULL;					// 32 frames in buffer (64 halved frames for interlaced videos)
int frameBufferCount = 32;
int topBg;
int bottomBg;
bool useBufferHalf = true;
u16 soundBuffer[2][32032];
u16* soundBufferPos = NULL;
u16 soundBufferReadLen = 0;
u16 soundBufferLen = 0;
int soundBufferDivide = 6;
bool useSoundBufferHalf = false;
bool updateSoundBuffer = false;

bool fadeType = false;

int screenBrightness = 31;

void clearBrightness(void) {
	fadeType = true;
	screenBrightness = 0;
}

u16* colorTable = NULL;
bool invertedColors = false;
bool noWhiteFade = false;

u16 blackColor = 0;
u16 whiteColor = 0xFFFF;

// Ported from PAlib (obsolete)
void SetBrightness(u8 screen, s8 bright) {
	if ((invertedColors && bright != 0) || (noWhiteFade && bright > 0)) {
		bright -= bright*2; // Invert brightness to match the inverted colors
	}

	u16 mode = 1 << 14;

	if (bright < 0) {
		mode = 2 << 14;
		bright = -bright;
	}
	if (bright > 31) bright = 31;
	*(vu16*)(0x0400006C + (0x1000 * screen)) = bright + mode;
}

static bool bottomBacklight = true;

void bottomBacklightSwitch(void) {
	if (bottomBacklight) {
		powerOff(PM_BACKLIGHT_BOTTOM);
	} else {
		powerOn(PM_BACKLIGHT_BOTTOM);
	}
	bottomBacklight = !bottomBacklight;
}

using namespace std;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

touchPosition touch;

FILE* rvid;
FILE* rvidSound;
bool showVideoGui = false;
bool updateVideoGuiFrame = true;
bool videoPlaying = false;
bool videoPausedPrior = false;
bool displayFrame = true;
int videoYpos = 0;
int frameOfRefreshRate = 0;
int frameOfRefreshRateLimit = 60;
int currentFrame = 0;
int currentFrameInBuffer = 0;
int loadedFrames = 0;
int frameDelay = 0;
bool frameDelayEven = true;
bool bottomField = false;

char filenameToDisplay[256];

char numberMark[6][16];

char timeStamp[96];

int hourMark = -1;
int minuteMark = 59;
int secondMark = 59;

int videoHourMark = -1;
int videoMinuteMark = 59;
int videoSecondMark = 59;

ITCM_CODE void fillBorders(void) {
	int scanline = REG_VCOUNT;
	if (scanline > videoYpos+rvidVRes) {
		return;
	} else {
		scanline++;
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE_SUB[0] = blackColor;
			BG_PALETTE[0] = blackColor;
		} else {
			BG_PALETTE_SUB[0] = SPRITE_PALETTE_SUB[0];
			BG_PALETTE[0] = SPRITE_PALETTE[0];
		}
	}
}

ITCM_CODE void fillBordersInterlaced(void) {
	int scanline = REG_VCOUNT;
	int check1 = (videoYpos*2);
	if (REG_BG3Y_SUB == -1) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	if (scanline > check1+check2) {
		return;
	} else {
		scanline++;
		if (scanline < check1 || scanline >= check1+check2) {
			BG_PALETTE_SUB[0] = blackColor;
			BG_PALETTE[0] = blackColor;
		} else {
			BG_PALETTE_SUB[0] = SPRITE_PALETTE_SUB[0];
			BG_PALETTE[0] = SPRITE_PALETTE[0];
		}
	}
}

void HBlankNull(void) {
}

ITCM_CODE void dmaFrameToScreen(void) {
	if (rvidDualScreen) {
		if (rvidInterlaced) {
			REG_BG3Y_SUB = bottomField ? -1 : 0;
			REG_BG3Y = bottomField ? -1 : 0;
		}
		const int currentFrameInBufferDoubled = currentFrameInBuffer*2;
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(0x100*rvidVRes)), bgGetGfxPtr(topBg)+((256/2)*videoYpos), 0x100*rvidVRes);
		dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(0x100*rvidVRes)), bgGetGfxPtr(bottomBg)+((256/2)*videoYpos), 0x100*rvidVRes);
		if (rvidVRes == (rvidInterlaced ? 96 : 192)) {
			dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBufferDoubled], BG_PALETTE_SUB, 256*2);
			dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBufferDoubled+1], BG_PALETTE, 256*2);
		} else {
			dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBufferDoubled]+1, BG_PALETTE_SUB+1, 255*2);
			dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBufferDoubled+1]+1, BG_PALETTE+1, 255*2);
			SPRITE_PALETTE_SUB[0] = palBuffer[currentFrameInBufferDoubled][0];
			SPRITE_PALETTE[0] = palBuffer[currentFrameInBufferDoubled+1][0];
		}
	} else {
		if (rvidInterlaced) {
			REG_BG3Y_SUB = bottomField ? -1 : 0;
		}
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBuffer*(0x100*rvidVRes)), bgGetGfxPtr(topBg)+((256/2)*videoYpos), 0x100*rvidVRes);
		if (rvidVRes == (rvidInterlaced ? 96 : 192)) {
			dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBuffer], BG_PALETTE_SUB, 256*2);
		} else {
			dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBuffer]+1, BG_PALETTE_SUB+1, 255*2);
			SPRITE_PALETTE_SUB[0] = palBuffer[currentFrameInBuffer][0];
		}
	}
}

ITCM_CODE void renderFrames(void) {
	if (fadeType) {
		screenBrightness--;
		if (screenBrightness < 0) screenBrightness = 0;
	} else {
		screenBrightness++;
		if (screenBrightness > 25) screenBrightness = 25;
	}
	SetBrightness(0, screenBrightness);
	SetBrightness(1, screenBrightness);

	if (videoPlaying && (currentFrame <= loadedFrames) && !displayFrame) {
		if (rvidHasSound) {
			if (!updateSoundBuffer && ((frameOfRefreshRate % (frameOfRefreshRateLimit/soundBufferDivide)) == 0)) {
				soundBufferPos += soundBufferReadLen/soundBufferDivide;
				soundBufferLen -= soundBufferReadLen/soundBufferDivide;
				if (videoPausedPrior) {
					sharedAddr[0] = (u32)soundBufferPos;
					sharedAddr[1] = (soundBufferLen*sizeof(u16)) >> 2;
					sharedAddr[2] = rvidSampleRate;
					IPC_SendSync(3);
					videoPausedPrior = false;
				}
			}
			if ((frameOfRefreshRate % frameOfRefreshRateLimit) == 0) {
				sharedAddr[0] = (u32)&soundBuffer[useSoundBufferHalf];
				sharedAddr[1] = (soundBufferReadLen*sizeof(u16)) >> 2;
				sharedAddr[2] = rvidSampleRate;
				IPC_SendSync(3);

				soundBufferPos = (u16*)&soundBuffer[useSoundBufferHalf];
				soundBufferLen = rvidSampleRate;
				updateSoundBuffer = true;
			}
		}

		frameOfRefreshRate++;
		if (frameOfRefreshRate == frameOfRefreshRateLimit) frameOfRefreshRate = 0;

		frameDelay++;
		switch (rvidFps) {
			default:
				displayFrame = (frameDelay == frameOfRefreshRateLimit/rvidFps);
				break;
			case 6:
				displayFrame = (frameDelay == 4+frameDelayEven);
				break;
			case 11:
				displayFrame = (frameDelay == 5+frameDelayEven);
				break;
			case 12:
				displayFrame = (frameDelay == 3+frameDelayEven);
				break;
			case 24:
			// case 25:
				displayFrame = (frameDelay == 2+frameDelayEven);
				break;
			case 48:
				displayFrame = (frameDelay == 1+frameDelayEven);
				break;
		}
	}
	if (videoPlaying && (currentFrame <= loadedFrames) && displayFrame) {
		if (currentFrame < rvidFrames) {
			dmaFrameToScreen();
		}
		if (!rvidDualScreen && ((currentFrame % rvidFps) == 0)) {
			secondMark++;
			if (secondMark == 60) {
				secondMark = 0;
				minuteMark++;
				if (minuteMark == 60) {
					minuteMark = 0;
					hourMark++;
				}
			}

			// Current time stamp
			if (hourMark < 10) {
				sprintf(numberMark[0], "0%i", hourMark);
			} else {
				sprintf(numberMark[0], "%i", hourMark);
			}
			if (minuteMark < 10) {
				sprintf(numberMark[1], "0%i", minuteMark);
			} else {
				sprintf(numberMark[1], "%i", minuteMark);
			}
			if (secondMark < 10) {
				sprintf(numberMark[2], "0%i", secondMark);
			} else {
				sprintf(numberMark[2], "%i", secondMark);
			}

			sprintf(timeStamp, "%s:%s:%s/%s:%s:%s",
			numberMark[0], numberMark[1], numberMark[2], numberMark[3], numberMark[4], numberMark[5]);
			updateVideoGuiFrame = true;
		}

		if (rvidInterlaced) {
			bottomField = !bottomField;
		}
		currentFrame++;
		currentFrameInBuffer++;
		if (currentFrameInBuffer == frameBufferCount) {
			currentFrameInBuffer = 0;
		}
		if (!rvidDualScreen) {
			if (updateVideoGuiFrame) {
				updatePlayBar();
			} else {
				updateVideoGuiFrame = updatePlayBar();
			}
		}
		switch (rvidFps) {
			case 6:
			case 12:
			case 24:
			case 48:
				frameDelayEven = !frameDelayEven;
				break;
			case 11:
				if ((currentFrame % 11) < 10) {
					frameDelayEven = !frameDelayEven;
				}
				break;
			/* case 25:
				if ((currentFrame % 24) != 10 && (currentFrame % 24) != 21) {
					frameDelayEven = !frameDelayEven;
				}
				break; */
		}
		frameDelay = 0;
		displayFrame = false;
	}

	if (showVideoGui && updateVideoGuiFrame) {
		if (!rvidDualScreen) {
			renderGui();
		}
		updateVideoGuiFrame = false;
	}
}

bool confirmReturn = false;
bool confirmStop = false;
int videoJump = 0;
bool doubleJump = false;

void sndUpdateStream(void) {
	if (!updateSoundBuffer) {
		return;
	}
	useSoundBufferHalf = !useSoundBufferHalf;
	toncset(soundBuffer[useSoundBufferHalf], 0, soundBufferReadLen*sizeof(u16));
	fread(soundBuffer[useSoundBufferHalf], sizeof(u16), soundBufferReadLen, rvidSound);
	updateSoundBuffer = false;
}

static inline void loadFramePal(const int num) {
	fread(palBuffer[num], 2, 256, rvid);
	if (colorTable) {
		for (int i = 0; i < 256; i++) {
			palBuffer[num][i] = colorTable[palBuffer[num][i]];
		}
	}
}

void loadFrame(const int num) {
	if (loadedFrames >= rvidFrames) {
		return;
	}

	if (rvidDualScreen) {
		if (rvidCompressed) {
			for (int b = 0; b < 2; b++) {
				const int pos = (num*2)+b;
				const int loadedFramesPos = (loadedFrames*2)+b;
				loadFramePal(pos);
				if (compressedFrameSizes[loadedFramesPos] == 0x100*rvidVRes) {
					fread(frameBuffer+(pos*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
				} else {
					fread(compressedFrameBuffer, 1, compressedFrameSizes[loadedFramesPos], rvid);
					sndUpdateStream();
					lzssDecompress(compressedFrameBuffer, frameBuffer+(pos*(0x100*rvidVRes)));
					DC_FlushRange(frameBuffer+(pos*(0x100*rvidVRes)), 0x100*rvidVRes);
				}
				sndUpdateStream();
			}
		} else {
			for (int b = 0; b < 2; b++) {
				const int pos = (num*2)+b;
				loadFramePal(pos);
				fread(frameBuffer+(pos*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
				sndUpdateStream();
			}
		}
	} else {
		loadFramePal(num);
		if (rvidCompressed) {
			if (compressedFrameSizes[loadedFrames] == 0x100*rvidVRes) {
				fread(frameBuffer+(num*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
			} else {
				fread(compressedFrameBuffer, 1, compressedFrameSizes[loadedFrames], rvid);
				sndUpdateStream();
				lzssDecompress(compressedFrameBuffer, frameBuffer+(num*(0x100*rvidVRes)));
				DC_FlushRange(frameBuffer+(num*(0x100*rvidVRes)), 0x100*rvidVRes);
			}
		} else {
			fread(frameBuffer+(num*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
		}
		sndUpdateStream();
	}
	loadedFrames++;
}

bool playerControls(void) {
	sndUpdateStream();

	scanKeys();
	touchRead(&touch);
	const int pressed = keysDown();
	const int held = keysHeld();
	if ((pressed & KEY_A) || ((pressed & KEY_LID) && videoPlaying)
	|| (!rvidDualScreen && (pressed & KEY_TOUCH) && touch.px >= 73 && touch.px <= 184 && touch.py >= 76 && touch.py <= 113)) {
		if (videoPlaying) {
			soundKill(0);
			soundKill(1);
			videoPlaying = false;
			updateVideoGuiFrame = true;
		} else {
			videoPlaying = true;
			videoPausedPrior = true;
			updateVideoGuiFrame = true;
		}
	}
	if ((pressed & KEY_B)
	|| (!rvidDualScreen && (pressed & KEY_TOUCH) && touch.px >= 2 && touch.px <= 159 && touch.py >= 162 && touch.py <= 191)) {
		confirmReturn = true;
		return true;
	}
	doubleJump = (held & KEY_R);
	if ((held & KEY_LEFT) && (held & KEY_DOWN)) {
		videoJump = -3;
		return true;
	} else if ((held & KEY_UP) && (held & KEY_RIGHT)) {
		videoJump = 3;
		return true;
	}
	if (held & KEY_LEFT) {
		videoJump = -1;
		return true;
	} else if (held & KEY_RIGHT) {
		videoJump = 1;
		return true;
	} else if (held & KEY_DOWN) {
		videoJump = -2;
		return true;
	} else if (held & KEY_UP) {
		videoJump = 2;
		return true;
	}
	if (((pressed & KEY_L)
	|| (!rvidDualScreen && (pressed & KEY_TOUCH) && touch.px >= 14 && touch.px <= 19 && touch.py >= 140 && touch.py <= 147)) && currentFrame > 0) {
		confirmStop = true;
		return true;
	}
	if (!rvidDualScreen && (pressed & KEY_SELECT)) {
		bottomBacklightSwitch();
	}

	return false;
}

int playRvid(const char* filename) {
	confirmReturn = false;
	confirmStop = false;

	if (rvidHeaderCheck.ver == 0) {
		return 0;
	} else if (rvidHeaderCheck.ver > 3) {
		return 3;
	} else if (rvidHeaderCheck.ver < 3) {
		return 4;
	}

	readRvidHeader(rvid);

	videoYpos = 0;

	if (rvidInterlaced) {
		frameBufferCount = 64;
		if (rvidVRes <= 190/2) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 192/2; i += 2) {
				videoYpos++;
			}
		}
	} else {
		frameBufferCount = 32;
		if (rvidVRes <= 190) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 192; i += 2) {
				videoYpos++;
			}
		}
	}
	if (rvidDualScreen) {
		frameBufferCount /= 2;
	}

	sprintf(filenameToDisplay, filename);
	for (int i = strlen(filename); i >= 0; i--) {
		if (filenameToDisplay[i] == '.') {
			filenameToDisplay[i] = 0; // Remove ".rvid" from title
			break;
		}
	}

	videoHourMark = -1;
	videoMinuteMark = 59;
	videoSecondMark = 59;

	// Get full time stamp
	for (int i = 0; i <= (int)rvidFrames; i += rvidFps) {
		videoSecondMark++;
		if (videoSecondMark == 60) {
			videoSecondMark = 0;
			videoMinuteMark++;
			if (videoMinuteMark == 60) {
				videoMinuteMark = 0;
				videoHourMark++;
			}
		}
	}

	// Full time stamp
	if (videoHourMark < 10) {
		sprintf(numberMark[3], "0%i", videoHourMark);
	} else {
		sprintf(numberMark[3], "%i", videoHourMark);
	}
	//printf(":");
	if (videoMinuteMark < 10) {
		sprintf(numberMark[4], "0%i", videoMinuteMark);
	} else {
		sprintf(numberMark[4], "%i", videoMinuteMark);
	}
	//printf(":");
	if (videoSecondMark < 10) {
		sprintf(numberMark[5], "0%i", videoSecondMark);
	} else {
		sprintf(numberMark[5], "%i", videoSecondMark);
	}

	sprintf(timeStamp, "00:00:00/%s:%s:%s",
	numberMark[3], numberMark[4], numberMark[5]);
	updateVideoGuiFrame = true;
	updatePlayBar();

	frameBuffer = new u8[0xC000*32];

	if (rvidCompressed) {
		const u32 tableSize = (rvidFramesOffset-0x200);
		compressedFrameSizes = new u16[tableSize/2];
		fseek(rvid, 0x200, SEEK_SET);
		fread(compressedFrameSizes, 2, tableSize/2, rvid);
	} else {
		fseek(rvid, rvidFramesOffset, SEEK_SET);
	}
	loadedFrames = 0;
	for (int i = 0; i < frameBufferCount/2; i++) {
		loadFrame(i);
	}

	if (rvidHasSound) {
		if (rvidSampleRate > 32000) {
			return 1;
		}
		// Ensure video and audio stay in sync
		soundBufferReadLen = rvidSampleRate;
		for (int i = 0; i < rvidSampleRate; i += 1000) {
			soundBufferReadLen++;
		}

		rvidSound = fopen(filename, "rb");
		fseek(rvidSound, rvidSoundOffset, SEEK_SET);
		soundBufferDivide = (rvidFps == 25 || rvidFps == 50) ? 5 : 6;
		toncset(soundBuffer[0], 0, soundBufferReadLen*sizeof(u16));
		fread(soundBuffer[0], sizeof(u16), soundBufferReadLen, rvidSound);
	}

	if (fadeType) {
		fadeType = false;
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
		consoleClear();
	}

	dmaFillHalfWords(whiteColor, BG_PALETTE_SUB, 256*2);	// Fill top screen with white
	topBg = bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	if (rvidDualScreen) {
		dmaFillHalfWords(whiteColor, BG_PALETTE, 256*2);	// Fill bottom screen with white
		bottomBg = bgInit(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);
	}

	if (rvidInterlaced) {
		REG_BG3PD_SUB = 0x80;
		if (rvidDualScreen) {
			REG_BG3PD = 0x80;
		}
	}

	for (int i = 0; i < 256*192; i++) {
		compressedFrameBuffer[i] = 0;
	}

	dmaCopyWords(3, compressedFrameBuffer, bgGetGfxPtr(topBg), 256*192);
	if (rvidDualScreen) {
		dmaCopyWords(3, compressedFrameBuffer, bgGetGfxPtr(bottomBg), 256*192);
	}

	videoSetMode(rvidDualScreen ? (MODE_5_2D | DISPLAY_BG3_ACTIVE) : MODE_5_3D);
	showVideoGui = true;
	updateVideoGuiFrame = true;

	fadeType = true;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	irqSet(IRQ_HBLANK, (rvidVRes == (rvidInterlaced ? 96 : 192)) ? HBlankNull : rvidInterlaced ? fillBordersInterlaced : fillBorders);
	irqEnable(IRQ_HBLANK);

	// Enable frame rate adjustment
	if (rvidFps == 25 || rvidFps == 50) {
		frameOfRefreshRateLimit = 50;
		IPC_SendSync(1);
	} else {
		frameOfRefreshRateLimit = 60;
		IPC_SendSync(2);
	}

	/* if (rvidVRes < 192) {
		dmaFillHalfWordsAsynch(3, 0, BG_GFX_SUB, 0x18000);	// Fill top screen with black
	} */
	videoPlaying = true;
	while (1) {
		if (currentFrameInBuffer >= 0
		 && currentFrameInBuffer < frameBufferCount/2)
		{
			if (useBufferHalf) {
				for (int i = frameBufferCount/2; i < frameBufferCount; i++) {
					loadFrame(i);
					if (playerControls()) {
						break;
					}
				}
				useBufferHalf = false;
			}
		} else if (currentFrameInBuffer >= frameBufferCount/2
				&& currentFrameInBuffer < frameBufferCount)
		{
			if (!useBufferHalf) {
				for (int i = 0; i < frameBufferCount/2; i++) {
					loadFrame(i);
					if (playerControls()) {
						break;
					}
				}
				useBufferHalf = true;
			}
		}
		playerControls();
		if (currentFrame > (int)rvidFrames) {
			confirmStop = true;
		}
		if (confirmStop || videoJump != 0) {
			videoPlaying = false;
			swiWaitForVBlank();

			useBufferHalf = true;
			useSoundBufferHalf = false;
			updateSoundBuffer = false;
			videoPausedPrior = false;
			displayFrame = true;
			frameOfRefreshRate = 0;
			const int currentFrameBak = currentFrame;
			if (videoJump == -1) { // Left
				currentFrame /= rvidFps;
				currentFrame -= doubleJump ? 30 : 5;
				currentFrame *= rvidFps;
			} else if (videoJump == 1) { // Right
				currentFrame /= rvidFps;
				currentFrame += doubleJump ? 30 : 5;
				currentFrame *= rvidFps;
			} else if (videoJump == -2) { // Down
				currentFrame /= rvidFps;
				currentFrame -= doubleJump ? 60 : 10;
				currentFrame *= rvidFps;
			} else if (videoJump == 2) { // Up
				currentFrame /= rvidFps;
				currentFrame += doubleJump ? 60 : 10;
				currentFrame *= rvidFps;
			} else if (videoJump == -3) { // Left+Down
				currentFrame /= rvidFps;
				currentFrame -= doubleJump ? 120 : 15;
				currentFrame *= rvidFps;
			} else if (videoJump == 3) { // Up+Right
				currentFrame /= rvidFps;
				currentFrame += doubleJump ? 120 : 15;
				currentFrame *= rvidFps;
			}
			if (confirmStop || currentFrame < 0) {
				currentFrame = 0;
			} else if (currentFrame > (int)rvidFrames) {
				currentFrame = currentFrameBak;
			}
			currentFrameInBuffer = 0;
			frameDelay = 0;
			frameDelayEven = true;
			bottomField = false;

			if (rvidHasSound) {
				soundKill(0);
				soundKill(1);
			}

			hourMark = -1;
			minuteMark = 59;
			secondMark = 59;

			if (currentFrame == 0) {
				sprintf(timeStamp, "00:00:00/%s:%s:%s",
				numberMark[3], numberMark[4], numberMark[5]);
				updateVideoGuiFrame = true;
				updatePlayBar();
			} else {
				for (int i = 0; i < currentFrame/rvidFps; i++) {
					secondMark++;
					if (secondMark == 60) {
						secondMark = 0;
						minuteMark++;
						if (minuteMark == 60) {
							minuteMark = 0;
							hourMark++;
						}
					}
				}

				int hourMarkDisplay = hourMark;
				int minuteMarkDisplay = minuteMark;
				int secondMarkDisplay = secondMark;

				secondMarkDisplay++;
				if (secondMarkDisplay == 60) {
					secondMarkDisplay = 0;
					minuteMarkDisplay++;
					if (minuteMarkDisplay == 60) {
						minuteMarkDisplay = 0;
						hourMarkDisplay++;
					}
				}

				// Current time stamp
				if (hourMarkDisplay < 10) {
					sprintf(numberMark[0], "0%i", hourMarkDisplay);
				} else {
					sprintf(numberMark[0], "%i", hourMarkDisplay);
				}
				if (minuteMarkDisplay < 10) {
					sprintf(numberMark[1], "0%i", minuteMarkDisplay);
				} else {
					sprintf(numberMark[1], "%i", minuteMarkDisplay);
				}
				if (secondMarkDisplay < 10) {
					sprintf(numberMark[2], "0%i", secondMarkDisplay);
				} else {
					sprintf(numberMark[2], "%i", secondMarkDisplay);
				}

				sprintf(timeStamp, "%s:%s:%s/%s:%s:%s",
				numberMark[0], numberMark[1], numberMark[2], numberMark[3], numberMark[4], numberMark[5]);
				updateVideoGuiFrame = true;

				if (updateVideoGuiFrame) {
					updatePlayBar();
				} else {
					updateVideoGuiFrame = updatePlayBar();
				}
			}

			// Reload video
			u32 rvidSeekOffset = rvidFramesOffset;
			if (currentFrame > 0) {
				if (rvidCompressed) {
					if (rvidDualScreen) {
						for (int i = 0; i < currentFrame; i++) {
							for (int b = 0; b < 2; b++) {
								rvidSeekOffset += 0x200;
								rvidSeekOffset += compressedFrameSizes[(i*2)+b];
							}
						}
					} else {
						for (int i = 0; i < currentFrame; i++) {
							rvidSeekOffset += 0x200;
							rvidSeekOffset += compressedFrameSizes[i];
						}
					}
				} else {
					rvidSeekOffset += (0x200+(0x100*rvidVRes))*currentFrame;
					if (rvidDualScreen) {
						rvidSeekOffset += (0x200+(0x100*rvidVRes))*currentFrame;
					}
				}
			}
			fseek(rvid, rvidSeekOffset, SEEK_SET);
			loadedFrames = currentFrame;
			for (int i = 0; i < frameBufferCount/2; i++) {
				loadFrame(i);
			}

			if (rvidHasSound) {
				fseek(rvidSound, rvidSoundOffset+((soundBufferReadLen*sizeof(u16))*(currentFrame/rvidFps)), SEEK_SET);
				toncset(soundBuffer[0], 0, soundBufferReadLen*sizeof(u16));
				fread(soundBuffer[0], sizeof(u16), soundBufferReadLen, rvidSound);
			}

			if (videoJump != 0) {
				swiWaitForVBlank();
				dmaFrameToScreen();
			}

			confirmStop = false;
			videoJump = 0;
		}
		if (confirmReturn) {
			break;
		}
		swiWaitForVBlank();
	}

	videoPlaying = false;
	swiWaitForVBlank();
	IPC_SendSync(0); // Disable frame rate adjustment

	if (rvidHasSound) {
		soundKill(0);
		soundKill(1);
	}

	if (!bottomBacklight) {
		bottomBacklightSwitch();
	}

	fadeType = false;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	// while (dmaBusy(3));
	irqSet(IRQ_HBLANK, HBlankNull);
	if (rvidCompressed) {
		delete[] compressedFrameSizes;
	}
	delete[] frameBuffer;
	bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

	showVideoGui = false;
	useBufferHalf = true;
	useSoundBufferHalf = false;
	updateSoundBuffer = false;
	videoPausedPrior = false;
	displayFrame = true;
	frameOfRefreshRate = 0;
	currentFrame = 0;
	currentFrameInBuffer = 0;
	frameDelay = 0;
	frameDelayEven = true;
	bottomField = false;

	hourMark = -1;
	minuteMark = 59;
	secondMark = 59;

	return 0;
}

void LoadBMP(void) {
	std::vector<unsigned char> image;
	unsigned width, height;
	lodepng::decode(image, width, height, top_png_bin, top_png_bin_size);
	for(unsigned i=0;i<image.size()/4;i++) {
		BG_GFX_SUB[i] = image[i*4]>>3 | (image[(i*4)+1]>>3)<<5 | (image[(i*4)+2]>>3)<<10 | BIT(15);
	}
	if (colorTable) {
		for(unsigned i=0;i<image.size()/4;i++) {
			BG_GFX_SUB[i] = colorTable[BG_GFX_SUB[i] % 0x8000] | BIT(15);
		}
	}
}

static std::string filename;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	if (!fatInitDefault()) {
		consoleDemoInit();
		iprintf ("fatinitDefault failed!\n");
		stop();
	}

	char currentSettingPath[40];
	sprintf(currentSettingPath, "/_nds/colorLut/currentSetting.txt");

	if (access(currentSettingPath, F_OK) == 0) {
		// Load color LUT
		char lutName[128] = {0};
		FILE* file = fopen(currentSettingPath, "rb");
		fread(lutName, 1, 128, file);
		fclose(file);

		char colorTablePath[256];
		sprintf(colorTablePath, "/_nds/colorLut/%s.lut", lutName);

		u32 colorTableSize = getFileSize(colorTablePath);
		if (colorTableSize > 0x10000 && colorTableSize < 0x20000) {
			colorTableSize = 0x10000;
		}
		if (colorTableSize == 0x10000 || colorTableSize == 0x20000) {
			colorTable = new u16[colorTableSize/sizeof(u16)];

			FILE* file = fopen(colorTablePath, "rb");
			fread(colorTable, 1, colorTableSize, file);
			fclose(file);

			const u16 color0 = colorTable[0] | BIT(15);
			const u16 color7FFF = colorTable[0x7FFF] | BIT(15);

			invertedColors =
			  (color0 >= 0xF000 && color0 <= 0xFFFF
			&& color7FFF >= 0x8000 && color7FFF <= 0x8FFF);
			if (!invertedColors) noWhiteFade = (color7FFF < 0xF000);

			vramSetBankD(VRAM_D_LCD);
			tonccpy(VRAM_D, colorTable, colorTableSize); // Copy LUT to VRAM
			if (colorTableSize == 0x10000) {
				tonccpy(VRAM_D+(0x10000/2), colorTable, colorTableSize); // Copy LUT to VRAM
			}
			delete[] colorTable; // Free up RAM space
			colorTable = VRAM_D;

			blackColor = colorTable[0];
			whiteColor = colorTable[0xFFFF];
		}
	}

	SetBrightness(0, 31);
	SetBrightness(1, 31);

	// nitroFSInit();

	if (isDSiMode()) {
		if(*twlCfgPointer < 0x02000000 || *twlCfgPointer >= 0x03000000) {
			*twlCfgPointer = 0x02000400;
		}
		twlCfgAddr = (u8*)*twlCfgPointer;
		useTwlCfg  = ((twlCfgAddr[0] != 0) && (twlCfgAddr[1] == 0) && (twlCfgAddr[2] == 0) && (twlCfgAddr[4] == 0) && (twlCfgAddr[0x48] != 0));

		sharedAddr = (vu32*)0x0CFFFD00;
	}

	irqSet(IRQ_VBLANK, renderFrames);
	irqEnable(IRQ_VBLANK);

	videoSetMode(MODE_5_2D | DISPLAY_BG3_ACTIVE);
	videoSetModeSub(MODE_3_2D | DISPLAY_BG3_ACTIVE);

	// Initialize gl2d
	glScreen2D();
	// Make gl2d render on transparent stage.
	glClearColor(31,31,31,0);
	glDisable(GL_CLEAR_BMP);

	// Clear the GL texture state
	glResetTextures();

	// sprites
	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_SUB_BG);
	vramSetBankE(VRAM_E_TEX_PALETTE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT4);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5); // 16Kb of palette ram, and font textures take up 8*16 bytes.

	bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

	loadGraphics();

	lcdMainOnBottom();

	keysSetRepeat(25,5);

	vector<string> extensionList;
	extensionList.push_back(".rvid");

	if (argc < 2) {
		chdir("/videos");
	}

	while(1) {
		if (argc >= 2) {
			std::string vidFolder = argv[1];
			while (!vidFolder.empty() && vidFolder[vidFolder.size()-1] != '/') {
				vidFolder.resize(vidFolder.size()-1);
			}
			chdir(vidFolder.c_str());

			filename = argv[1];
			const size_t last_slash_idx = filename.find_last_of("/");
			if (std::string::npos != last_slash_idx)
			{
				filename.erase(0, last_slash_idx + 1);
			}
		} else {
			videoSetMode(MODE_0_2D);
			vramSetBankG(VRAM_G_MAIN_BG);
			consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);
			if (colorTable) {
				for (int i = 0; i < 256; i++) {
					BG_PALETTE[i] = colorTable[BG_PALETTE[i]];
				}
			}
			LoadBMP();
			fadeType = true;

			filename = browseForFile(extensionList);
		}

		if ( strcasecmp (filename.c_str() + filename.size() - 5, ".rvid") != 0 ) {
			iprintf("No .rvid file specified.\n");
			if (argc < 2) {
				for (int i = 0; i < 60*2; i++) {
					swiWaitForVBlank();
				}
			}
		} else {
			printf("Loading...");
			rvid = fopen(filename.c_str(), "rb");
			if (rvid) {
				fread(&rvidHeaderCheck, 1, sizeof(rvidHeaderCheck), rvid);
				if (rvidHeaderCheck.formatString != 0x44495652) {
					consoleClear();
					printf("Not a Rocket Video file!");
					fclose(rvid);
					if (argc < 2) {
						for (int i = 0; i < 60*2; i++) {
							swiWaitForVBlank();
						}
					}
				} else {
					int err = playRvid(filename.c_str());
					fclose(rvid);
					fclose(rvidSound);
					if (err == 4) {
						consoleClear();
						printf("This Rocket Video file\n");
						printf("contains a version which is\n");
						printf("no longer supported.\n");
					} else if (err == 3) {
						consoleClear();
						printf("This Rocket Video file\n");
						printf("contains a version higher than\n");
						printf("the player supports.\n");
						printf("\n");
						printf("Please update the player to\n");
						printf("the latest version.\n");
					} else if (err == 1) {
						consoleClear();
						printf("Audio sample rate is higher\n");
						printf("than 32000Hz.\n");
						printf("\n");
						printf("Please lower the sample rate\n");
						printf("to 32000Hz or less.\n");
					}
					if ((err > 0) && (argc < 2)) {
						printf("\n");
						printf("A: OK\n");
						while (1) {
							scanKeys();
							if (keysDown() & KEY_A) {
								break;
							}
						}
						for (int i = 0; i < 25; i++) {
							swiWaitForVBlank();
						}
					}
				}
			}
		}

		if (argc >= 2) {
			break;
		}

	}

	return 0;
}
