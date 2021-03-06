#include "sound.h"

#include "streamingaudio.h"
#include "string.h"
#include "tonccpy.h"
#include "myDma.h"
#include "rvidHeader.h"
#include <algorithm>

#define MSL_NSONGS		0
#define MSL_NSAMPS		7
#define MSL_BANKSIZE	7


extern volatile s16 fade_counter;
extern volatile bool fade_out;

extern volatile s16* play_stream_buf;
extern volatile s16* fill_stream_buf;

// Number of samples filled into the fill buffer so far.
extern volatile s32 filled_samples;

extern volatile bool fill_requested;
extern volatile s32 samples_left_until_next_fill;
extern volatile s32 streaming_buf_ptr;

#define SAMPLES_USED (STREAMING_BUF_LENGTH - samples_left)
#define REFILL_THRESHOLD STREAMING_BUF_LENGTH >> 2

#ifdef SOUND_DEBUG
extern char debug_buf[256];
#endif

extern volatile u32 sample_delay_count;

extern u8* frameSoundBufferExtended;

static bool sndInited = false;
bool streamFound = false;
bool streamInRam = false;
extern u32 rvidSizeAllowed;
extern bool extendedMemory;
extern bool rvidInRam;

u32 soundSize = 0;

int positionInSoundFile = 0;

mm_word SOUNDBANK[MSL_BANKSIZE] = {0};

SoundControl::SoundControl()
	: stream_is_playing(false), stream_source(NULL), startup_sample_length(0)
 {

	sys.mod_count = MSL_NSONGS;
	sys.samp_count = MSL_NSAMPS;
	sys.mem_bank = SOUNDBANK;
	sys.fifo_channel = FIFO_MAXMOD;

	mmInit(&sys);

	sndInited = true;
}

void SoundControl::loadStreamFromRvid(const char* filename) {
	if (stream_source) fclose(stream_source);

	if (!sndInited) return;

	stream_source = fopen(filename, "rb");
	
	if (!rvidHasSound) {
		streamFound = false;
		fclose(stream_source);
		return;
	}

	resetStreamSettings();

	fseek(stream_source, 0, SEEK_END);
	soundSize = ftell(stream_source);					// Get sound stream size
	soundSize -= rvidSoundOffset;						// Fix size
	fseek(stream_source, rvidSoundOffset, SEEK_SET);

	stream.sampling_rate = rvidSampleRate;
	stream.buffer_length = 800;	  			// should be adequate
	stream.callback = on_stream_request;    
	stream.format = MM_STREAM_16BIT_MONO;  // select format
	stream.timer = MM_TIMER0;	    	   // use timer0
	stream.manual = false;	      		   // auto filling
	streamFound = true;
	streamInRam = false;
	positionInSoundFile = STREAMING_BUF_LENGTH*2;
	
	if (!rvidInRam && extendedMemory) {
	if (soundSize <= rvidSizeAllowed) {
		// Load sound stream into RAM
		fread(frameSoundBufferExtended, 1, soundSize, stream_source);

		// Prep the first section of the stream
		tonccpy((void*)play_stream_buf, (s16*)frameSoundBufferExtended, STREAMING_BUF_LENGTH*sizeof(s16));

		// Fill the next section premptively
		tonccpy((void*)fill_stream_buf, (s16*)frameSoundBufferExtended+STREAMING_BUF_LENGTH, STREAMING_BUF_LENGTH*sizeof(s16));

		streamInRam = true;

		return;
	}
	}

	if (rvidInRam || !streamInRam) {
		// Prep the first section of the stream
		fread((void*)play_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, stream_source);

		// Fill the next section premptively
		fread((void*)fill_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, stream_source);
	}

}

void SoundControl::beginStream() {
	if (!streamFound || stream_is_playing) return;

	// open the stream
	stream_is_playing = true;
	mmStreamOpen(&stream);
	SetYtrigger(0);
}

void SoundControl::stopStream() {
	if (!streamFound || !stream_is_playing) return;

	stream_is_playing = false;
	mmStreamClose();
}

void SoundControl::resetStream() {
	if (!streamFound) return;

	resetStreamSettings();
	positionInSoundFile = STREAMING_BUF_LENGTH*2;

	if (streamInRam) {
		// Prep the first section of the stream
		tonccpy((void*)play_stream_buf, (s16*)frameSoundBufferExtended, STREAMING_BUF_LENGTH*sizeof(s16));

		// Fill the next section premptively
		tonccpy((void*)fill_stream_buf, (s16*)frameSoundBufferExtended+STREAMING_BUF_LENGTH, STREAMING_BUF_LENGTH*sizeof(s16));

		return;
	}

	fseek(stream_source, rvidSoundOffset, SEEK_SET);

	if (rvidInRam || !streamInRam) {
		// Prep the first section of the stream
		fread((void*)play_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, stream_source);

		// Fill the next section premptively
		fread((void*)fill_stream_buf, sizeof(s16), STREAMING_BUF_LENGTH, stream_source);
	}
}

void SoundControl::fadeOutStream() {
	fade_out = true;
}

void SoundControl::cancelFadeOutStream() {
	fade_out = false;
	fade_counter = FADE_STEPS;
}

void SoundControl::setStreamDelay(u32 delay) {
	sample_delay_count = delay;
}


// Samples remaining in the fill buffer.
#define SAMPLES_LEFT_TO_FILL (abs(STREAMING_BUF_LENGTH - filled_samples))

// Samples that were already streamed and need to be refilled into the buffer.
#define SAMPLES_TO_FILL (abs(streaming_buf_ptr - filled_samples))

// Updates the background music fill buffer
// Fill the amount of samples that were used up between the
// last fill request and this.

// Precondition Invariants:
// filled_samples <= STREAMING_BUF_LENGTH
// filled_samples <= streaming_buf_ptr

// Postcondition Invariants:
// filled_samples <= STREAMING_BUF_LENGTH
// filled_samples <= streaming_buf_ptr
// fill_requested == false
volatile void SoundControl::updateStream() {
	
	if (!stream_is_playing) return;
	if (fill_requested && filled_samples < STREAMING_BUF_LENGTH) {
			
		// Reset the fill request
		fill_requested = false;
		int instance_filled = 0;

		// Either fill the max amount, or fill up the buffer as much as possible.
		int instance_to_fill = std::min(SAMPLES_LEFT_TO_FILL, SAMPLES_TO_FILL);

		// If we don't read enough samples, stop.
		if (streamInRam) {
			tonccpy((s16*)fill_stream_buf + filled_samples, (s16*)frameSoundBufferExtended+positionInSoundFile, instance_to_fill*sizeof(s16));
			instance_filled = instance_to_fill;
			positionInSoundFile += instance_to_fill;
		} else {
			instance_filled = fread((s16*)fill_stream_buf + filled_samples, sizeof(s16), instance_to_fill, stream_source);
		}
		if (instance_filled < instance_to_fill) {
			instance_filled++;
			toncset((s16*)fill_stream_buf + filled_samples + instance_filled, 0, (instance_to_fill - instance_filled));
		}

		#ifdef SOUND_DEBUG
		sprintf(debug_buf, "FC: SAMPLES_LEFT_TO_FILL: %li, SAMPLES_TO_FILL: %li, instance_filled: %i, filled_samples %li, to_fill: %i", SAMPLES_LEFT_TO_FILL, SAMPLES_TO_FILL, instance_filled, filled_samples, instance_to_fill);
		nocashMessage(debug_buf);
		#endif

		// maintain invariant 0 < filled_samples <= STREAMING_BUF_LENGTH
		filled_samples = std::min<s32>(filled_samples + instance_filled, STREAMING_BUF_LENGTH);
	
	} else if (fill_requested && filled_samples >= STREAMING_BUF_LENGTH) {
		// filled_samples == STREAMING_BUF_LENGTH is the only possible case
		// but we'll keep it at gte to be safe.
		filled_samples = 0;
		// fill_count = 0;
	}

}
