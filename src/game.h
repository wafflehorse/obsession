#pragma once
#include "proc_gen.h"
#include "entity.h"

#define MAX_SOUND_VARIATIONS 10
#define MAX_ENTITIES 10000
#define MAX_COLLISION_RULES 512

#define MAX_DECORATIONS 5000

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
#define MAX_HP_PLANT_CORN 5
#define MAX_HP_ROBOT_GATHERER 10

#define MAX_HUNGER_PLAYER 10
#define HUNGER_TICK_COOLDOWN_S 30

#define ITEM_DROP_PICKUP_COOLDOWN_S 1.0f

#define ENTITY_DAMAGE_TAKEN_TINT_COOLDOWN_S 0.5f

#define ATTACK_ID_MAX_IDS 512
#define ATTACK_ID_START (MAX_ENTITIES - 1)
#define ATTACK_ID_LAST (ATTACK_ID_START + ATTACK_ID_MAX_IDS - 1)

#define FOURCC(a, b, c, d) ((uint32)(a) << 24 | (uint32)(b) << 16 | (uint32)(c) << 8 | (uint32)(d))

// #define SEED_IRON_ORE FOURCC('I', 'R', 'O', 'N')
// #define SEED_CORN FOURCC('C', 'O', 'R', 'N')
#define SEED_PLANT FOURCC('P', 'L', 'A', 'N')

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

struct WorldCollider {
    ColliderShape shape;
    Vec2 position;
    Vec2 size;
};

struct Collider {
    ColliderShape shape;
    Vec2 offset;
    Vec2 size;
};

struct EntityHandle {
    uint32 id;
    int generation;
};

enum AISearchingState { AI_SEARCHING_STATE_STARTING, AI_SEARCHING_STATE_SEARCHING, AI_SEARCHING_STATE_RETURNING };

enum AIState {
    AI_STATE_IDLE,
    AI_STATE_WANDER,
    AI_STATE_CHASE,
    AI_STATE_ATTACK,
    AI_STATE_DEAD,
    AI_STATE_SEARCHING,
    AI_STATE_HARVESTING
};

enum BrainType { BRAIN_TYPE_NONE, BRAIN_TYPE_BOAR, BRAIN_TYPE_WARRIOR, BRAIN_TYPE_ROBOT_GATHERER };

#define BRAIN_F_SEARCHING_INITIALIZED (1 << 0)

#define BRAIN_HARVESTING_COOLDOWN_S 1.0f;

struct Brain {
    flags flags;
    BrainType type;
    AIState ai_state;
    float cooldown_s;
    EntityHandle target_handle;
    Vec2 target_position;

    AISearchingState searching_state;
    Vec2 searching_direction;
    Vec2 searching_target_position;
    float harvesting_cooldown_s;
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

struct InventoryItem {
    EntityType entity_type;
    EntityHandle entity_handle;
    uint32 stack_size;
};

struct Inventory {
    InventoryItem items[ENTITY_MAX_INVENTORY_SIZE];
    uint32 row_count;
    uint32 col_count;
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
#define ENTITY_F_GETS_HUNGERY (1 << 10)
#define ENTITY_F_COLLECTS_ITEMS (1 << 11)

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

    AnimationState anim_state;
    SpriteID sprite_id;
    float z_index;

    Vec2 facing_direction;
    EntityHandle owner_handle;

    // projectile
    float distance_traveled;

    uint32 hp;
    uint32 hunger;
    float hunger_cooldown_s;
    float damage_taken_tint_cooldown_s;
    float item_drop_pickup_cooldown_s;
    uint32 attack_id;

    float item_floating_anim_timer_s;

    // item spawning
    float damage_since_spawn;

    Inventory inventory;

    uint32 stack_size;

    Brain brain;
};

struct EntityData {
    Entity entities[MAX_ENTITIES];
    uint32 entity_count;
    uint32 entity_ids[MAX_ENTITIES];
    EntityLookup entity_lookups[MAX_ENTITIES];
};

struct HotBar {
    Inventory inventory;
    uint32 active_item_idx;
};

struct EntityItemSpawnInfo {
    EntityType spawned_entity_type;
    float damage_required_to_spawn;
    float spawn_chance;
};

struct WorldGenContext {
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
    bool disable_hunger;
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

enum UIState {
    UI_STATE_NONE,
    UI_STATE_PLAYER_INVENTORY,
    UI_STATE_ENTITY_UI,
    UI_STATE_STRUCTURE_PLACEMENT,
    UI_STATE_COUNT
};

#define UI_MODE_F_INVENTORY_ACTIVE (1 << 0)
#define UI_MODE_F_CAMERA_OVERRIDE (1 << 1)

struct UIMode {
    flags flags;
    UIState state;
    EntityHandle entity_handle;

    // For UI_STATE_STRUCTURE_PLACEMENT
    EntityType placing_structure_type;
    Vec2 camera_position;
    float camera_zoom;
};

#define GAME_STATE_F_INITIALIZED (1 << 0)

struct GameState {
    flags flags;
    uint32 frame_flags;
    GameInit game_init_config;
    WorldInit world_init;
    uint32 world_seed;
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
    EntityItemSpawnInfo entity_item_spawn_info[ENTITY_TYPE_COUNT];
    uint32 attack_id_next;
    WorldGenContext world_gen_context;
    DecorationData decoration_data;
    TextureInfo sprite_texture_info;
    TextureInfo font_texture_info;
    ChunkSpawn chunk_spawn;
    uint32 chunk_spawn_state_count;
    UIMode ui_mode;
    Tools tools;
};
