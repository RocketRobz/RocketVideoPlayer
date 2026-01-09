#include <nds.h>
#include <stdio.h>
#include "rvidHeader.h"

rvidHeaderCheckInfo rvidHeaderCheck;

int rvidFrames = 0;
int rvidFps = 0;
bool rvidReduceFpsBy01 = false;
bool rvidNativeRefreshRate = false;
int rvidHRes = 0;
int rvidVRes = 0;
bool rvidInterlaced = false;
bool rvidDualScreen = false;
int rvidOver256Colors = 0;
bool rvidHasSound = false;
u16 rvidSampleRate = 0;
bool rvidAudioIs16bit = false;
bool rvidCompressed = false;
u32 rvidCompressedFrameSizeTableOffset = 0;
u32 rvidSoundOffset = 0;
u32 rvidSoundRightOffset = 0;

void readRvidHeader(FILE* rvid) {
	u32 rvidFramesOffset = 0x200;

	fseek(rvid, 0, SEEK_SET);
	switch (rvidHeaderCheck.ver) {
		case 1:
			rvidHeaderInfo1 rvidHeader1;
			fread(&rvidHeader1, 1, sizeof(rvidHeader1), rvid);
			rvidFrames = rvidHeader1.frames;
			rvidFps = rvidHeader1.fps;
			rvidReduceFpsBy01 = false;
			rvidNativeRefreshRate = false;
			rvidVRes = rvidHeader1.vRes;
			rvidInterlaced = false;
			rvidDualScreen = false;
			rvidHasSound = rvidHeader1.hasSound;
			rvidSampleRate = rvidHeader1.sampleRate;
			rvidAudioIs16bit = true;
			rvidOver256Colors = true;
			rvidCompressed = false;
			rvidCompressedFrameSizeTableOffset = 0;
			rvidSoundOffset = rvidFramesOffset+((0x200*rvidVRes)*rvidFrames);
			rvidSoundRightOffset = 0;
			rvidHRes = 0x200;
			break;
		case 2:
			rvidHeaderInfo2 rvidHeader2;
			fread(&rvidHeader2, 1, sizeof(rvidHeader2), rvid);
			rvidFrames = rvidHeader2.frames;
			rvidFps = rvidHeader2.fps;
			rvidReduceFpsBy01 = false;
			rvidNativeRefreshRate = false;
			rvidVRes = rvidHeader2.vRes;
			// rvidInterlaced = rvidHeader2.interlaced;
			rvidInterlaced = false;
			rvidDualScreen = false;
			rvidHasSound = rvidHeader2.hasSound;
			rvidSampleRate = rvidHeader2.sampleRate;
			rvidAudioIs16bit = true;
			rvidOver256Colors = true;
			rvidFramesOffset = rvidHeader2.framesOffset;
			rvidCompressed = rvidHeader2.framesCompressed;
			rvidCompressedFrameSizeTableOffset = rvidCompressed ? rvidFramesOffset : 0;
			rvidSoundOffset = rvidHeader2.soundOffset;
			rvidSoundRightOffset = 0;
			rvidHRes = 0x200;
			break;
		case 3:
		case 4: {
			rvidHeaderInfo4 rvidHeader4;
			fread(&rvidHeader4, 1, sizeof(rvidHeader4), rvid);
			rvidFrames = rvidHeader4.frames;
			rvidFps = rvidHeader4.fps;
			if (rvidFps == 0) {
				rvidFps = 60;
				rvidReduceFpsBy01 = false;
				rvidNativeRefreshRate = true;
			} else if (rvidFps >= 0x80) {
				rvidFps -= 0x80;
				rvidReduceFpsBy01 = true;
				rvidNativeRefreshRate = false;
			} else {
				rvidReduceFpsBy01 = false;
				rvidNativeRefreshRate = false;
			}
			rvidVRes = rvidHeader4.vRes;
			rvidInterlaced = rvidHeader4.interlaced;
			rvidDualScreen = rvidHeader4.dualScreen;
			rvidSampleRate = rvidHeader4.sampleRate;
			rvidAudioIs16bit = rvidHeader4.audioBitMode;
			rvidOver256Colors = rvidHeader4.bmpMode;
			rvidCompressedFrameSizeTableOffset = rvidHeader4.compressedFrameSizeTableOffset;
			rvidSoundOffset = rvidHeader4.soundLeftOffset;
			if (rvidHeaderCheck.ver != 3) {
				rvidSoundRightOffset = rvidHeader4.soundRightOffset;
			}

			rvidHRes = rvidOver256Colors ? 0x200 : 0x100;
			rvidCompressed = (rvidCompressedFrameSizeTableOffset > 0);
			rvidHasSound = (rvidSampleRate && rvidSoundOffset);
		}	break;
	}

	fseek(rvid, rvidFramesOffset, SEEK_SET);
}