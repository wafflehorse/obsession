#define MAX_SOUND_VARIATIONS 5
#define MAX_ENTITIES 1000

#define GAME_STATE_F_INITIALIZED (1 << 0)

struct FontData {
    uint32 ascent;
    uint32 descent;
    uint32 line_gap;
    float baked_pixel_size;
    Vec2 texture_dimensions;
    stbtt_packedchar char_data[95];
};

enum TextureID {
	TEXTURE_ID_SPRITE,
	TEXTURE_ID_FONT
};

enum SoundType {
    SOUND_UNKNOWN,
    SOUND_BACKGROUND_MUSIC,
    SOUND_TYPE_COUNT
};

struct SoundSampleData {
    int16* samples;
    uint64 sample_count;
};

struct Sound {
    SoundType type;
    uint8 num_variations;
    SoundSampleData variations[MAX_SOUND_VARIATIONS];
    float default_volume;
};

enum RenderGroupID {
	RENDER_GROUP_ID_DEBUG,
	RENDER_GROUP_ID_COUNT
};

struct Camera {
	Vec2 position;
	Vec2 size;
};

struct GameState {
	flags flags;
	Camera camera;
	AudioPlayer audio_player;
	Sound sounds[SOUND_TYPE_COUNT];
	Arena main_arena;
	Arena frame_arena;
	FontData font_data;
};
