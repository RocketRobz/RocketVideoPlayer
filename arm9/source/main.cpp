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
#include "graphics/fontHandler.h"
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
// bool dsiWramAvailable = false;

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

u32* frameOffsets = NULL;

// u8 compressedFrameBuffer[0xC000];
u8 compressedFrameBuffer[0x18000];
u16* compressedFrameSizes16 = NULL;
u32* compressedFrameSizes32 = NULL;

u16 palBuffer[60][256];
u8* frameBuffer = NULL;					// 32 frames in buffer (64 halved frames for interlaced videos)
int frameBufferCount = 32;
int topBg;
int bottomBg;
u16* topBgPtr = NULL;
u16* bottomBgPtr = NULL;
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

bool reinitFileBrowserGfx = true;

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
u32 rvidCurrentOffset = 0;
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
int currentFrameInBufferForHBlank = 0;
int loadedSingleFrames = 0;
int loadedFrames = 0;
int frameDelay = 0;
bool frameDelayEven = true;
bool bottomField = false;

char filenameToDisplay[256];
bool filenameDisplayCentered = false;

char timeStamp[96];

int hourMark = -1;
int minuteMark = 59;
int secondMark = 59;

int videoHourMark = -1;
int videoMinuteMark = 59;
int videoSecondMark = 59;

int fileCursorPosition = 2;
u16 fileHighlightColor = 0;

ITCM_CODE void fileHighlighter(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	if (scanline == 11) {
		BG_PALETTE[0] = whiteColor;
	} else if (scanline >= fileCursorPosition*8 && scanline < (fileCursorPosition+1)*8) {
		BG_PALETTE[0] = fileHighlightColor;
	} else {
		BG_PALETTE[0] = blackColor;
	}
}

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

ITCM_CODE void HBlank_dmaFrameToScreen(void) {
	int scanline = REG_VCOUNT;
	if (scanline > videoYpos+rvidVRes) {
		return;
	} else if (rvidVRes == 192 && scanline == rvidVRes) {
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferForHBlank*(0x200*rvidVRes)), BG_PALETTE_SUB, 256*2);
	} else {
		scanline++;
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE_SUB[0] = blackColor;
		} else {
			dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferForHBlank*(0x200*rvidVRes))+((scanline-videoYpos)*0x200), BG_PALETTE_SUB, 256*2);
		}
	}
}

ITCM_CODE void HBlank_dmaDualFrameToScreen(void) {
	int scanline = REG_VCOUNT;
	const int currentFrameInBufferDoubled = currentFrameInBufferForHBlank*2;
	if (scanline > videoYpos+rvidVRes) {
		return;
	} else if (rvidVRes == 192 && scanline == rvidVRes) {
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(0x200*rvidVRes)), BG_PALETTE_SUB, 256*2);
		dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(0x200*rvidVRes)), BG_PALETTE, 256*2);
	} else {
		scanline++;
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE_SUB[0] = blackColor;
			BG_PALETTE[0] = blackColor;
		} else {
			dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(0x200*rvidVRes))+((scanline-videoYpos)*0x200), BG_PALETTE_SUB, 256*2);
			dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(0x200*rvidVRes))+((scanline-videoYpos)*0x200), BG_PALETTE, 256*2);
		}
	}
}

ITCM_CODE void HBlank_dmaFrameToScreenInterlaced(void) {
	int scanline = REG_VCOUNT;
	int check1 = (videoYpos*2);
	if (REG_BG3Y_SUB == -1) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	if (scanline > check1+check2) {
		return;
	} else if (check2 == 192 && scanline == check2) {
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferForHBlank*(0x200*rvidVRes)), BG_PALETTE_SUB, 256*2);
	} else {
		scanline++;
		if (scanline < check1 || scanline >= check1+check2) {
			BG_PALETTE_SUB[0] = blackColor;
		} else {
			dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferForHBlank*(0x200*rvidVRes))+(((scanline/2)-(videoYpos*2))*0x200), BG_PALETTE_SUB, 256*2);
		}
	}
}

ITCM_CODE void HBlank_dmaDualFrameToScreenInterlaced(void) {
	int scanline = REG_VCOUNT;
	const int currentFrameInBufferDoubled = currentFrameInBufferForHBlank*2;
	int check1 = (videoYpos*2);
	if (REG_BG3Y_SUB == -1) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	if (scanline > check1+check2) {
		return;
	} else if (check2 == 192 && scanline == check2) {
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(0x200*rvidVRes)), BG_PALETTE_SUB, 256*2);
		dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(0x200*rvidVRes)), BG_PALETTE, 256*2);
	} else {
		scanline++;
		if (scanline < check1 || scanline >= check1+check2) {
			BG_PALETTE_SUB[0] = blackColor;
			BG_PALETTE[0] = blackColor;
		} else {
			dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(0x200*rvidVRes))+(((scanline/2)-(videoYpos*2))*0x200), BG_PALETTE_SUB, 256*2);
			dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(0x200*rvidVRes))+(((scanline/2)-(videoYpos*2))*0x200), BG_PALETTE, 256*2);
		}
	}
}

void HBlankNull(void) {
}

ITCM_CODE void dmaFrameToScreen(void) {
	if (rvidOver256Colors == 2) {
		if (rvidInterlaced) {
			REG_BG3Y_SUB = bottomField ? -1 : 0;
			if (rvidDualScreen) {
				REG_BG3Y = bottomField ? -1 : 0;
			}
		}
		currentFrameInBufferForHBlank = currentFrameInBuffer;
		return;
	}

	if (rvidDualScreen) {
		if (rvidInterlaced) {
			REG_BG3Y_SUB = bottomField ? -1 : 0;
			REG_BG3Y = bottomField ? -1 : 0;
		}
		const int currentFrameInBufferDoubled = currentFrameInBuffer*2;
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBufferDoubled*(rvidHRes*rvidVRes)), topBgPtr+((rvidHRes/2)*videoYpos), rvidHRes*rvidVRes);
		dmaCopyWordsAsynch(1, frameBuffer+((currentFrameInBufferDoubled+1)*(rvidHRes*rvidVRes)), bottomBgPtr+((rvidHRes/2)*videoYpos), rvidHRes*rvidVRes);
		if (!rvidOver256Colors) {
			if (rvidVRes == (rvidInterlaced ? 96 : 192)) {
				dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBufferDoubled], BG_PALETTE_SUB, 256*2);
				dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBufferDoubled+1], BG_PALETTE, 256*2);
			} else {
				dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBufferDoubled]+1, BG_PALETTE_SUB+1, 255*2);
				dmaCopyHalfWordsAsynch(3, palBuffer[currentFrameInBufferDoubled+1]+1, BG_PALETTE+1, 255*2);
				SPRITE_PALETTE_SUB[0] = palBuffer[currentFrameInBufferDoubled][0];
				SPRITE_PALETTE[0] = palBuffer[currentFrameInBufferDoubled+1][0];
			}
		}
	} else {
		if (rvidInterlaced) {
			REG_BG3Y_SUB = bottomField ? -1 : 0;
		}
		dmaCopyWordsAsynch(0, frameBuffer+(currentFrameInBuffer*(rvidHRes*rvidVRes)), topBgPtr+((rvidHRes/2)*videoYpos), rvidHRes*rvidVRes);
		if (!rvidOver256Colors) {
			if (rvidVRes == (rvidInterlaced ? 96 : 192)) {
				dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBuffer], BG_PALETTE_SUB, 256*2);
			} else {
				dmaCopyHalfWordsAsynch(2, palBuffer[currentFrameInBuffer]+1, BG_PALETTE_SUB+1, 255*2);
				SPRITE_PALETTE_SUB[0] = palBuffer[currentFrameInBuffer][0];
			}
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
			sprintf(timeStamp, "%02i:%02i:%02i/%02i:%02i:%02i",
			hourMark, minuteMark, secondMark, videoHourMark, videoMinuteMark, videoSecondMark);
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

static inline void loadFramePal(const int num, const int loadedFrameNum) {
	if (rvidOver256Colors) return;

	static int previousNum = 0;

	if (rvidCurrentOffset == frameOffsets[loadedFrameNum]) {
		tonccpy(palBuffer[num], palBuffer[previousNum], 256*2);
	} else {
		fread(palBuffer[num], 2, 256, rvid);
		if (colorTable) {
			for (int i = 0; i < 256; i++) {
				palBuffer[num][i] = colorTable[palBuffer[num][i]];
			}
		}
	}

	previousNum = num;
}

/* static inline void applyColorLutToFrame(u16* frame) {
	if (!rvidOver256Colors || !colorTable) return;

	if (rvidOver256Colors == 2) {
		for (int i = 0; i < 256*rvidVRes; i++) {
			frame[i] = colorTable[frame[i]];
		}
	} else {
		for (int i = 0; i < 256*rvidVRes; i++) {
			frame[i] = colorTable[frame[i]] | BIT(15);
		}
	}
} */

void loadFrame(const int num) {
	if (loadedFrames >= rvidFrames) {
		return;
	}

	static int previousNum = 0;
	static int previousNumDS = 0;

	if (rvidDualScreen) {
		if (rvidCompressed) {
			for (int b = 0; b < 2; b++) {
				const int pos = (num*2)+b;
				loadFramePal(pos, loadedSingleFrames);
				u8* dst = frameBuffer+(pos*(rvidHRes*rvidVRes));
				if (rvidCurrentOffset == frameOffsets[loadedSingleFrames]) {
					// Duplicate frame found
					const u8* src = frameBuffer+(previousNumDS*(rvidHRes*rvidVRes));
					tonccpy(dst, src, rvidHRes*rvidVRes);
				} else {
					const int loadedFramesPos = (loadedFrames*2)+b;
					const u32 size = rvidOver256Colors ? compressedFrameSizes32[loadedFramesPos] : compressedFrameSizes16[loadedFramesPos];
					if (size == (unsigned)rvidHRes*rvidVRes) {
						fread(dst, 1, rvidHRes*rvidVRes, rvid);
					} else {
						fread(compressedFrameBuffer, 1, size, rvid);
						sndUpdateStream();
						lzssDecompress(compressedFrameBuffer, dst);
					}
					// applyColorLutToFrame((u16*)dst);
				}
				sndUpdateStream();
				rvidCurrentOffset = frameOffsets[loadedSingleFrames];
				loadedSingleFrames++;
				previousNumDS = pos;
			}
		} else {
			for (int b = 0; b < 2; b++) {
				const int pos = (num*2)+b;
				loadFramePal(pos, loadedSingleFrames);
				u8* dst = frameBuffer+(pos*(rvidHRes*rvidVRes));
				if (rvidCurrentOffset == frameOffsets[loadedSingleFrames]) {
					// Duplicate frame found
					const u8* src = frameBuffer+(previousNumDS*(rvidHRes*rvidVRes));
					tonccpy(dst, src, rvidHRes*rvidVRes);
				} else {
					fread(dst, 1, rvidHRes*rvidVRes, rvid);
					// applyColorLutToFrame((u16*)dst);
				}
				sndUpdateStream();
				rvidCurrentOffset = frameOffsets[loadedSingleFrames];
				loadedSingleFrames++;
				previousNumDS = pos;
			}
		}
	} else {
		loadFramePal(num, loadedFrames);
		u8* dst = frameBuffer+(num*(rvidHRes*rvidVRes));
		if (rvidCurrentOffset == frameOffsets[loadedFrames]) {
			// Duplicate frame found
			const u8* src = frameBuffer+(previousNum*(rvidHRes*rvidVRes));
			tonccpy(dst, src, rvidHRes*rvidVRes);
		} else {
			if (rvidCompressed) {
				const u32 size = rvidOver256Colors ? compressedFrameSizes32[loadedFrames] : compressedFrameSizes16[loadedFrames];
				if (size == (unsigned)rvidHRes*rvidVRes) {
					fread(dst, 1, rvidHRes*rvidVRes, rvid);
				} else {
					fread(compressedFrameBuffer, 1, size, rvid);
					sndUpdateStream();
					lzssDecompress(compressedFrameBuffer, dst);
				}
			} else {
				fread(dst, 1, rvidHRes*rvidVRes, rvid);
			}
			// applyColorLutToFrame((u16*)dst);
		}
		sndUpdateStream();
		rvidCurrentOffset = frameOffsets[loadedFrames];
	}
	loadedFrames++;
	previousNum = num;
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

	if (rvidHasSound && (rvidSampleRate > 32000)) {
		return 1;
	}

	if (rvidOver256Colors && colorTable) {
		return 2;
	}

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
	if (rvidOver256Colors && !isDSiMode()) {
		frameBufferCount /= 2;
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
	filenameDisplayCentered = (calcLargeFontWidth(filenameToDisplay) <= 248);

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
	sprintf(timeStamp, "00:00:00/%02i:%02i:%02i",
	videoHourMark, videoMinuteMark, videoSecondMark);
	updateVideoGuiFrame = true;
	updatePlayBar();

	if (rvidOver256Colors) {
		const int amount = isDSiMode() ? 32 : 16;
		frameBuffer = new u8[0x18000*amount];
	} else {
		frameBuffer = new u8[0xC000*32];
	}

	if (rvidDualScreen) {
		frameOffsets = new u32[rvidFrames*2];
		fread(frameOffsets, 4, rvidFrames*2, rvid);
	} else {
		frameOffsets = new u32[rvidFrames];
		fread(frameOffsets, 4, rvidFrames, rvid);
	}

	if (rvidCompressed) {
		fseek(rvid, rvidCompressedFrameSizeTableOffset, SEEK_SET);
		if (rvidOver256Colors) {
			compressedFrameSizes32 = new u32[rvidFrames];
			fread(compressedFrameSizes32, 4, rvidFrames, rvid);
		} else {
			compressedFrameSizes16 = new u16[rvidFrames];
			fread(compressedFrameSizes16, 2, rvidFrames, rvid);
		}
	}
	rvidCurrentOffset = 0;
	fseek(rvid, frameOffsets[0], SEEK_SET);
	loadedSingleFrames = 0;
	loadedFrames = 0;
	for (int i = 0; i < frameBufferCount/2; i++) {
		loadFrame(i);
	}

	if (rvidHasSound) {
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

	dmaFillHalfWords((rvidOver256Colors == 1) ? blackColor : whiteColor, BG_PALETTE_SUB, 256*2);	// Fill top screen with black/white
	topBg = bgInitSub(3, (rvidOver256Colors == 1) ? BgType_Bmp16 : BgType_Bmp8, (rvidOver256Colors == 1) ? BgSize_B16_256x256 : BgSize_B8_256x256, 0, 0);
	topBgPtr = bgGetGfxPtr(topBg);
	if (rvidDualScreen) {
		dmaFillHalfWords((rvidOver256Colors == 1) ? blackColor : whiteColor, BG_PALETTE, 256*2);	// Fill bottom screen with black/white
		bottomBg = bgInit(3, (rvidOver256Colors == 1) ? BgType_Bmp16 : BgType_Bmp8, (rvidOver256Colors == 1) ? BgSize_B16_256x256 : BgSize_B8_256x256, 0, 0);
		bottomBgPtr = bgGetGfxPtr(bottomBg);
	} else {
		bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 0, 0);
	}

	REG_BG3PD = 0x100;
	REG_BG3Y = 0;
	if (rvidInterlaced) {
		REG_BG3PD_SUB = 0x80;
		if (rvidDualScreen) {
			REG_BG3PD = 0x80;
		}
	}

	if (rvidOver256Colors != 1) {
		toncset(compressedFrameBuffer, 0, 256*192);
		if (rvidOver256Colors == 2) {
			for (int i = 256*videoYpos; i < 256*(videoYpos+rvidVRes); i++) {
				compressedFrameBuffer[i] = i;
			}
		}
		DC_FlushRange(compressedFrameBuffer, 256*192);
	}

	if (rvidOver256Colors == 1) {
		dmaFillHalfWords(whiteColor | BIT(15), topBgPtr, (256*192)*2);
	} else {
		dmaCopyWords(3, compressedFrameBuffer, topBgPtr, 256*192);
	}
	if (rvidDualScreen) {
		if (rvidOver256Colors == 1) {
			dmaFillHalfWords(whiteColor | BIT(15), bottomBgPtr, (256*192)*2);
		} else {
			dmaCopyWords(3, compressedFrameBuffer, bottomBgPtr, 256*192);
		}
	} else {
		renderGuiBg();
	}

	videoSetMode(rvidDualScreen ? (MODE_5_2D | DISPLAY_BG3_ACTIVE) : (MODE_5_3D | DISPLAY_BG3_ACTIVE));
	showVideoGui = true;
	updateVideoGuiFrame = true;

	fadeType = true;
	for (int i = 0; i < 25; i++) {
		swiWaitForVBlank();
	}

	if ((rvidOver256Colors == 1) && (rvidVRes < (rvidInterlaced ? 96 : 192))) {
		dmaFillHalfWords(0, topBgPtr, (256*192)*2);
		if (rvidDualScreen) {
			dmaFillHalfWords(0, bottomBgPtr, (256*192)*2);
		}
	}

	if (rvidOver256Colors == 2) {
		irqSet(IRQ_HBLANK, rvidInterlaced ? (rvidDualScreen ? HBlank_dmaDualFrameToScreenInterlaced : HBlank_dmaFrameToScreenInterlaced) : (rvidDualScreen ? HBlank_dmaDualFrameToScreen : HBlank_dmaFrameToScreen));
		irqEnable(IRQ_HBLANK);
	} else if (rvidOver256Colors == 0) {
		irqSet(IRQ_HBLANK, (rvidVRes == (rvidInterlaced ? 96 : 192)) ? HBlankNull : rvidInterlaced ? fillBordersInterlaced : fillBorders);
		irqEnable(IRQ_HBLANK);
	}

	// Enable frame rate adjustment
	if (rvidFps == 25 || rvidFps == 50) {
		frameOfRefreshRateLimit = 50;
		IPC_SendSync(1);
	} else {
		frameOfRefreshRateLimit = 60;
		IPC_SendSync(2);
	}

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
				sprintf(timeStamp, "00:00:00/%02i:%02i:%02i",
				videoHourMark, videoMinuteMark, videoSecondMark);
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
				sprintf(timeStamp, "%02i:%02i:%02i/%02i:%02i:%02i",
				hourMark, minuteMark, secondMark, videoHourMark, videoMinuteMark, videoSecondMark);
				updateVideoGuiFrame = true;

				if (updateVideoGuiFrame) {
					updatePlayBar();
				} else {
					updateVideoGuiFrame = updatePlayBar();
				}
			}

			// Reload video
			rvidCurrentOffset = 0;
			u32 rvidNextOffset = 0;
			if (rvidDualScreen) {
				rvidNextOffset = frameOffsets[currentFrame*2];
			} else {
				rvidNextOffset = frameOffsets[currentFrame];
			}
			fseek(rvid, rvidNextOffset, SEEK_SET);
			loadedSingleFrames = loadedFrames = currentFrame;
			if (rvidDualScreen) {
				loadedSingleFrames *= 2;
			}
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

	if (rvidOver256Colors != 1) {
		irqSet(IRQ_HBLANK, HBlankNull);
	}
	if (rvidCompressed) {
		if (rvidOver256Colors) {
			delete[] compressedFrameSizes32;
		} else {
			delete[] compressedFrameSizes16;
		}
	}
	delete[] frameOffsets;
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

	reinitFileBrowserGfx = true;
	return 0;
}

void setupFileBrowserGfx(void) {
	if (!reinitFileBrowserGfx) {
		return;
	}
	reinitFileBrowserGfx = false;

	videoSetMode(MODE_0_2D);
	vramSetBankG(VRAM_G_MAIN_BG);
	consoleInit(NULL, 0, BgType_Text4bpp, BgSize_T_256x256, 15, 0, true, true);
	if (colorTable) {
		for (int i = 0; i < 256; i++) {
			BG_PALETTE[i] = colorTable[BG_PALETTE[i]];
		}
	}

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

	fadeType = true;
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

		// u32 headerTidUppercase = *(u32*)0x02FFFE0C;
		// u32 headerTidLowercase = *(u32*)0x02FFFE0C;
		// headerTidLowercase += 0x20202020;

		// *(vu32*)0x03700000 = headerTidUppercase;
		// *(vu32*)0x03708000 = headerTidLowercase;
		// dsiWramAvailable = ((*(vu32*)0x03700000 == headerTidUppercase) && (*(vu32*)0x03708000 == headerTidLowercase));
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
			setupFileBrowserGfx();
			irqSet(IRQ_HBLANK, fileHighlighter);
			irqEnable(IRQ_HBLANK);

			filename = browseForFile(extensionList);

			irqSet(IRQ_HBLANK, HBlankNull);
			BG_PALETTE[0] = blackColor;
		}

		if ( strcasecmp (filename.c_str() + filename.size() - 5, ".rvid") != 0 ) {
			consoleClear();
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
					const int err = playRvid(filename.c_str());
					fclose(rvid);
					fclose(rvidSound);
					if ((err > 0) && !fadeType) {
						setupFileBrowserGfx();
					}
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
					} else if (err == 2) {
						consoleClear();
						printf("BMP16 videos are not supported\n");
						printf("with screen filters.\n");
						printf("\n");
						printf("Please re-convert your video\n");
						printf("to BMP8, or turn off the\n");
						printf("screen filter.\n");
					} else if (err == 1) {
						consoleClear();
						printf("Audio sample rate is higher\n");
						printf("than 32000Hz.\n");
						printf("\n");
						printf("Please lower the sample rate\n");
						printf("to 32000Hz or less.\n");
					}
					if (err > 0) {
						printf("\n");
						printf("A: OK\n");
						while (1) {
							scanKeys();
							if (keysDown() & KEY_A) {
								break;
							}
						}
						if (argc >= 2) {
							fadeType = false;
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
