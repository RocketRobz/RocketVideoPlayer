#ifndef RVIDHEADER_H
#define RVIDHEADER_H

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

extern int rvidFrames;
extern int rvidFps;
extern int rvidVRes;
extern bool rvidInterlaced;
extern bool rvidHasSound;
extern u16 rvidSampleRate;
extern bool rvidCompressed;
extern off_t rvidFramesOffset;
extern off_t rvidSoundOffset;

void readRvidHeader(FILE* rvid);

#endif //RVIDHEADER_H
