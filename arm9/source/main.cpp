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

u16 frameBuffer[30][256*192];
bool useBufferHalf = true;

typedef struct rvidHeaderInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
} rvidHeaderInfo;

rvidHeaderInfo rvidHeader;

using namespace std;

//---------------------------------------------------------------------------------
void stop (void) {
//---------------------------------------------------------------------------------
	while (1) {
		swiWaitForVBlank();
	}
}

char filePath[PATH_MAX];

FILE* rvid;
bool videoPlaying = false;
bool loadFrame = false;
int currentFrame = 0;
int frameDelay = 1;
bool frameDelayEven = false;

void renderFrames(void) {
	if (videoPlaying) {
		frameDelay++;
		if (rvidHeader.fps == 24) {
			loadFrame = (frameDelay == 2+frameDelayEven);
		}
		if (loadFrame) {
			if (currentFrame < (int)rvidHeader.frames) {
				dmaCopyAsynch(frameBuffer[currentFrame % 30], BG_GFX_SUB, 0x18000);
			}
			currentFrame++;
			frameDelayEven = !frameDelayEven;
			frameDelay = 0;
		}
	}
}

void playRvid(FILE* rvid) {
	fseek(rvid, 0x200, SEEK_SET);
	fread(frameBuffer[0], 1, 0x168000, rvid);
	consoleClear();
	printf("Loaded successfully!\n");
	printf("\n");
	printf("B: Stop");
	videoPlaying = true;
	while (1) {
		if ((currentFrame % 30) >= 0 && (currentFrame % 30) < 15) {
			if (useBufferHalf) {
				fread(frameBuffer[15], 1, 0x168000, rvid);
				useBufferHalf = false;
			}
		} else if ((currentFrame % 30) >= 15 && (currentFrame % 30) < 30) {
			if (!useBufferHalf) {
				fread(frameBuffer[0], 1, 0x168000, rvid);
				useBufferHalf = true;
			}
		}
		scanKeys();
		if (currentFrame > (int)rvidHeader.frames || keysDown() & KEY_B) {
			break;
		}
		swiWaitForVBlank();
	}

	videoPlaying = false;
	useBufferHalf = true;
	loadFrame = false;
	currentFrame = 0;
	frameDelay = 1;
	frameDelayEven = false;
}

//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------

	std::string filename;

	lcdMainOnBottom();

	videoSetMode(MODE_0_2D);
	vramSetBankG(VRAM_G_MAIN_BG);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);

	videoSetModeSub(MODE_3_2D | DISPLAY_BG3_ACTIVE);
	vramSetBankC(VRAM_C_SUB_BG_0x06200000);

	REG_BG3CNT_SUB = BG_MAP_BASE(0) | BG_BMP16_256x256 | BG_PRIORITY(0);
	REG_BG3X_SUB = 0;
	REG_BG3Y_SUB = 0;
	REG_BG3PA_SUB = 1<<8;
	REG_BG3PB_SUB = 0;
	REG_BG3PC_SUB = 0;
	REG_BG3PD_SUB = 1<<8;

	if (!fatInitDefault()) {
		iprintf ("fatinitDefault failed!\n");
		stop();
	}

	keysSetRepeat(25,5);

	vector<string> extensionList;
	extensionList.push_back(".rvid");

	irqSet(IRQ_VBLANK, renderFrames);
	irqEnable(IRQ_VBLANK);

	while(1) {

		filename = browseForFile(extensionList);

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
					playRvid(rvid);
					fclose(rvid);
				}
			}
		}

	}

	return 0;
}
