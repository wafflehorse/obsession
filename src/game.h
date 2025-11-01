#define MAX_SOUND_VARIATIONS 10
#define MAX_ENTITIES 1000
#define MAX_COLLISION_RULES 512

#define GAME_STATE_F_INITIALIZED (1 << 0)

#define BASE_RESOLUTION_WIDTH 640 
#define BASE_RESOLUTION_HEIGHT 360 
#define BASE_PIXELS_PER_UNIT 16.0f

#define MAX_PROJECTILE_DISTANCE 80
#define ENTITY_MAX_Z 100

#define MAX_HOT_BAR_SLOTS 8 
#define MAX_ITEM_STACK_SIZE 99

#define ITEM_PICKUP_RANGE 1.5

#define MAX_HP_BOAR 500.0f

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
	RENDER_GROUP_ID_UI,
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

struct CollisionRule {
	uint32 a_id;
	uint32 b_id;
	bool should_collide;
	CollisionRule* next_rule;
};

enum ColliderShape {
	COLLIDER_SHAPE_UNKNOWN,
	COLLIDER_SHAPE_RECT,
};

struct Collider {
	ColliderShape shape;
	Vec2 offset;
	float width;
	float height;
};

enum AIState {
	AI_STATE_IDLE,
	AI_STATE_WANDER,
	AI_STATE_CHASE,
	AI_STATE_ATTACK,
	AI_STATE_DEAD
};

enum BrainType {
	BRAIN_TYPE_NONE,
	BRAIN_TYPE_BOAR,
	BRAIN_TYPE_WARRIOR
};

struct Brain {
	BrainType type;
	AIState ai_state;
	float cooldown_s;
	uint32 target_id;
	Vec2 target_position;
};

struct EntityLookup {
	uint32 idx;
	int generation;
};

struct EntityHandle {
	uint32 id;
	int generation;
};

enum EntityType {
	ENTITY_TYPE_UNKNOWN,
	ENTITY_TYPE_PLAYER,
	ENTITY_TYPE_GUN,
	ENTITY_TYPE_WARRIOR,
	ENTITY_TYPE_PROJECTILE,
	ENTITY_TYPE_PROP,
	ENTITY_TYPE_BOAR,
	ENTITY_TYPE_BOAR_MEAT,
	ENTITY_TYPE_IRON_DEPOSIT,
	ENTITY_TYPE_IRON,
	ENTITY_TYPE_COUNT
};

#define ENTITY_FLAG_MARK_FOR_DELETION (1 << 0)
#define ENTITY_FLAG_OWNED (1 << 1)
#define ENTITY_FLAG_SPRITE_FLIP_X (1 << 2)
#define ENTITY_FLAG_KILLABLE (1 << 3)
#define ENTITY_FLAG_NONSPACIAL (1 << 4)
#define ENTITY_FLAG_BLOCKER (1 << 5)
#define ENTITY_FLAG_DELETE_AFTER_ANIMATION (1 << 6)
#define ENTITY_FLAG_ITEM (1 << 7)
#define ENTITY_FLAG_ITEM_SPAWNING (1 << 8)
#define ENTITY_FLAG_ITEM_PERSIST_ENTITY (1 << 9)

#define ENTITY_DAMAGE_TAKEN_TINT_COOLDOWN_S 0.25f

struct Entity {
	flags flags;

	EntityType type;
	uint32 id;

	Vec2 position;
	Vec2 velocity;
	Vec2 acceleration;
	float rotation_rads;
	float z_pos;
	float z_velocity;
	float z_acceleration;

	Collider collider;

	AnimationState anim_state;
	SpriteID sprite_id;
	float z_index;

	Vec2 facing_direction;
	EntityHandle owner_handle;

	// projectile
	float distance_traveled;

	float hp;
	float damage_taken_tint_cooldown_s;

	// item spawning
	float damage_since_spawn;

	uint32 stack_size;

	Brain brain;
};

struct EntityData {
	Entity entities[MAX_ENTITIES];
	uint32 entity_count;
	uint32 entity_ids[MAX_ENTITIES];
	EntityLookup entity_lookups[MAX_ENTITIES];
};

struct HotBarSlot {
	EntityType entity_type;
	EntityHandle entity_handle;
	uint32 stack_size;
};

struct HotBar {
	HotBarSlot slots[MAX_HOT_BAR_SLOTS];
	uint32 active_item_idx;
};

struct EntityItemSpawnInfo {
	EntityType spawned_entity_type;
	float damage_required_to_spawn;
	float spawn_chance;
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
	CollisionRule* collision_rule_hash[MAX_COLLISION_RULES];
	CollisionRule* collision_rule_free_list;
	Entity* player;
	HotBar hot_bar;
	EntityItemSpawnInfo entity_item_spawn_info[ENTITY_TYPE_COUNT];
};
