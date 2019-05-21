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
#include "gl2d.h"
#include "sound.h"
#include "gui.h"
#include "nitrofs.h"

u8 frameBuffer[0x18000*28];
bool useBufferHalf = true;

typedef struct rvidHeaderInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
} rvidHeaderInfo;

rvidHeaderInfo rvidHeader;

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
	*(u16*)(0x0400006C + (0x1000 * screen)) = bright + mode;
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
bool showVideoGui = false;
bool videoPlaying = false;
bool loadFrame = true;
int videoYpos = 0;
int currentFrame = 0;
int currentFrameInBuffer = 0;
int loadedFrames = 0;
int frameDelay = 0;
bool frameDelayEven = true;

char numberMark[6][6];

char timeStamp[32];

int hourMark = -1;
int minuteMark = 59;
int secondMark = 59;

int videoHourMark = -1;
int videoMinuteMark = 59;
int videoSecondMark = 59;

void renderFrames(void) {
	if(fadeType == true) {
		screenBrightness--;
		if (screenBrightness < 0) screenBrightness = 0;
	} else {
		screenBrightness++;
		if (screenBrightness > 25) screenBrightness = 25;
	}
	SetBrightness(0, screenBrightness);
	SetBrightness(1, screenBrightness);

	if (videoPlaying && currentFrame <= loadedFrames) {
		if (!loadFrame) {
			frameDelay++;
			switch (rvidHeader.fps) {
				case 24:
					loadFrame = (frameDelay == 2+frameDelayEven);
					break;
				default:
					loadFrame = (frameDelay == 60/rvidHeader.fps);
					break;
			}
		}
		if (loadFrame) {
			if (currentFrame < (int)rvidHeader.frames) {
				dmaCopyAsynch(frameBuffer+(currentFrameInBuffer*(0x200*rvidHeader.vRes)), (u16*)BG_GFX_SUB+(256*videoYpos), 0x200*rvidHeader.vRes);
			}
			if ((currentFrame % rvidHeader.fps) == 0) {
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
				snprintf(numberMark[0], sizeof(numberMark[0]), "0%i", hourMark);
			} else {
				snprintf(numberMark[0], sizeof(numberMark[0]), "%i", hourMark);
			}
			//printf(":");
			if (minuteMark < 10) {
				snprintf(numberMark[1], sizeof(numberMark[1]), "0%i", minuteMark);
			} else {
				snprintf(numberMark[1], sizeof(numberMark[1]), "%i", minuteMark);
			}
			//printf(":");
			if (secondMark < 10) {
				snprintf(numberMark[2], sizeof(numberMark[2]), "0%i", secondMark);
			} else {
				snprintf(numberMark[2], sizeof(numberMark[2]), "%i", secondMark);
			}

			snprintf(timeStamp, sizeof(timeStamp), "%s:%s:%s/%s:%s:%s",
			numberMark[0], numberMark[1], numberMark[2], numberMark[3], numberMark[4], numberMark[5]);

			currentFrame++;
			currentFrameInBuffer++;
			if (currentFrameInBuffer == 28) {
				currentFrameInBuffer = 0;
			}
			frameDelayEven = !frameDelayEven;
			frameDelay = 0;
			loadFrame = false;
		}
	}

	if (showVideoGui) {
		renderGui();
	}
}

void playRvid(FILE* rvid, const char* filename) {
	bool confirmStop = false;

	videoYpos = 0;

	if (rvidHeader.vRes <= 190) {
		// Adjust video positioning
		for (int i = rvidHeader.vRes; i < 192; i += 2) {
			videoYpos++;
		}
	}
	
	videoHourMark = -1;
	videoMinuteMark = 59;
	videoSecondMark = 59;

	// Get full time stamp
	for (int i = 0; i <= (int)rvidHeader.frames; i += rvidHeader.fps) {
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
		snprintf(numberMark[3], sizeof(numberMark[3]), "0%i", videoHourMark);
	} else {
		snprintf(numberMark[3], sizeof(numberMark[3]), "%i", videoHourMark);
	}
	//printf(":");
	if (videoMinuteMark < 10) {
		snprintf(numberMark[4], sizeof(numberMark[4]), "0%i", videoMinuteMark);
	} else {
		snprintf(numberMark[4], sizeof(numberMark[4]), "%i", videoMinuteMark);
	}
	//printf(":");
	if (videoSecondMark < 10) {
		snprintf(numberMark[5], sizeof(numberMark[5]), "0%i", videoSecondMark);
	} else {
		snprintf(numberMark[5], sizeof(numberMark[5]), "%i", videoSecondMark);
	}

	snprintf(timeStamp, sizeof(timeStamp), "00:00:00/%s:%s:%s",
	numberMark[3], numberMark[4], numberMark[5]);

	fseek(rvid, 0x200, SEEK_SET);
	fread(frameBuffer, 1, (0x200*rvidHeader.vRes)*14, rvid);
	loadedFrames = 13;

	if (fadeType) {
		fadeType = false;
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
		consoleClear();
	}

	dmaFillHalfWords(0xFFFF, BG_GFX_SUB, 0x18000);	// Fill top screen with white

	videoSetMode(MODE_5_3D);
	showVideoGui = true;

	fadeType = true;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	dmaFillHalfWords(0, BG_GFX_SUB, 0x18000);	// Fill top screen with black
	videoPlaying = true;
	snd().beginStream();
	while (1) {
		if ((currentFrame % 28) >= 0 && (currentFrame % 28) < 14) {
			if (useBufferHalf) {
				for (int i = 14; i < 28; i++) {
					snd().updateStream();
					fread(frameBuffer+(i*(0x200*rvidHeader.vRes)), 1, 0x200*rvidHeader.vRes, rvid);
					loadedFrames++;

					scanKeys();
					touchRead(&touch);
					if (keysDown() & KEY_A
					|| ((keysDown() & KEY_TOUCH) && touch.px >= 73 && touch.px <= 184 && touch.py >= 76 && touch.py <= 113)) {
						if (videoPlaying) {
							videoPlaying = false;
							snd().stopStream();
						} else {
							videoPlaying = true;
							snd().beginStream();
						}
					}
					if (keysDown() & KEY_B
					|| ((keysDown() & KEY_TOUCH) && touch.px >= 2 && touch.px <= 159 && touch.py >= 162 && touch.py <= 191)) {
						confirmStop = true;
						break;
					}
				}
				useBufferHalf = false;
			}
		} else if ((currentFrame % 28) >= 14 && (currentFrame % 28) < 28) {
			if (!useBufferHalf) {
				for (int i = 0; i < 14; i++) {
					snd().updateStream();
					fread(frameBuffer+(i*(0x200*rvidHeader.vRes)), 1, 0x200*rvidHeader.vRes, rvid);
					loadedFrames++;

					scanKeys();
					touchRead(&touch);
					if (keysDown() & KEY_A
					|| ((keysDown() & KEY_TOUCH) && touch.px >= 73 && touch.px <= 184 && touch.py >= 76 && touch.py <= 113)) {
						if (videoPlaying) {
							videoPlaying = false;
							snd().stopStream();
						} else {
							videoPlaying = true;
							snd().beginStream();
						}
					}
					if (keysDown() & KEY_B
					|| ((keysDown() & KEY_TOUCH) && touch.px >= 2 && touch.px <= 159 && touch.py >= 162 && touch.py <= 191)) {
						confirmStop = true;
						break;
					}
				}
				useBufferHalf = true;
			}
		}
		scanKeys();
		touchRead(&touch);
		if (keysDown() & KEY_A
		|| ((keysDown() & KEY_TOUCH) && touch.px >= 73 && touch.px <= 184 && touch.py >= 76 && touch.py <= 113)) {
			if (videoPlaying) {
				videoPlaying = false;
				snd().stopStream();
			} else {
				videoPlaying = true;
				snd().beginStream();
			}
		}
		if (currentFrame > (int)rvidHeader.frames) {
			videoPlaying = false;
			snd().stopStream();
			snd().resetStream();
			useBufferHalf = true;
			loadFrame = true;
			currentFrame = 0;
			currentFrameInBuffer = 0;
			frameDelay = 0;
			frameDelayEven = true;

			hourMark = -1;
			minuteMark = 59;
			secondMark = 59;

			snprintf(timeStamp, sizeof(timeStamp), "00:00:00/%s:%s:%s",
			numberMark[3], numberMark[4], numberMark[5]);

			// Reload video
			fseek(rvid, 0x200, SEEK_SET);
			fread(frameBuffer, 1, (0x200*rvidHeader.vRes)*14, rvid);
			loadedFrames = 13;
		}
		if (confirmStop || keysDown() & KEY_B
		|| ((keysDown() & KEY_TOUCH) && touch.px >= 2 && touch.px <= 159 && touch.py >= 162 && touch.py <= 191)) {
			break;
		}
		swiWaitForVBlank();
	}

	videoPlaying = false;
	snd().stopStream();

	fadeType = false;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	snd().resetStream();

	showVideoGui = false;
	useBufferHalf = true;
	loadFrame = true;
	currentFrame = 0;
	currentFrameInBuffer = 0;
	frameDelay = 0;
	frameDelayEven = true;

	hourMark = -1;
	minuteMark = 59;
	secondMark = 59;
}

void LoadBMP(bool top, const char* filename) {
	FILE* file = fopen(filename, "rb");

	if (file) {
		// Start loading
		fseek(file, 0xe, SEEK_SET);
		u8 pixelStart = (u8)fgetc(file) + 0xe;
		fseek(file, pixelStart, SEEK_SET);
		fread(frameBuffer, 2, 0x18000, file);
		u16* src = (u16*)frameBuffer;
		int x = 0;
		int y = 191;
		for (int i=0; i<256*192; i++) {
			if (x >= 256) {
				x = 0;
				y--;
			}
			u16 val = *(src++);
			if (top) {
				BG_GFX[y*256+x] = ((val>>10)&31) | (val&31<<5) | (val&31)<<10 | BIT(15);
			} else {
				BG_GFX_SUB[y*256+x] = ((val>>10)&31) | (val&31<<5) | (val&31)<<10 | BIT(15);
			}
			x++;
		}
	}

	fclose(file);
}

std::string filename;

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	if (!fatInitDefault()) {
		consoleDemoInit();
		iprintf ("fatinitDefault failed!\n");
		stop();
	}

	bool nitroFSInited = nitroFSInit(argv[0]);

	*(u16*)(0x0400006C) |= BIT(14);
	*(u16*)(0x0400006C) &= BIT(15);
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

	REG_BG3CNT = BG_MAP_BASE(0) | BG_BMP16_256x256 | BG_PRIORITY(0);
	REG_BG3X = 0;
	REG_BG3Y = 0;
	REG_BG3PA = 1<<8;
	REG_BG3PB = 0;
	REG_BG3PC = 0;
	REG_BG3PD = 1<<8;

	REG_BG3CNT_SUB = BG_MAP_BASE(0) | BG_BMP16_256x256 | BG_PRIORITY(0);
	REG_BG3X_SUB = 0;
	REG_BG3Y_SUB = 0;
	REG_BG3PA_SUB = 1<<8;
	REG_BG3PB_SUB = 0;
	REG_BG3PC_SUB = 0;
	REG_BG3PD_SUB = 1<<8;
	
	loadGraphics();
	
	if (nitroFSInited) {
		LoadBMP(true, "nitro:/logo_rocketrobz.bmp");
		LoadBMP(false, "nitro:/logo_rocketvideo.bmp");

		fadeType = true;
		for (int i = 0; i < 60*3; i++) {
			swiWaitForVBlank();
		}

		fadeType = false;
		for (int i = 0; i < 25; i++) {
			swiWaitForVBlank();
		}
	}

	dmaFillHalfWords(0, BG_GFX, 0x18000);		// Clear top screen
	dmaFillHalfWords(0, BG_GFX_SUB, 0x18000);	// Clear bottom screen
	snd();

	lcdMainOnBottom();

	keysSetRepeat(25,5);

	vector<string> extensionList;
	extensionList.push_back(".rvid");

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
			clearBrightness();
		
			videoSetMode(MODE_0_2D);
			vramSetBankG(VRAM_G_MAIN_BG);
			consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

			dmaFillHalfWords(0, BG_GFX_SUB, 0x18000);	// Clear top screen

			filename = browseForFile(extensionList);
		}

		if ( strcasecmp (filename.c_str() + filename.size() - 5, ".rvid") != 0 ) {
			iprintf("No .rvid file specified.\n");
			for (int i = 0; i < 60*2; i++) {
				swiWaitForVBlank();
			}
		} else {
			printf("Loading...");
			rvid = fopen(filename.c_str(), "rb");
			if (rvid) {
				fread(&rvidHeader, 1, sizeof(rvidHeaderInfo), rvid);
				if (rvidHeader.formatString != 0x44495652) {
					consoleClear();
					printf("Not a Rocket Video file!");
					fclose(rvid);
					for (int i = 0; i < 60*2; i++) {
						swiWaitForVBlank();
					}
				} else {
					playRvid(rvid, filename.c_str());
					fclose(rvid);
				}
			}
		}

		if (argc >= 2) {
			break;
		}

	}

	return 0;
}
