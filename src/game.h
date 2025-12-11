#pragma once

#define MAX_SOUND_VARIATIONS 10
#define MAX_ENTITIES 10000
#define MAX_COLLISION_RULES 512

#define MAX_DECORATIONS 1000

#define BASE_RESOLUTION_WIDTH 640
#define BASE_RESOLUTION_HEIGHT 360
#define BASE_PIXELS_PER_UNIT 16.0f

#define MAX_PROJECTILE_DISTANCE 80
#define ENTITY_MAX_Z 100

#define ENTITY_MAX_INVENTORY_SIZE 36

#define HOTBAR_MAX_SLOTS 8
#define MAX_ITEM_STACK_SIZE 99

#define ITEM_PICKUP_RANGE 1.5

#define MAX_HP_BOAR 2
#define MAX_HP_PLAYER 10
#define MAX_HP_WARRIOR 4
#define MAX_HP_PLANT_CORN 2

#define ATTACK_ID_MAX_IDS 512
#define ATTACK_ID_START (MAX_ENTITIES - 1)
#define ATTACK_ID_LAST (ATTACK_ID_START + ATTACK_ID_MAX_IDS - 1)

struct FontData {
    uint32 ascent;
    uint32 descent;
    uint32 line_gap;
    float baked_pixel_size;
    Vec2 texture_dimensions;
    stbtt_packedchar char_data[95];
};

enum TextureID { TEXTURE_ID_SPRITE, TEXTURE_ID_FONT };

enum SoundType { SOUND_UNKNOWN, SOUND_BACKGROUND_MUSIC, SOUND_BASIC_GUN_SHOT, SOUND_TYPE_COUNT };

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
    RENDER_GROUP_ID_DECORATIONS,
    RENDER_GROUP_ID_TOOLS,
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
    float zoom;
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

enum AIState { AI_STATE_IDLE, AI_STATE_WANDER, AI_STATE_CHASE, AI_STATE_ATTACK, AI_STATE_DEAD };

enum BrainType { BRAIN_TYPE_NONE, BRAIN_TYPE_BOAR, BRAIN_TYPE_WARRIOR };

struct Brain {
    BrainType type;
    AIState ai_state;
    float cooldown_s;
    uint32 target_id;
    Vec2 target_position;
};

enum DecorationType { DECORATION_TYPE_NONE, DECORATION_TYPE_PLANT };

struct Decoration {
    Vec2 position;
    SpriteID sprite_id;
    DecorationType type;
};

struct DecorationData {
    Decoration decorations[MAX_DECORATIONS];
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
    ENTITY_TYPE_BLOCK,
    ENTITY_TYPE_BOAR,
    ENTITY_TYPE_BOAR_MEAT,
    ENTITY_TYPE_IRON_DEPOSIT,
    ENTITY_TYPE_IRON,
    ENTITY_TYPE_PLANT_CORN,
    ENTITY_TYPE_ITEM_CORN,
    ENTITY_TYPE_CHEST_IRON,
    ENTITY_TYPE_COUNT
};

struct InventoryItem {
    EntityType entity_type;
    EntityHandle entity_handle;
    uint32 stack_size;
};

#define ENTITY_F_MARK_FOR_DELETION (1 << 0)
#define ENTITY_F_OWNED (1 << 1)
#define ENTITY_F_SPRITE_FLIP_X (1 << 2)
#define ENTITY_F_KILLABLE (1 << 3)
#define ENTITY_F_NONSPACIAL (1 << 4)
#define ENTITY_F_BLOCKER (1 << 5)
#define ENTITY_F_DELETE_AFTER_ANIMATION (1 << 6)
#define ENTITY_F_ITEM (1 << 7)
#define ENTITY_F_ITEM_SPAWNING (1 << 8)
#define ENTITY_F_PLAYER_INTERACTABLE (1 << 9)

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

    uint32 hp;
    float damage_taken_tint_cooldown_s;
    uint32 attack_id;

    float item_floating_anim_timer_s;

    // item spawning
    float damage_since_spawn;

    InventoryItem inventory[ENTITY_MAX_INVENTORY_SIZE];
    uint32 inventory_count;
    uint32 inventory_rows;
    uint32 inventory_cols;

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
    HotBarSlot slots[HOTBAR_MAX_SLOTS];
    uint32 active_item_idx;
};

struct EntityItemSpawnInfo {
    EntityType spawned_entity_type;
    float damage_required_to_spawn;
    float spawn_chance;
};

struct WorldGenContext {
    FBMContext ore_fbm_context;
    FBMContext plant_fbm_context;
};

struct WorldInput {
    Vec2 mouse_position_world;
};

#define TOOLS_F_CAPTURING_KEYBOARD_INPUT (1 << 0)
#define TOOLS_F_CAPTURING_MOUSE_INPUT (1 << 1)

struct Tools {
    flags flags;
    bool is_panel_open;
    EntityType selected_entity;
    bool entity_palette_should_add_to_init;
    bool draw_colliders;
    bool draw_entity_positions;
    bool draw_hitboxes;
};

struct GameInit {
    char default_world_init_path[PATH_MAX];
};

struct WorldInitEntity {
    EntityType type;
    Vec2 position;
};

#define MAX_WORLD_ENTITY_INITS (MAX_ENTITIES / 2)

struct WorldInit {
    Vec2 world_size;
    WorldInitEntity entity_inits[MAX_WORLD_ENTITY_INITS];
    uint32 entity_init_count;
};

#define GAME_STATE_F_INITIALIZED (1 << 0)
#define GAME_STATE_F_INVENTORY_OPEN (1 << 1)

struct GameState {
    flags flags;
    uint32 frame_flags;
    GameInit game_init_config;
    WorldInit world_init;
    WorldInput world_input;

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
    HotBar hotbar;
    EntityHandle open_entity_inventory;
    EntityItemSpawnInfo entity_item_spawn_info[ENTITY_TYPE_COUNT];
    uint32 attack_id_next;
    WorldGenContext world_gen_context;
    DecorationData decoration_data;
    TextureInfo sprite_texture_info;
    TextureInfo font_texture_info;
    Tools tools;
};
