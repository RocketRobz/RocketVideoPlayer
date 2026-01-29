#ifdef __GBA__
#include <gba_types.h>
#else
#include <nds.h>
#endif
#include <stdio.h>
#include "rvidHeader.h"
#ifdef __GBA__
#include "tonccpy.h"
#endif

#ifdef __GBA__
#define width8 240
#define width16 240*2
#else
#define width8 256
#define width16 256*2
#endif

rvidHeaderCheckInfo rvidHeaderCheck;

#ifdef __GBA__
u32* frameOffsets = NULL;
u16* compressedFrameSizes16 = NULL;
u32* compressedFrameSizes32 = NULL;

void* rvidPos = NULL;
u32 rvidFramesOffset = 0x200;
#endif
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

void readRvidHeader(
	#ifdef __GBA__
	const void* rvid
	#else
	FILE* rvid
	#endif
) {
	#ifdef __GBA__
	rvidPos = (void*)rvid;
	tonccpy(&rvidHeaderCheck, rvid, sizeof(rvidHeaderCheck));

	if (rvidHeaderCheck.formatString != 0x44495652) {
		return;
	}
	#else
	u32
	#endif

	rvidFramesOffset = 0x200;

	switch (rvidHeaderCheck.ver) {
		#ifndef __GBA__
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
			rvidHRes = width16;
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
			rvidHRes = width16;
			break;
		case 3:
		#endif
		case 4: {
			rvidHeaderInfo4 rvidHeader4;
			#ifdef __GBA__
			tonccpy(&rvidHeader4, rvid, sizeof(rvidHeader4));
			#else
			fread(&rvidHeader4, 1, sizeof(rvidHeader4), rvid);
			#endif
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
			#ifdef __GBA__
			rvidFramesOffset = ((u32)rvid) + rvidFramesOffset;
			frameOffsets = (u32*)rvidFramesOffset;
			#endif
			rvidCompressedFrameSizeTableOffset = rvidHeader4.compressedFrameSizeTableOffset;
			#ifdef __GBA__
			rvidCompressedFrameSizeTableOffset = ((u32)rvid) + rvidCompressedFrameSizeTableOffset;
			if (rvidOver256Colors) {
				compressedFrameSizes32 = (u32*)rvidCompressedFrameSizeTableOffset;
			} else {
				compressedFrameSizes16 = (u16*)rvidCompressedFrameSizeTableOffset;
			}
			#endif
			rvidSoundOffset = rvidHeader4.soundLeftOffset;
			if (rvidHeaderCheck.ver != 3) {
				rvidSoundRightOffset = rvidHeader4.soundRightOffset;
			} else {
				rvidSoundRightOffset = 0;
			}

			rvidHRes = rvidOver256Colors ? width16 : width8;
			rvidCompressed = (rvidCompressedFrameSizeTableOffset > 0);
			#ifdef __GBA__
			rvidHasSound = rvidSampleRate;
			#else
			if (rvidHeaderCheck.ver != 3) {
				rvidHasSound = rvidSampleRate;
			} else {
				rvidHasSound = (rvidSampleRate && rvidSoundOffset);
			}
			#endif
		}	break;
	}

	#ifdef __GBA__
	if (rvidHasSound) {
		rvidSoundOffset += (u32)rvid;
		if (rvidSoundRightOffset) {
			rvidSoundRightOffset += (u32)rvid;
		}
	}
	#else
	fseek(rvid, rvidFramesOffset, SEEK_SET);
	#endif
}