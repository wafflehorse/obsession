#pragma once
#include "entity.h"

#define SPAWN_CHUNK_DIMENSION 64
#define MAX_SPAWN_TABLE_ENTRIES 64
#define MAX_CHUNK_SPAWN_STATES 64
#define PLAYER_SAFE_DIMENSION 48

struct ChunkSpawnState {
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

struct BiomeSpawnTable {
    SpawnEntry entries[MAX_SPAWN_TABLE_ENTRIES];
    uint32 entry_count;

    uint32 max_chunk_population;
    float spawn_interval_s;
};

enum BiomeType {
    BIOME_FART,
    BIOME_COUNT,
};
