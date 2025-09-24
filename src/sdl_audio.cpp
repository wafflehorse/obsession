#include <SDL3/SDL.h>
#include "audio.h"

static SDL_AudioStream* sdl_audio_stream;

#define AUDIO_FREQ 44100
#ifdef _WIN32
	#define TARGET_BUFFER_SIZE_BYTES 3528 // ~ 20ms latency
#else
	// I find that on mac, it's not clearing the queue fast enough to handle a 3528 buffer size
	#define TARGET_BUFFER_SIZE_BYTES 5292 // ~ 30ms latency
#endif

INIT_AUDIO(init_audio) {
    SDL_AudioSpec desired_spec = {
        .format = SDL_AUDIO_S16,
        .channels = 2,
        .freq = AUDIO_FREQ,
    };

    sdl_audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &desired_spec, NULL, NULL);

    if (!SDL_ResumeAudioStreamDevice(sdl_audio_stream)) {
        fprintf(stderr, "Failed to start audio stream\n");
        return -1;
    }

    return 0;
}

PUSH_AUDIO_SAMPLES(push_audio_samples) {
    uint32 bytes_in_queue = SDL_GetAudioStreamQueued(sdl_audio_stream);

    int32 bytes_to_write = TARGET_BUFFER_SIZE_BYTES - bytes_in_queue;
    int16 out_buffer[TARGET_BUFFER_SIZE_BYTES / 2] = {};
    uint32 frames_written = 0;
    if (bytes_to_write > 4) {
        for (int i = 0; i < MAX_PLAYED_SOUNDS; i++) {
            AudioSampleBuffer* buffer = &audio_player->buffers[i];

            if (buffer->samples != NULL) {
                uint32 num_frames_to_write = bytes_to_write / 4;
                uint64 frames_remaining = (buffer->sample_count - buffer->samples_read) / 2;

                int16* curr_sample = buffer->samples + buffer->samples_read;

				if (frames_remaining < num_frames_to_write) {
					num_frames_to_write = frames_remaining;
				}

				if(audio_player->isolated_sound == 0 || buffer->sound_type == audio_player->isolated_sound) {
					for (int j = 0; j < num_frames_to_write; j++) {
						out_buffer[2 * j] += (int16)(*curr_sample * buffer->volume);
						curr_sample += 1;
						out_buffer[(2 * j) + 1] += (int16)(*curr_sample * buffer->volume);
						curr_sample += 1;
					}
				}

				if (frames_written < num_frames_to_write) {
					frames_written = num_frames_to_write;
				}

				buffer->samples_read += num_frames_to_write * 2;

				if (buffer->samples_read >= buffer->sample_count) {
					buffer->samples = NULL;
					buffer->samples_read = 0;
					buffer->sample_count = 0;
				}
			}
			else {
				*buffer = {};
			}
		}

		if(frames_written > 0) {
			if (!SDL_PutAudioStreamData(sdl_audio_stream, out_buffer, frames_written * 4)) {
				fprintf(stderr, "Failed to PutAudioStreamData\n");
			}
		}
	}
}
