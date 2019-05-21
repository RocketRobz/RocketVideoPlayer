#ifndef RVIDHEADER_H
#define RVIDHEADER_H

typedef struct rvidHeaderInfo {
	u32 formatString;	// "RVID" string
	u32 ver;			// File format version
	u32 frames;			// Number of frames
	u8 fps;				// Frames per second
	u8 vRes;			// Vertical resolution
	u8 hasSound;		// Has sound/audio
	u8 reserved;
	u16 sampleRate;		// Audio sample rate
} rvidHeaderInfo;

extern rvidHeaderInfo rvidHeader;

#endif //RVIDHEADER_H
