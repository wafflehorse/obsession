#pragma once
#include "entity.h"

#define SPAWN_CHUNK_DIMENSION 64
#define MAX_SPAWN_TABLE_ENTRIES 64
#define MAX_CHUNK_SPAWN_STATES 64
#define PLAYER_SAFE_DIMENSION 48
#define MAX_SPAWN_GROUP_MEMBERS 8

#define CHUNK_SPAWN_STATE_F_RESOURCES_SPAWNED (1 << 0)

struct ChunkSpawnState {
    flags flags;
    uint32 current_population;
    float respawn_cooldown_s; // determines whether we spawn on tick
    float respawn_timer_s;    // controls tick
    uint32 rng;
};

struct ChunkSpawn {
    ChunkSpawnState states[MAX_CHUNK_SPAWN_STATES];
    uint32 state_count;
    uint32 num_rows;
    uint32 num_cols;
};

struct SpawnEntry {
    EntityType entity_type;
    float weight;
    uint32 group_min;
    uint32 group_max;
};

// struct SpawnGroupMember {
//     EntityType entity_type;
//     uint32 min_count;
//     uint32 max_count;
//     // float weight; // Maybe want this in the future?
// };
//
// struct SpawnGroup {
//     float weight;
//     uint32 min_total;
//     uint32 max_total;
//     SpawnGroupMember members[MAX_SPAWN_GROUP_MEMBERS];
//     uint32 member_count;
// };

struct ResourcePatchSpawnEntry {
    EntityType entity_type;
    float weight;

    uint32 min_patches;
    uint32 max_patches;
    uint32 min_nodes_per_patch;
    uint32 max_nodes_per_patch;
    float min_patch_radius;
    float max_patch_radius;
};

struct BiomeSpawnTable {
    SpawnEntry entries[MAX_SPAWN_TABLE_ENTRIES];
    uint32 entry_count;

    ResourcePatchSpawnEntry resource_entries[MAX_SPAWN_TABLE_ENTRIES];
    uint32 resource_entry_count;

    uint32 max_chunk_population;
    float spawn_interval_s;
};

enum BiomeType {
    BIOME_FART,
    BIOME_COUNT,
};
