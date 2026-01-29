#include <gba.h>
// #include <fade.h>
#include <stdlib.h>

#include "rvidHeader.h"
#include "tonccpy.h"

u32 soundBufferPos = 0;
u16 soundBufferReadLen = 0;
u16 soundBufferLen = 0;
int soundBufferDivide = 6;

u32 rvidFrameOffset = 0;
bool videoPlaying = false;
bool videoPausedPrior = false;
bool displayFrame = true;
bool frameDisplayed = false;
int videoYpos = 0;
int frameOfRefreshRate = 0;
int frameOfRefreshRateLimit = 60;
int currentFrame = 0;
bool forceUncompressedFrame = false;
int frameDelay = 0;
bool frameDelayEven = true;
bool bottomField = false;
bool bottomFieldForHBlank = false;

__attribute__((section(".iwram")))
void fillBorders(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	if (scanline > videoYpos+rvidVRes) {
		return;
	} else {
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE[0] = 0;
		} else {
			BG_PALETTE[0] = SPRITE_PALETTE[0];
		}
	}
}

__attribute__((section(".iwram")))
void fillBordersInterlaced(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	int check1 = (videoYpos*2);
	if (bottomFieldForHBlank) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	if (scanline > check1+check2) {
		return;
	} else {
		if (scanline < check1 || scanline >= check1+check2) {
			BG_PALETTE[0] = 0;
		} else {
			BG_PALETTE[0] = SPRITE_PALETTE[0];
		}
	}
}

__attribute__((section(".iwram")))
void HBlank_dmaFrameToScreen(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	const u8* src = rvidCompressed ? (u8*)EWRAM : (u8*)rvidPos + rvidFrameOffset;
	if (rvidVRes < 160 && scanline > videoYpos+rvidVRes) {
		return;
	} else if (rvidVRes == 160 && scanline == 227) {
		dmaCopy(src, BG_PALETTE, 240*2);
	} else {
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE[0] = 0;
		} else {
			dmaCopy(src+((scanline-videoYpos)*rvidHRes), BG_PALETTE, 240*2);
		}
	}
}

__attribute__((section(".iwram")))
void HBlank_dmaFrameToScreenInterlaced(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	const int scanlineVid = scanline+1;
	int check1 = (videoYpos*2);
	if (bottomFieldForHBlank) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	const u8* src = rvidCompressed ? (u8*)EWRAM : (u8*)rvidPos + rvidFrameOffset;
	if (check2 < 160 && scanline > check1+check2) {
		return;
	} else if (check2 == 160 && scanline == 227) {
		dmaCopy(src, BG_PALETTE, 240*2);
	} else {
		if (scanline < check1 || scanline >= check1+check2) {
			BG_PALETTE[0] = 0;
		} else {
			const int videoScanline = (scanlineVid-check1)/2;
			dmaCopy(src+(videoScanline*rvidHRes), BG_PALETTE, 240*2);
		}
	}
}

__attribute__((section(".iwram")))
void dmaFrameToScreen(void) {
	// if (frameOffsets) {
		rvidFrameOffset = (u32)frameOffsets[currentFrame];
	/* } else if (rvidOver256Colors != 0) {
		rvidFrameOffset = rvidFramesOffset + (currentFrame * (rvidHRes*rvidVRes));
	} else {
		rvidFrameOffset = rvidFramesOffset + (currentFrame * (0x200 + rvidHRes*rvidVRes));
	} */

	if (rvidOver256Colors == 2) {
		if (rvidInterlaced) {
			REG_BG2Y = bottomField ? -1 : 0;
			bottomFieldForHBlank = bottomField;
		}
		return;
	}

	if (rvidInterlaced) {
		REG_BG2Y = bottomField ? -1 : 0;
		bottomFieldForHBlank = bottomField;
	}
	if (rvidOver256Colors == 1) {
		const u8* src = (rvidCompressed && !forceUncompressedFrame) ? (u8*)EWRAM : (u8*)rvidPos + rvidFrameOffset;
		dmaCopy(src, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	} else {
		if (rvidVRes == (rvidInterlaced ? 160/2 : 160)) {
			tonccpy(BG_PALETTE, (u8*)rvidPos + rvidFrameOffset, 256*2);
		} else {
			tonccpy(SPRITE_PALETTE, (u8*)rvidPos + rvidFrameOffset, 2);
			tonccpy(BG_PALETTE + 1, (u8*)rvidPos + rvidFrameOffset + 2, 255*2);
		}
		const u8* src = (rvidCompressed && !forceUncompressedFrame) ? (u8*)EWRAM : (u8*)rvidPos + rvidFrameOffset + 0x200;
		dmaCopy(src, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	}
}

static inline void soundKill(void) {
  REG_DMA1CNT = 0;

  /* no-op to let DMA registers catch up */
  asm volatile ("eor r0, r0; eor r0, r0" ::: "r0");

  REG_TM0CNT_H = 0;
}

static void playSoundSamples(const void *src) {
	REG_TM0CNT_H |= TIMER_START;
	REG_DMA1CNT = 0;
	
	/* no-op to let DMA registers catch up */
	asm volatile ("eor r0, r0; eor r0, r0" ::: "r0");
	
	REG_DMA1SAD = (intptr_t)src;
	REG_DMA1DAD = (intptr_t)0x040000a0; /* write to FIFO A address */
	REG_DMA1CNT = DMA_DST_FIXED | DMA_SRC_INC | DMA_REPEAT | DMA32 |
				  DMA_SPECIAL | DMA_ENABLE | 1;
}

#define SOUND_FREQ(n)	((-0x1000000 / (n))) // From libnds

void init_sound(void) {
	// TM0CNT_L is count; TM0CNT_H is control
	REG_TM0CNT_H = 0;
	//turn on sound circuit
	SETSNDRES(1);
	SNDSTAT = SNDSTAT_ENABLE;
	DSOUNDCTRL = 0x0b0e;
	REG_TM0CNT_L = SOUND_FREQ(rvidSampleRate);
}

//---------------------------------------------------------------------------------
__attribute__((section(".iwram")))
void VblankInterrupt()
//---------------------------------------------------------------------------------
{
	if (!videoPlaying) return;

	if (!displayFrame) {
		frameOfRefreshRate++;
		if (frameOfRefreshRate == frameOfRefreshRateLimit) {
			frameOfRefreshRate = 0;
		}

		frameDelay++;
		switch (rvidFps) {
			default:
				displayFrame = (frameDelay == frameOfRefreshRateLimit/rvidFps);
				break;
			case 11:
				displayFrame = (frameDelay == 5+frameDelayEven);
				break;
			case 24:
			case 25:
				displayFrame = (frameDelay == 2+frameDelayEven);
				break;
			case 48:
				displayFrame = (frameOfRefreshRate != 3
							 && frameOfRefreshRate != 8
							 && frameOfRefreshRate != 13
							 && frameOfRefreshRate != 18
							 && frameOfRefreshRate != 23
							 && frameOfRefreshRate != 28
							 && frameOfRefreshRate != 33
							 && frameOfRefreshRate != 38
							 && frameOfRefreshRate != 43
							 && frameOfRefreshRate != 48
							 && frameOfRefreshRate != 53
							 && frameOfRefreshRate != 58);
				break;
			case 50:
				displayFrame = (frameOfRefreshRate != 3
							 && frameOfRefreshRate != 9
							 && frameOfRefreshRate != 16
							 && frameOfRefreshRate != 22
							 && frameOfRefreshRate != 28
							 && frameOfRefreshRate != 34
							 && frameOfRefreshRate != 40
							 && frameOfRefreshRate != 46
							 && frameOfRefreshRate != 51
							 && frameOfRefreshRate != 58);
				break;
		}
	}
	if (rvidHasSound && ((frameOfRefreshRate % (frameOfRefreshRateLimit/soundBufferDivide)) == 0)) {
		const u16 lenUpdate = soundBufferReadLen/soundBufferDivide;
		soundBufferPos += lenUpdate;
		if (videoPausedPrior) {
			playSoundSamples((void*)rvidSoundOffset + soundBufferPos);
			videoPausedPrior = false;
		}
	}
	if (displayFrame) {
		// displaySavedFrameBuffer = false;
		if (currentFrame < rvidFrames) {
			dmaFrameToScreen();
		}
		if (rvidHasSound && (currentFrame % rvidFps) == 0) {
			soundBufferPos = soundBufferReadLen * currentFrame/rvidFps;
			playSoundSamples((void*)rvidSoundOffset + soundBufferPos);
		} 

		if (rvidInterlaced) {
			bottomField = !bottomField;
		}
		currentFrame++;
		forceUncompressedFrame = false;
		frameDisplayed = true;
		switch (rvidFps) {
			case 24:
				frameDelayEven = !frameDelayEven;
				break;
			case 11:
				if ((currentFrame % 11) < 10) {
					frameDelayEven = !frameDelayEven;
				}
				break;
			case 25:
				if ((currentFrame % 24) != 10 && (currentFrame % 24) != 21) {
					frameDelayEven = !frameDelayEven;
				}
				break;
		}
		frameDelay = 0;
		displayFrame = false;
	}
}

bool confirmReturn = false;
bool confirmStop = false;
int videoJump = 0;
bool doubleJump = false;

void loadFrame(void) {
	const u32 size = rvidOver256Colors ? compressedFrameSizes32[currentFrame] : compressedFrameSizes16[currentFrame];
	const void* src = rvidPos + frameOffsets[currentFrame] + (rvidOver256Colors ? 0 : 0x200);
	if (size == (unsigned)rvidHRes*rvidVRes) {
		forceUncompressedFrame = true;
	} else {
		LZ77UnCompWram(src, (u8*)EWRAM);
	}
}

void playerControls(void) {
	scanKeys();
	const u16 pressed = keysDown();
	const u16 held = keysHeld();
	if (pressed & KEY_A) {
		if (videoPlaying) {
			soundKill();

			videoPlaying = false;
		} else {
			videoPlaying = true;
			videoPausedPrior = true;
		}
	}
	doubleJump = (held & KEY_R);
	if ((held & KEY_LEFT) && (held & KEY_DOWN)) {
		videoJump = -3;
		return;
	} else if ((held & KEY_UP) && (held & KEY_RIGHT)) {
		videoJump = 3;
		return;
	}
	if (held & KEY_LEFT) {
		videoJump = -1;
		return;
	} else if (held & KEY_RIGHT) {
		videoJump = 1;
		return;
	} else if (held & KEY_DOWN) {
		videoJump = -2;
		return;
	} else if (held & KEY_UP) {
		videoJump = 2;
		return;
	}
	if (((pressed & KEY_L) || (pressed & KEY_B)) && currentFrame > 0) {
		confirmStop = true;
	}
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void)
//---------------------------------------------------------------------------------
{
	// Set up the interrupt handlers
	irqInit();

	irqSet(IRQ_VBLANK, VblankInterrupt);
	irqEnable(IRQ_VBLANK);

	readRvidHeader((const void*)0x08002000);

	if (rvidHeaderCheck.formatString != 0x44495652 || rvidHeaderCheck.ver < 4 || rvidDualScreen) { // Old versions and dual-screen videos are not supported
		SetMode( MODE_4 | BG2_ON );

		*(vu16*)BG_PALETTE = 0x001F; // Red screen

		while (1) {
			VBlankIntrWait();
		}
	}

	if (rvidInterlaced) {
		if (rvidVRes <= 158/2) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 160/2; i += 2) {
				videoYpos++;
			}
		}
	} else {
		if (rvidVRes <= 158) {
			// Adjust video positioning
			for (int i = rvidVRes; i < 160; i += 2) {
				videoYpos++;
			}
		}
	}

	if (rvidOver256Colors != 1) {
		SetMode( MODE_4 | BG2_ON );
		REG_BG2CNT |= BG_256_COLOR;

		if (rvidOver256Colors == 2) {
			vu8* buffer = (vu8*)EWRAM;
			int i2 = 0;
			for (int i = 240*videoYpos; i < 240*(videoYpos+rvidVRes); i++) {
				buffer[i] = i2;
				i2++;
				if (i2 == 240) i2 = 0;
			}
			tonccpy((u8*)VRAM, (u8*)EWRAM, 240*160);
		}
	} else {
		SetMode( MODE_3 | BG2_ON );
	}

	if (rvidInterlaced) {
		REG_BG2PD = 0x80;
	}

	if (rvidOver256Colors == 2) {
		irqSet(IRQ_HBLANK, rvidInterlaced ? HBlank_dmaFrameToScreenInterlaced : HBlank_dmaFrameToScreen);
		irqEnable(IRQ_HBLANK);
	} else if (rvidOver256Colors == 0 && rvidVRes < (rvidInterlaced ? 160/2 : 160)) {
		irqSet(IRQ_HBLANK, rvidInterlaced ? fillBordersInterlaced : fillBorders);
		irqEnable(IRQ_HBLANK);
	}

	if (rvidHasSound) {
		soundBufferReadLen = rvidSampleRate;
		if (rvidNativeRefreshRate) {
			// Ensure video and audio stay in sync
			for (int i = 0; i < rvidSampleRate; i += 350) {
				soundBufferReadLen++;
			}
		} else if (rvidReduceFpsBy01) {
			// Ensure video and audio stay in sync
			for (int i = 0; i < rvidSampleRate; i += 1000) {
				soundBufferReadLen++;
			}
		}
		// soundBufferDivide = ((rvidFps % 25) == 0) ? 5 : 6;

		init_sound();
	}

	frameOfRefreshRateLimit = 60;
	frameOfRefreshRate = frameOfRefreshRateLimit-1;
	frameDelay = (frameOfRefreshRateLimit/rvidFps)-1;

	if (rvidCompressed) {
		loadFrame();
	}

	videoPlaying = true;

	while (1) {
		if (frameDisplayed) {
			if (rvidCompressed && currentFrame < (int)rvidFrames) {
				loadFrame();
			}
			frameDisplayed = false;
		}
		playerControls();
		if (currentFrame > (int)rvidFrames) {
			confirmStop = true;
		}
		if (confirmStop || videoJump != 0) {
			videoPlaying = false;

			frameOfRefreshRate = frameOfRefreshRateLimit-1;
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
			} else if (currentFrame >= (int)rvidFrames) {
				currentFrame = currentFrameBak;
			}
			frameDelay = (frameOfRefreshRateLimit/rvidFps)-1;
			frameDelayEven = true;
			bottomField = (currentFrame % 2) == 1;

			if (rvidHasSound) {
				soundKill();
			}

			if (rvidCompressed) {
				loadFrame();
			}

			if (videoJump != 0) {
				dmaFrameToScreen();
				for (int i = 0; i < 15; i++) {
					VBlankIntrWait();
				}
			}

			confirmStop = false;
			videoJump = 0;
		}
		VBlankIntrWait();
	}
}


