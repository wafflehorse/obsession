void setup_sound_from_wav(SoundType type, const char* rel_file_path, float default_volume, Sound* sounds, Arena* arena) {
    WavFile wav_file;
    w_read_wav_file(rel_file_path, &wav_file, arena);
    Sound* sound = &sounds[type];
    sound->type = type;
    sound->num_variations = 1;
    sound->variations[0].samples = wav_file.samples;
    sound->variations[0].sample_count = wav_file.num_sample_bytes / (wav_file.fmt_chunk->bits_per_sample / 8);
    sound->default_volume = default_volume;
}

void add_sound_variant(SoundType type, const char* rel_file_path, GameState* game_state) {
    WavFile wav_file;
    w_read_wav_file(rel_file_path, &wav_file, &game_state->main_arena);
    Sound* sound = &game_state->sounds[type];

    ASSERT(sound != NULL, "Sound with this type has not been setup yet");
    ASSERT(sound->num_variations < MAX_SOUND_VARIATIONS, "MAX_SOUND_VARIATIONS has been exceeded");

    sound->variations[sound->num_variations].samples = wav_file.samples;
    sound->variations[sound->num_variations].sample_count = wav_file.num_sample_bytes / (wav_file.fmt_chunk->bits_per_sample / 8);
    sound->num_variations++;
}

void play_sound(Sound* sound, uint8 variant, float volume, AudioPlayer* audio_player) {
    int i = 0;
    while (audio_player->buffers[i].samples != NULL && i < MAX_PLAYED_SOUNDS) {
        i++;
    }

    ASSERT(i < MAX_PLAYED_SOUNDS, "MAX_PLAYED_SOUNDS has been reached");

    AudioSampleBuffer* buffer = &audio_player->buffers[i];
    buffer->samples = sound->variations[variant].samples;
    buffer->sample_count = sound->variations[variant].sample_count;
    buffer->volume = volume;
	buffer->sound_type = sound->type;
	buffer->samples_read = 0;
}

// TODO: This isn't great as it only works on sound type
void stop_sound(SoundType type, AudioPlayer* audio_player) {
	for(int i = 0; i < MAX_PLAYED_SOUNDS; i++) {
		AudioSampleBuffer* buffer = &audio_player->buffers[i];		
		if(buffer->sound_type == type) {
			buffer->samples = NULL;
		}
	}
}

void play_sound_rand(Sound* sound, AudioPlayer* audio_player) {
    play_sound(sound, rand() % sound->num_variations, sound->default_volume, audio_player);
}

void play_sound(Sound* sound, AudioPlayer* audio_player) {
    play_sound(sound, 0, sound->default_volume, audio_player);
}

bool is_sound_playing(SoundType type, AudioPlayer* audio_player) {
	for(int i = 0; i < MAX_PLAYED_SOUNDS; i++) {
	 	AudioSampleBuffer* buffer = &audio_player->buffers[i];
	 	if(buffer->sound_type == type) {
			return true;
		}
	}
	return false;
}
