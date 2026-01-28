#ifndef RVIDHEADER_H
#define RVIDHEADER_H

#define latestRvidVer 4

typedef struct rvidHeaderCheckInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
} rvidHeaderCheckInfo;

typedef struct rvidHeaderInfo1 {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
	u8 hasSound;		// Has sound/audio
	u8 reserved;
	u16 sampleRate;		// Audio sample rate
} rvidHeaderInfo1;

typedef struct rvidHeaderInfo2 {
	u32 formatString;		// "RVID" string
	u32 ver;				// File format version
	u32 frames;				// Number of frames
	u8 fps;					// Frames per second
	u8 vRes;				// Vertical resolution
	u8 interlaced;			// Is interlaced
	u8 hasSound;			// Has sound/audio
	u16 sampleRate;			// Audio sample rate
	u16 framesCompressed;	// Frames are compressed
	u32 framesOffset;		// Offset of first frame
	u32 soundOffset;		// Offset of sound stream
} rvidHeaderInfo2;

typedef struct rvidHeaderInfo4 {
	u32 formatString;		// "RVID" string
	u32 ver;				// File format version
	u32 frames;				// Number of frames
	u8 fps;					// Frames per second
	u8 vRes;				// Vertical resolution
	u8 interlaced;			// Is interlaced
	u8 dualScreen;			// Is dual screen video
	u16 sampleRate;			// Audio sample rate
	u8 audioBitMode;		// 0 = 8-bit, 1 = 16-bit
	u8 bmpMode;				// 0 = 8 BPP (RGB565), 1 = 16 BPP (RGB555), 2 = 16 BPP (RGB565)
	u32 compressedFrameSizeTableOffset;		// Offset of compressed frame size table
	u32 soundLeftOffset;	// Offset of left-side sound stream
	u32 soundRightOffset;	// Offset of right-side sound stream
} rvidHeaderInfo4;

#ifdef __GBA__
extern u32* frameOffsets;

extern void* rvidPos;
extern u32 rvidFramesOffset;
#endif
extern int rvidFrames;
extern int rvidFps;
extern bool rvidReduceFpsBy01;
extern bool rvidNativeRefreshRate;
extern int rvidHRes;
extern int rvidVRes;
extern bool rvidInterlaced;
extern bool rvidDualScreen;
extern int rvidOver256Colors;
extern bool rvidHasSound;
extern u16 rvidSampleRate;
extern bool rvidAudioIs16bit;
extern bool rvidCompressed;
extern u32 rvidCompressedFrameSizeTableOffset;
extern u32 rvidSoundOffset;
extern u32 rvidSoundRightOffset;

void readRvidHeader(
	#ifdef __GBA__
	const void* rvid
	#else
	FILE* rvid
	#endif
);

#endif //RVIDHEADER_H
