#define MAX_SOUND_VARIATIONS 10
#define MAX_ENTITIES 1000

#define GAME_STATE_F_INITIALIZED (1 << 0)

#define BASE_RESOLUTION_WIDTH 640 
#define BASE_RESOLUTION_HEIGHT 360 
#define BASE_PIXELS_PER_UNIT 16.0f

#define MAX_PROJECTILE_DISTANCE 80

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
	SOUND_BASIC_GUN_SHOT,
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
	RENDER_GROUP_ID_BACKGROUND,
	RENDER_GROUP_ID_MAIN,
	RENDER_GROUP_ID_COUNT
};

struct CameraShake {
	float magnitude;
	float timer_s;
	float frequency;
	float duration_s;
	float seed;
};

struct Camera {
	Vec2 position;
	Vec2 size;
	CameraShake shake;
};

enum ColliderShape {
	COLLIDER_SHAPE_RECT,
};

struct Collider {
	ColliderShape shape;
	Vec2 offset;
	float width;
	float height;
};

struct EntityLookup {
	uint32 idx;
	uint32 generation;
};

struct EntityHandle {
	uint32 id;
	uint32 generation;
};

enum EntityType {
	ENTITY_TYPE_PLAYER,
	ENTITY_TYPE_GUN,
	ENTITY_TYPE_WARRIOR,
	ENTITY_TYPE_PROJECTILE
};

#define ENTITY_FLAG_MARK_FOR_DELETION (1 << 0)
#define ENTITY_FLAG_EQUIPPED (1 << 1)
#define ENTITY_FLAG_OWNED (1 << 2)
#define ENTITY_FLAG_SPRITE_FLIP_X (1 << 3)
#define ENTITY_FLAG_HAS_COLLIDER (1 << 4)

struct Entity {
	flags flags;

	EntityType type;
	uint32 id;

	Vec2 position;
	Vec2 velocity;
	Vec2 acceleration;
	float rotation_rads;

	AnimationState anim_state;
	SpriteID sprite_id;
	float z_index;

	Vec2 facing_direction;
	EntityHandle owner_handle;

	// projectile
	float distance_traveled;

	Collider collider;
};

struct EntityData {
	Entity entities[MAX_ENTITIES];
	uint32 entity_count;
	uint32 entity_ids[MAX_ENTITIES];
	EntityLookup entity_lookups[MAX_ENTITIES];
};

struct GameState {
	flags flags;
	Camera camera;
	AudioPlayer audio_player;
	Sound sounds[SOUND_TYPE_COUNT];
	Arena main_arena;
	Arena frame_arena;
	FontData font_data;
	uint32 viewport_scale_factor;
	EntityData entity_data;
};
