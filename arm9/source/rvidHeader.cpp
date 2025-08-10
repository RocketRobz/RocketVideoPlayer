#include <nds.h>
#include <stdio.h>
#include "rvidHeader.h"

rvidHeaderCheckInfo rvidHeaderCheck;
// rvidHeaderInfo1 rvidHeader1;
rvidHeaderInfo2 rvidHeader2;

int rvidFrames = 0;
int rvidFps = 0;
int rvidHRes = 0;
int rvidVRes = 0;
bool rvidInterlaced = false;
bool rvidDualScreen = false;
int rvidOver256Colors = 0;
bool rvidHasSound = false;
u16 rvidSampleRate = 0;
bool rvidCompressed = false;
u32 rvidFramesOffset = 0;
u32 rvidSoundOffset = 0;

void readRvidHeader(FILE* rvid) {
	fseek(rvid, 0, SEEK_SET);
	/* switch (rvidHeaderCheck.ver) {
		case 1:
			fread(&rvidHeader1, 1, sizeof(rvidHeader1), rvid);
			rvidFrames = rvidHeader1.frames;
			rvidFps = rvidHeader1.fps;
			rvidVRes = rvidHeader1.vRes;
			rvidInterlaced = false;
			rvidHasSound = rvidHeader1.hasSound;
			rvidSampleRate = rvidHeader1.sampleRate;
			rvidCompressed = false;
			rvidFramesOffset = 0x200;
			rvidSoundOffset = 0x200+((0x200*rvidVRes)*rvidFrames);
			break;
		case 2:
			fread(&rvidHeader2, 1, sizeof(rvidHeader2), rvid);
			rvidFrames = rvidHeader2.frames;
			rvidFps = rvidHeader2.fps;
			rvidVRes = rvidHeader2.vRes;
			rvidInterlaced = rvidHeader2.interlaced;
			rvidHasSound = rvidHeader2.hasSound;
			rvidSampleRate = rvidHeader2.sampleRate;
			rvidCompressed = rvidHeader2.framesCompressed;
			rvidFramesOffset = rvidHeader2.framesOffset;
			rvidSoundOffset = rvidHeader2.soundOffset;
			break;
	} */

	fread(&rvidHeader2, 1, sizeof(rvidHeader2), rvid);
	rvidFrames = rvidHeader2.frames;
	rvidFps = rvidHeader2.fps;
	rvidVRes = rvidHeader2.vRes;
	rvidInterlaced = rvidHeader2.interlaced;
	rvidDualScreen = rvidHeader2.dualScreen;
	rvidSampleRate = rvidHeader2.sampleRate;
	rvidCompressed = rvidHeader2.framesCompressed;
	rvidOver256Colors = rvidHeader2.bmpMode;
	rvidFramesOffset = rvidHeader2.framesOffset;
	rvidSoundOffset = rvidHeader2.soundOffset;

	rvidHRes = rvidOver256Colors ? 0x200 : 0x100;
	rvidHasSound = (rvidSampleRate && rvidSoundOffset);
}