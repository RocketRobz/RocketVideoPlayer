#include <gba_video.h>
#include <gba_systemcalls.h>
#include <gba_dma.h>
#include <gba_input.h>
#include <gba_interrupt.h>
// #include <fade.h>
#include <stdlib.h>

#include "rvidHeader.h"
#include "tonccpy.h"

//---------------------------------------------------------------------------------
// storage space for palette data
//---------------------------------------------------------------------------------
ALIGN(4) u8 PaletteBuffer[240];

// unsigned int frame;

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

void HBlank_dmaFrameToScreen(void) {
	int scanline = REG_VCOUNT;
	if (scanline == 227) scanline = 0;
	u8* src = (u8*)rvidPos + rvidFrameOffset;
	if (rvidVRes < 160 && scanline > videoYpos+rvidVRes) {
		return;
	} else if (rvidVRes == 160 && scanline >= rvidVRes) {
		dmaCopy(src, BG_PALETTE, 240*2);
	} else {
		scanline++;
		if (scanline < videoYpos || scanline >= videoYpos+rvidVRes) {
			BG_PALETTE[0] = 0;
		} else {
			dmaCopy(src+((scanline-videoYpos)*0x200), BG_PALETTE, 240*2);
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
			// bottomFieldForHBlank = bottomField;
		}
		// dmaCopy((u8*)rvidPos + rvidFrameOffset, (u8*)EWRAM, rvidHRes*rvidVRes);
		return;
	}

	if (rvidInterlaced) {
		REG_BG2Y = bottomField ? -1 : 0;
		// bottomFieldForHBlank = bottomField;
	}
	if (rvidOver256Colors == 1) {
		dmaCopy((u8*)rvidPos + rvidFrameOffset, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	} else {
		tonccpy((u8*)BG_PALETTE, (u8*)rvidPos + rvidFrameOffset, 256*2);
		dmaCopy((u8*)rvidPos + rvidFrameOffset + 0x200, (u8*)VRAM+(rvidHRes*videoYpos), rvidHRes*rvidVRes);
	}
}

//---------------------------------------------------------------------------------
void VblankInterrupt()
//---------------------------------------------------------------------------------
{
	// frame += 1;

	if (!videoPlaying) goto renderFrames_end;

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
			case 25:
				displayFrame = (frameDelay == 2+frameDelayEven);
				break;
			case 48:
				displayFrame = (frameDelay == 1+frameDelayEven);
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
			case 25:
				if ((currentFrame % 24) != 10 && (currentFrame % 24) != 21) {
					frameDelayEven = !frameDelayEven;
				}
				break;
		}
		frameDelay = 0;
		displayFrame = false;
	}

renderFrames_end:
	scanKeys();
}

//---------------------------------------------------------------------------------
// Program entry point
//---------------------------------------------------------------------------------
int main(void)
//---------------------------------------------------------------------------------
{
	// Set up the interrupt handlers
	// irqInit();

	// VBlank doesn't run for some reason
	// irqSet(IRQ_VBLANK, VblankInterrupt);
	// irqEnable(IRQ_VBLANK);

	readRvidHeader((const void*)0x08002000);

	int lastUsedScanline = 0;

	if (rvidDualScreen || rvidCompressedFrameSizeTableOffset) { // Dual-screen and/or compressed videos not supported
		SetMode( MODE_4 | BG2_ON );

		PaletteBuffer[0] = 0x001F; // Red screen

		while (1) {
			const int scanline = REG_VCOUNT;
			if (lastUsedScanline != 161 && scanline > 160) {
				lastUsedScanline = 161;
				dmaCopy(PaletteBuffer, BG_PALETTE, 2);
			} else if (lastUsedScanline != scanline && scanline <= 160) {
				lastUsedScanline = scanline;
			}
			// VBlankIntrWait();
		}
	}

	videoYpos = 0;

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
				PaletteBuffer[i] = i;
			}

			for (int i = 0; i < 160; i++) {
				tonccpy((u8*)VRAM+(240*i), PaletteBuffer, 240);
			}

			// irqSet(IRQ_HBLANK, HBlank_dmaFrameToScreen);
			// irqEnable(IRQ_HBLANK);
		}
	} else {
		SetMode( MODE_3 | BG2_ON );
	}

	frameOfRefreshRateLimit = 60;
	frameOfRefreshRate = frameOfRefreshRateLimit-1;
	frameDelay = (frameOfRefreshRateLimit/rvidFps)-1;

	videoPlaying = true;

	while (1) {
		const int scanline = REG_VCOUNT;
		if (scanline > 160 && scanline != 227) {
			if (currentFrame > (int)rvidFrames) {
				videoPlaying = false;

				frameOfRefreshRate = frameOfRefreshRateLimit-1;
				frameDelay = (frameOfRefreshRateLimit/rvidFps)-1;
				frameDelayEven = true;
				bottomField = false;

				currentFrame = 0;
			}
			if (keysDown() & KEY_A) {
				videoPlaying = !videoPlaying;
			}
			if (lastUsedScanline != 161) {
				lastUsedScanline = 161;
				VblankInterrupt();
			}
		} else if (lastUsedScanline != scanline && (scanline <= 160 || scanline == 227)) {
			lastUsedScanline = scanline;
			if (rvidOver256Colors == 2) {
				HBlank_dmaFrameToScreen();
			}
		}
		// VBlankIntrWait();
	}
}


