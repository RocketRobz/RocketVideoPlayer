#ifndef RVIDHEADER_H
#define RVIDHEADER_H

typedef struct rvidHeaderCheckInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
} rvidHeaderCheckInfo;

/* typedef struct rvidHeaderInfo1 {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
	u8 hasSound;		// Has sound/audio
	u8 reserved;
	u16 sampleRate;		// Audio sample rate
} rvidHeaderInfo1; */

typedef struct rvidHeaderInfo2 {
	u32 formatString;		// "RVID" string
	u32 ver;				// File format version
	u32 frames;				// Number of frames
	u8 fps;					// Frames per second
	u8 vRes;				// Vertical resolution
	u8 interlaced;			// Is interlaced
	u8 dualScreen;			// Is dual screen video
	u16 sampleRate;			// Audio sample rate
	u8 audioBitMode;		// 0 = 8-bit, 1 = 16-bit
	u8 bmpMode;				// 0 = 256 RGB565 colors, 1 = Unlimited RGB555 colors, 2 = Unlimited RGB565 colors
	u32 compressedFrameSizeTableOffset;		// Offset of compressed frame size table
	u32 soundOffset;		// Offset of sound stream
} rvidHeaderInfo2;

extern int rvidFrames;
extern int rvidFps;
extern bool rvidReduceFpsBy01;
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

void readRvidHeader(FILE* rvid);

#endif //RVIDHEADER_H
