#pragma once
#ifndef __TWILIGHTMENU_SOUND__
#define __TWILIGHTMENU_SOUND__
#include <nds.h>
#include <mm_types.h>
#include <maxmod9.h>
#include "singleton.h"
#include <cstdio>

/*
 * Handles playing sound effects and the streaming background music control.
 * See streamingaudio.c for a technical overview of how streaming works.
 */
class SoundControl {
    public:
        SoundControl();

        // Refill the stream buffers
        volatile void updateStream();

        void beginStream();
        void stopStream();
        void fadeOutStream();
        void cancelFadeOutStream();

        // Sets the number of samples of silence to
        // stream before continuing.
        void setStreamDelay(u32 stream_delay);
        
        u32 getStartupSoundLength() { return startup_sample_length; }
      
    private:
        mm_stream stream;
        mm_ds_system sys;
        bool stream_is_playing;
        FILE* stream_source;
        u32 startup_sample_length;
};

typedef singleton<SoundControl> soundCtl_s;
inline SoundControl &snd() { return soundCtl_s::instance(); }

#endif