#include <gba_video.h>
#include <gba_systemcalls.h>
#include <gba_dma.h>
#include <gba_input.h>
#include <gba_interrupt.h>
// #include <fade.h>
#include <stdlib.h>

#include "rvidHeader.h"
#include "tonccpy.h"

ALIGN(4) u8 rgb565Setup[240];

u32 rvidFrameOffset = 0;
bool videoPlaying = false;
bool videoPausedPrior = false;
bool displayFrame = true;
int videoYpos = 0;
int frameOfRefreshRate = 0;
int frameOfRefreshRateLimit = 60;
int currentFrame = 0;
int frameDelay = 0;
bool frameDelayEven = true;
bool bottomField = false;
bool bottomFieldForHBlank = false;

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

void HBlank_dmaFrameToScreen(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	const u8* src = (u8*)rvidPos + rvidFrameOffset;
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

void HBlank_dmaFrameToScreenInterlaced(void) {
	int scanline = REG_VCOUNT;
	scanline++;
	const int scanlineVid = scanline+1;
	int check1 = (videoYpos*2);
	if (bottomFieldForHBlank) {
		check1++;
	}
	const int check2 = (rvidVRes*2);
	const u8* src = (u8*)rvidPos + rvidFrameOffset;
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

void dmaFrameToScreen(void) {
	if (frameOffsets) {
		rvidFrameOffset = (u32)frameOffsets[currentFrame];
	} else if (rvidOver256Colors != 0) {
		rvidFrameOffset = rvidFramesOffset + (currentFrame * (rvidHRes*rvidVRes));
	} else {
		rvidFrameOffset = rvidFramesOffset + (currentFrame * (0x200 + rvidHRes*rvidVRes));
	}

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
		dmaCopy((u8*)rvidPos + rvidFrameOffset, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	} else {
		if (rvidVRes == (rvidInterlaced ? 160/2 : 160)) {
			tonccpy(BG_PALETTE, (u8*)rvidPos + rvidFrameOffset, 256*2);
		} else {
			tonccpy(SPRITE_PALETTE, (u8*)rvidPos + rvidFrameOffset, 2);
			tonccpy(BG_PALETTE + 1, (u8*)rvidPos + rvidFrameOffset + 2, 255*2);
		}
		dmaCopy((u8*)rvidPos + rvidFrameOffset + 0x200, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	}
}

//---------------------------------------------------------------------------------
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
			case 12:
				displayFrame = (frameDelay == 3+frameDelayEven);
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
	/* if (rvidHasSound && !updateSoundBuffer && ((frameOfRefreshRate % (frameOfRefreshRateLimit/soundBufferDivide)) == 0)) {
		const u16 lenUpdate = soundBufferReadLen/soundBufferDivide;
		if (rvidAudioIs16bit) {
			soundBufferPos[0] += lenUpdate;
			soundBufferPos[1] += lenUpdate;
		} else {
			soundBufferPos8[0] += lenUpdate;
			soundBufferPos8[1] += lenUpdate;
		}
		soundBufferLen -= lenUpdate;
		if (videoPausedPrior) {
			sharedAddr[0] = rvidAudioIs16bit ? (u32)soundBufferPos[0] : (u32)soundBufferPos8[0];
			sharedAddr[1] = rvidAudioIs16bit ? (u32)soundBufferPos[1] : (u32)soundBufferPos8[1];
			sharedAddr[2] = (soundBufferLen*(rvidAudioIs16bit ? 2 : 1)) >> 2;
			sharedAddr[3] = rvidSampleRate;
			sharedAddr[4] = rvidAudioIs16bit;
			fifoSendValue32(FIFO_USER_03, 1);
			videoPausedPrior = false;
		}
	} */
	if (displayFrame) {
		// displaySavedFrameBuffer = false;
		if (currentFrame < rvidFrames) {
			dmaFrameToScreen();
		}
		/* if (rvidHasSound && (currentFrame % rvidFps) == 0) {
			sharedAddr[0] = (u32)soundBuffer[0][useSoundBufferHalf];
			sharedAddr[1] = (u32)soundBuffer[rvidSoundRightOffset ? 1 : 0][useSoundBufferHalf];
			sharedAddr[2] = (soundBufferReadLen*(rvidAudioIs16bit ? 2 : 1)) >> 2;
			sharedAddr[3] = rvidSampleRate;
			sharedAddr[4] = rvidAudioIs16bit;
			fifoSendValue32(FIFO_USER_03, 1);

			if (rvidAudioIs16bit) {
				soundBufferPos[0] = (u16*)sharedAddr[0];
				soundBufferPos[1] = (u16*)sharedAddr[1];
			} else {
				soundBufferPos8[0] = (u8*)sharedAddr[0];
				soundBufferPos8[1] = (u8*)sharedAddr[1];
			}
			soundBufferLen = rvidSampleRate;
			updateSoundBuffer = true;
		} */

		if (rvidInterlaced) {
			bottomField = !bottomField;
		}
		currentFrame++;
		switch (rvidFps) {
			case 12:
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

void playerControls(void) {
	scanKeys();
	const u16 pressed = keysDown();
	const u16 held = keysHeld();
	if (pressed & KEY_A) {
		if (videoPlaying) {
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

	if (rvidHeaderCheck.formatString != 0x44495652 || rvidDualScreen || rvidCompressedFrameSizeTableOffset) { // Dual-screen and/or compressed videos not supported
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
			for (int i = 0; i < 240; i++) {
				rgb565Setup[i] = i;
			}

			for (int i = 240*videoYpos; i < 240*(videoYpos+rvidVRes); i++) {
				tonccpy((u8*)VRAM+(240*i), rgb565Setup, 240);
			}
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

	frameOfRefreshRateLimit = 60;
	frameOfRefreshRate = frameOfRefreshRateLimit-1;
	frameDelay = (frameOfRefreshRateLimit/rvidFps)-1;

	videoPlaying = true;

	while (1) {
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
			bottomField = false;

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


