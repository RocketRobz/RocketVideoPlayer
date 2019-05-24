#include <nds.h>
#include <stdio.h>
#include "rvidHeader.h"

rvidHeaderCheckInfo rvidHeaderCheck;
rvidHeaderInfo1 rvidHeader1;
rvidHeaderInfo2 rvidHeader2;

int rvidFrames = 0;
int rvidFps = 0;
int rvidVRes = 0;
bool rvidInterlaced = false;
bool rvidHasSound = false;
u16 rvidSampleRate = 0;

void readRvidHeader(FILE* rvid) {
	fseek(rvid, 0, SEEK_SET);
	switch (rvidHeaderCheck.ver) {
		case 1:
			fread(&rvidHeader1, 1, sizeof(rvidHeader1), rvid);
			rvidFrames = rvidHeader1.frames;
			rvidFps = rvidHeader1.fps;
			rvidVRes = rvidHeader1.vRes;
			rvidInterlaced = false;
			rvidHasSound = rvidHeader1.hasSound;
			rvidSampleRate = rvidHeader1.sampleRate;
			break;
		case 2:
			fread(&rvidHeader2, 1, sizeof(rvidHeader2), rvid);
			rvidFrames = rvidHeader2.frames;
			rvidFps = rvidHeader2.fps;
			rvidVRes = rvidHeader2.vRes;
			rvidInterlaced = rvidHeader2.interlaced;
			rvidHasSound = rvidHeader2.hasSound;
			rvidSampleRate = rvidHeader2.sampleRate;
			break;
	}
}