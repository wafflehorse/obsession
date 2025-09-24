#pragma once

#define MAX_PLAYED_SOUNDS 10

struct AudioSampleBuffer {
	uint32 sound_type;
    float volume;
    int16* samples;
    uint64 sample_count;
    uint64 samples_read;
};

struct AudioPlayer {
    AudioSampleBuffer buffers[MAX_PLAYED_SOUNDS];
	uint32 isolated_sound;
};

#define INIT_AUDIO(name) int name(AudioPlayer* audio_player)
typedef INIT_AUDIO(InitAudio);

#define PUSH_AUDIO_SAMPLES(name) void name(AudioPlayer* audio_player)
typedef PUSH_AUDIO_SAMPLES(PushAudioSamples);
