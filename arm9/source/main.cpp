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

extern rvidHeaderCheckInfo rvidHeaderCheck;
// extern rvidHeaderInfo1 rvidHeader1;
extern rvidHeaderInfo2 rvidHeader2;

u8 compressedFrameBuffer[0x10000];
u32 compressedFrameSizes[128];

u16 palBuffer[60][256];
u8 frameBuffer[0xC000*30];					// 30 frames in buffer
int frameBufferCount = 30;
bool useBufferHalf = true;
u16 soundBuffer[2][32000/4];
int soundBufferDivide = 4;
bool useSoundBufferHalf = false;
bool updateSoundBuffer = false;
int sndId = 0;

bool fadeType = false;

int screenBrightness = 31;

void clearBrightness(void) {
	fadeType = true;
	screenBrightness = 0;
}

// Ported from PAlib (obsolete)
void SetBrightness(u8 screen, s8 bright) {
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
FILE* rvidFrameSizeTable;
FILE* rvidSound;
u32 rvidSizeProcessed = 0;
u32 rvidCurrentOffset = 0;
bool showVideoGui = false;
bool updateVideoGuiFrame = true;
bool videoPlaying = false;
bool loadFrame = true;
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
			BG_PALETTE_SUB[0] = 0;
		} else {
			BG_PALETTE_SUB[0] = SPRITE_PALETTE_SUB[0];
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
			BG_PALETTE_SUB[0] = 0;
		} else {
			BG_PALETTE_SUB[0] = SPRITE_PALETTE_SUB[0];
		}
	}
}

void HBlankNull(void) {
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

	if (videoPlaying && (currentFrame <= loadedFrames) && !loadFrame) {
		if ((frameOfRefreshRate % (frameOfRefreshRateLimit/soundBufferDivide)) == 0) {
			updateSoundBuffer = rvidHasSound;
		}

		frameOfRefreshRate++;
		if (frameOfRefreshRate == frameOfRefreshRateLimit) frameOfRefreshRate = 0;

		frameDelay++;
		switch (rvidFps) {
			default:
				loadFrame = (frameDelay == frameOfRefreshRateLimit/rvidFps);
				break;
			case 11:
				loadFrame = (frameDelay == 5+frameDelayEven);
				break;
		}
	}
	if (videoPlaying && (currentFrame <= loadedFrames) && loadFrame) {
		if (currentFrame < rvidFrames) {
			if (rvidInterlaced) {
				REG_BG3Y_SUB = bottomField ? -1 : 0;
			}
			dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBuffer*(0x100*rvidVRes)), (u16*)BG_GFX_SUB+((256/2)*videoYpos), 0x100*rvidVRes);
			if (rvidVRes == (rvidInterlaced ? 96 : 192)) {
				dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBuffer], BG_PALETTE_SUB, 256*2);
			} else {
				dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBuffer]+1, BG_PALETTE_SUB+1, 255*2);
				SPRITE_PALETTE_SUB[0] = palBuffer[currentFrameInBuffer][0];
			}
		}
		if ((currentFrame % rvidFps) == 0) {
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

		// Current time stamp
		if (hourMark < 10) {
			sprintf(numberMark[0], "0%i", hourMark);
		} else {
			sprintf(numberMark[0], "%i", hourMark);
		}
		//printf(":");
		if (minuteMark < 10) {
			sprintf(numberMark[1], "0%i", minuteMark);
		} else {
			sprintf(numberMark[1], "%i", minuteMark);
		}
		//printf(":");
		if (secondMark < 10) {
			sprintf(numberMark[2], "0%i", secondMark);
		} else {
			sprintf(numberMark[2], "%i", secondMark);
		}

		sprintf(timeStamp, "%s:%s:%s/%s:%s:%s",
		numberMark[0], numberMark[1], numberMark[2], numberMark[3], numberMark[4], numberMark[5]);
		updateVideoGuiFrame = true;

		if (rvidInterlaced) {
			bottomField = !bottomField;
		}
		currentFrame++;
		currentFrameInBuffer++;
		if (currentFrameInBuffer == frameBufferCount) {
			currentFrameInBuffer = 0;
		}
		switch (rvidFps) {
			case 11:
				if ((currentFrame % 11) < 10) {
					frameDelayEven = !frameDelayEven;
				}
				break;
		}
		frameDelay = 0;
		loadFrame = false;
	}

	if (showVideoGui && updateVideoGuiFrame) {
		renderGui();
		updateVideoGuiFrame = false;
	}
}

bool confirmReturn = false;
bool confirmStop = false;

bool playerControls(void) {
	if (updateSoundBuffer) {
		sndId = soundPlaySample(soundBuffer[useSoundBufferHalf], SoundFormat_16Bit, (rvidSampleRate/soundBufferDivide)*sizeof(u16), rvidSampleRate, 127, 64, false, 0);
		useSoundBufferHalf = !useSoundBufferHalf;
		toncset(soundBuffer[useSoundBufferHalf], 0, (rvidSampleRate/soundBufferDivide)*sizeof(u16));
		fread(soundBuffer[useSoundBufferHalf], sizeof(u16), rvidSampleRate/soundBufferDivide, rvidSound);
		updateSoundBuffer = false;
	}

	scanKeys();
	touchRead(&touch);
	if (keysDown() & KEY_A
	|| ((keysDown() & KEY_TOUCH) && touch.px >= 73 && touch.px <= 184 && touch.py >= 76 && touch.py <= 113)) {
		if (videoPlaying) {
			videoPlaying = false;
			updateVideoGuiFrame = true;
		} else {
			videoPlaying = true;
			updateVideoGuiFrame = true;
		}
	}
	if (keysDown() & KEY_B
	|| ((keysDown() & KEY_TOUCH) && touch.px >= 2 && touch.px <= 159 && touch.py >= 162 && touch.py <= 191)) {
		confirmReturn = true;
		return true;
	}
	if ((keysDown() & KEY_LEFT) && currentFrame > 0) {
		confirmStop = true;
		return true;
	}
	if (keysDown() & KEY_SELECT) {
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
		frameBufferCount = 60;
		if (rvidVRes <= 190/2) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 192/2; i += 2) {
				videoYpos++;
			}
		}
	} else {
		frameBufferCount = 30;
		if (rvidVRes <= 190) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 192; i += 2) {
				videoYpos++;
			}
		}
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

	fseek(rvid, rvidFramesOffset, SEEK_SET);
	loadedFrames = 0;
	if (rvidCompressed) {
		rvidFrameSizeTable = fopen(filename, "rb");
		fseek(rvidFrameSizeTable, 0x200, SEEK_SET);
		fread(compressedFrameSizes, sizeof(u32), 128, rvidFrameSizeTable);
		for (int i = 0; i < frameBufferCount/2; i++) {
			fread(palBuffer[i], 2, 256, rvid);
			fread(compressedFrameBuffer, 1, compressedFrameSizes[i], rvid);
			lzssDecompress(compressedFrameBuffer, frameBuffer+(i*(0x100*rvidVRes)));
			DC_FlushRange(frameBuffer+(i*(0x100*rvidVRes)), 0x100*rvidVRes);
			loadedFrames++;
		}
	} else {
		for (int i = 0; i < frameBufferCount/2; i++) {
			fread(palBuffer[i], 2, 256, rvid);
			fread(frameBuffer+(i*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
			loadedFrames++;
		}
	}

	if (rvidHasSound) {
		if (rvidSampleRate > 32000) {
			return 1;
		}
		rvidSound = fopen(filename, "rb");
		fseek(rvidSound, rvidSoundOffset, SEEK_SET);
		soundBufferDivide = (rvidFps == 25 || rvidFps == 50) ? 5 : 4;
		toncset(soundBuffer[0], 0, (rvidSampleRate/soundBufferDivide)*sizeof(u16));
		fread(soundBuffer[0], sizeof(u16), rvidSampleRate/soundBufferDivide, rvidSound);
	}

	if (fadeType) {
		fadeType = false;
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
		consoleClear();
	}

	// dmaFillHalfWords(0xFFFF, BG_GFX_SUB, 0x18000);	// Fill top screen with white
	dmaFillHalfWordsAsynch(3, 0xFFFF, BG_PALETTE_SUB, 256*2);	// Fill top screen with white

	bgInitSub(3, BgType_Bmp8, BgSize_B8_256x256, 0, 0);

	if (rvidInterlaced) {
		REG_BG3PD_SUB = 0x80;
	}

	u8* dsImageBuffer8 = new u8[256*192];
	for (int i = 0; i < 256*192; i++) {
		dsImageBuffer8[i] = 0;
	}

	dmaCopyWords(3, dsImageBuffer8, BG_GFX_SUB, 256*192);
	delete[] dsImageBuffer8;

	videoSetMode(MODE_5_3D);
	showVideoGui = true;
	updateVideoGuiFrame = true;

	fadeType = true;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	irqSet(IRQ_HBLANK, (rvidVRes == (rvidInterlaced ? 96 : 192)) ? HBlankNull : rvidInterlaced ? fillBordersInterlaced : fillBorders);
	irqEnable(IRQ_HBLANK);

	// Enable frame rate adjustment
	if (rvidFps == 6 || rvidFps == 12 || rvidFps == 24 || rvidFps == 48) {
		frameOfRefreshRateLimit = 48;
		IPC_SendSync(1);
	} else if (rvidFps == 25 || rvidFps == 50) {
		frameOfRefreshRateLimit = 50;
		IPC_SendSync(2);
	} else {
		frameOfRefreshRateLimit = 60;
		IPC_SendSync(3);
	}

	/* if (rvidVRes < 192) {
		dmaFillHalfWordsAsynch(3, 0, BG_GFX_SUB, 0x18000);	// Fill top screen with black
	} */
	videoPlaying = true;
	while (1) {
		if ((currentFrame % frameBufferCount) >= 0
		&& (currentFrame % frameBufferCount) < frameBufferCount/2)
		{
			if (useBufferHalf) {
				for (int i = frameBufferCount/2; i < frameBufferCount; i++) {
					if (loadedFrames < rvidFrames) {
						if (rvidCompressed) {
							if ((loadedFrames % 128) == 0) {
								fread(compressedFrameSizes, sizeof(u32), 128, rvidFrameSizeTable);
							}
							if (compressedFrameSizes[loadedFrames % 128] > 0
							|| compressedFrameSizes[loadedFrames % 128] <= sizeof(compressedFrameBuffer)) {
								fread(palBuffer[i], 2, 256, rvid);
								fread(compressedFrameBuffer, 1, compressedFrameSizes[loadedFrames % 128], rvid);
								lzssDecompress(compressedFrameBuffer, frameBuffer+(i*(0x100*rvidVRes)));
								DC_FlushRange(frameBuffer+(i*(0x100*rvidVRes)), 0x100*rvidVRes);
							}
						} else {
							fread(palBuffer[i], 2, 256, rvid);
							fread(frameBuffer+(i*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
						}
						loadedFrames++;
					}

					if (playerControls()) {
						break;
					}
				}
				useBufferHalf = false;
			}
		} else if ((currentFrame % frameBufferCount) >= frameBufferCount/2
				&& (currentFrame % frameBufferCount) < frameBufferCount)
		{
			if (!useBufferHalf) {
				for (int i = 0; i < frameBufferCount/2; i++) {
					if (loadedFrames < rvidFrames) {
						if (rvidCompressed) {
							if ((loadedFrames % 128) == 0) {
								fread(compressedFrameSizes, sizeof(u32), 128, rvidFrameSizeTable);
							}
							if (compressedFrameSizes[loadedFrames % 128] > 0
							|| compressedFrameSizes[loadedFrames % 128] <= sizeof(compressedFrameBuffer)) {
								fread(palBuffer[i], 2, 256, rvid);
								fread(compressedFrameBuffer, 1, compressedFrameSizes[loadedFrames % 128], rvid);
								lzssDecompress(compressedFrameBuffer, frameBuffer+(i*(0x100*rvidVRes)));
								DC_FlushRange(frameBuffer+(i*(0x100*rvidVRes)), 0x100*rvidVRes);
							}
						} else {
							fread(palBuffer[i], 2, 256, rvid);
							fread(frameBuffer+(i*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
						}
						loadedFrames++;
					}

					if (playerControls()) {
						break;
					}
				}
				useBufferHalf = true;
			}
		}
		playerControls();
		if (confirmStop || currentFrame > (int)rvidFrames) {
			videoPlaying = false;
			swiWaitForVBlank();

			hourMark = -1;
			minuteMark = 59;
			secondMark = 59;

			sprintf(timeStamp, "00:00:00/%s:%s:%s",
			numberMark[3], numberMark[4], numberMark[5]);
			updateVideoGuiFrame = true;

			useBufferHalf = true;
			useSoundBufferHalf = false;
			loadFrame = true;
			frameOfRefreshRate = 0;
			currentFrame = 0;
			currentFrameInBuffer = 0;
			rvidSizeProcessed = 0;
			frameDelay = 0;
			frameDelayEven = true;
			bottomField = false;

			if (rvidHasSound) {
				soundKill(sndId);
			}

			// Reload video
			fseek(rvid, rvidFramesOffset, SEEK_SET);
			loadedFrames = 0;
			if (rvidCompressed) {
				fseek(rvidFrameSizeTable, 0x200, SEEK_SET);
				fread(compressedFrameSizes, sizeof(u32), 128, rvidFrameSizeTable);
				for (int i = 0; i < frameBufferCount/2; i++) {
					fread(palBuffer[i], 2, 256, rvid);
					fread(compressedFrameBuffer, 1, compressedFrameSizes[i], rvid);
					lzssDecompress(compressedFrameBuffer, frameBuffer+(i*(0x100*rvidVRes)));
					DC_FlushRange(frameBuffer+(i*(0x100*rvidVRes)), 0x100*rvidVRes);
					loadedFrames++;
				}
			} else {
				for (int i = 0; i < frameBufferCount/2; i++) {
					fread(palBuffer[i], 2, 256, rvid);
					fread(frameBuffer+(i*(0x100*rvidVRes)), 1, 0x100*rvidVRes, rvid);
					loadedFrames++;
				}
			}

			if (rvidHasSound) {
				fseek(rvidSound, rvidSoundOffset, SEEK_SET);
				toncset(soundBuffer[0], 0, (rvidSampleRate/soundBufferDivide)*sizeof(u16));
				fread(soundBuffer[0], sizeof(u16), rvidSampleRate/soundBufferDivide, rvidSound);
			}

			confirmStop = false;
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
		soundKill(sndId);
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
	bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

	showVideoGui = false;
	useBufferHalf = true;
	useSoundBufferHalf = false;
	loadFrame = true;
	frameOfRefreshRate = 0;
	currentFrame = 0;
	currentFrameInBuffer = 0;
	rvidSizeProcessed = 0;
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

	// nitroFSInit();

	SetBrightness(0, 31);
	SetBrightness(1, 31);

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
	vramSetBankA(VRAM_A_TEXTURE);
	vramSetBankB(VRAM_B_TEXTURE);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);
	vramSetBankD(VRAM_D_MAIN_BG_0x06000000);
	vramSetBankE(VRAM_E_TEX_PALETTE);
	vramSetBankF(VRAM_F_TEX_PALETTE_SLOT4);
	vramSetBankG(VRAM_G_TEX_PALETTE_SLOT5); // 16Kb of palette ram, and font textures take up 8*16 bytes.

	bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);

	loadGraphics();

	dmaFillHalfWords(0, BG_GFX, 0x18000);		// Clear top screen
	dmaFillHalfWords(0, BG_GFX_SUB, 0x18000);	// Clear bottom screen

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
					fclose(rvidFrameSizeTable);
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
