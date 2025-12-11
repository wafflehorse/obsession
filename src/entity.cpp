#include "waffle_lib.h"
#include "game.h"

struct EntityAnimations {
    AnimationID idle;
    AnimationID move;
    AnimationID move_up;
    AnimationID move_down;
    AnimationID death;
    AnimationID attack;
};

#define ENTITY_INFO_F_PERSIST_IN_INVENTORY (1 << 0)
#define ENTITY_INFO_F_PLACEABLE (1 << 1)

struct EntityInfo {
    flags flags;
    EntityAnimations animations;
    SpriteID default_sprite;
    char type_name_string[256];
};

EntityInfo entity_info[ENTITY_TYPE_COUNT] = {};

EntityHandle entity_null_handle = {.generation = -1};

void entity_init() {
    entity_info[ENTITY_TYPE_UNKNOWN] = {
        .type_name_string = "Unknown",
    };

    entity_info[ENTITY_TYPE_PLAYER] = {
        .type_name_string = "Player",
        .animations =
            {
                .idle = ANIM_HERO_IDLE,
                .move = ANIM_HERO_MOVE_LEFT,
                .move_down = ANIM_HERO_MOVE_DOWN,
                .move_up = ANIM_HERO_MOVE_UP,
                .death = ANIM_HERO_DEAD,
            },
        .default_sprite = SPRITE_HERO_IDLE_0,
    };

    entity_info[ENTITY_TYPE_GUN] = {
        .flags = ENTITY_INFO_F_PERSIST_IN_INVENTORY,
        .type_name_string = "Gun",
        .default_sprite = SPRITE_GUN_GREEN,
    };

    entity_info[ENTITY_TYPE_WARRIOR] = {
        .type_name_string = "Warrior",
        .animations =
            {
                .idle = ANIM_WARRIOR_IDLE,
                .move = ANIM_WARRIOR_MOVE_LEFT,
                .death = ANIM_WARRIOR_DEAD,
                .attack = ANIM_WARRIOR_ATTACK,
            },
        .default_sprite = SPRITE_WARRIOR_IDLE_0,
    };

    entity_info[ENTITY_TYPE_PROJECTILE] = {
        .type_name_string = "Projectile",
        .default_sprite = SPRITE_GREEN_BULLET_1,
    };

    entity_info[ENTITY_TYPE_BLOCK] = {
        .type_name_string = "Block",
        .default_sprite = SPRITE_BLOCK_1,
    };

    entity_info[ENTITY_TYPE_BOAR] = {.type_name_string = "Boar",
                                     .animations =
                                         {
                                             .idle = ANIM_BOAR_IDLE,
                                             .move = ANIM_BOAR_WALK,
                                             .death = ANIM_BOAR_2_DEATH,
                                         },
                                     .default_sprite = SPRITE_BOAR_IDLE_0};

    entity_info[ENTITY_TYPE_BOAR_MEAT] = {
        .type_name_string = "Boar Meat",
        .default_sprite = SPRITE_BOAR_MEAT_RAW,
    };

    entity_info[ENTITY_TYPE_IRON_DEPOSIT] = {
        .type_name_string = "Iron Deposit",
        .default_sprite = SPRITE_ORE_IRON_0,
    };

    entity_info[ENTITY_TYPE_IRON] = {
        .type_name_string = "Iron",
        .default_sprite = SPRITE_IRON_1,
    };

    entity_info[ENTITY_TYPE_PLANT_CORN] = {
        .type_name_string = "Corn Plant",
        .default_sprite = SPRITE_PLANT_CORN_3,
    };

    entity_info[ENTITY_TYPE_ITEM_CORN] = {
        .type_name_string = "Corn",
        .default_sprite = SPRITE_ITEM_CORN,
    };

    entity_info[ENTITY_TYPE_CHEST_IRON] = {
        .flags = ENTITY_INFO_F_PLACEABLE,
        .type_name_string = "Iron Chest",
        .default_sprite = SPRITE_CHESTS_IRON_0,
    };
}

Sprite entity_get_default_sprite(EntityType type) {
    return sprite_table[entity_info[type].default_sprite];
}

Entity* entity_new(EntityData* entity_data) {
    uint32 idx = entity_data->entity_count;
    Entity* entity = &entity_data->entities[entity_data->entity_count++];
    memset(entity, 0, sizeof(Entity));

    ASSERT(entity_data->entity_count < MAX_ENTITIES, "MAX_ENTITIES has been reached!");

    entity->id = entity_data->entity_ids[idx];
    entity->z_index = 1;
    entity->facing_direction = {1, 0};
    entity->owner_handle.generation = -1;
    entity->stack_size = 1;

    return entity;
}

Entity* entity_find_first_of_type(EntityData* entity_data, EntityType type) {
    for (int i = 0; i < entity_data->entity_count; i++) {
        if (entity_data->entities[i].type == type) {
            return &entity_data->entities[i];
        }
    }

    return NULL;
}

void entity_free(uint32 id, EntityData* entity_data) {
    EntityLookup* freed_lookup = &entity_data->entity_lookups[id];

    uint32 last_idx = entity_data->entity_count - 1;
    Entity* last_entity = &entity_data->entities[last_idx];
    if (last_entity->id != id) {
        entity_data->entities[freed_lookup->idx] = *last_entity;

        entity_data->entity_ids[freed_lookup->idx] = last_entity->id;
        entity_data->entity_ids[last_idx] = id;

        EntityLookup* last_lookup = &entity_data->entity_lookups[last_entity->id];
        last_lookup->idx = freed_lookup->idx;

        freed_lookup->idx = last_idx;
    }

    freed_lookup->generation++;

    entity_data->entity_count--;
}

Entity* entity_find(EntityHandle handle, EntityData* entity_data) {
    ASSERT(handle.id < MAX_ENTITIES, "Entity handle has id greater than max entities");
    Entity* entity = NULL;

    EntityLookup lookup = entity_data->entity_lookups[handle.id];
    if (lookup.generation == handle.generation) {
        entity = &entity_data->entities[lookup.idx];
    }

    return entity;
}

EntityHandle entity_to_handle(Entity* entity, EntityData* entity_data) {
    EntityHandle handle;

    if (entity) {
        handle.id = entity->id;
        handle.generation = entity_data->entity_lookups[entity->id].generation;
    } else {
        handle.generation = -1;
    }

    return handle;
}

bool entity_same(EntityHandle entity_a, EntityHandle entity_b) {
    return entity_a.id == entity_b.id && entity_a.generation == entity_b.generation;
}

Collider entity_rect_collider_from_sprite(SpriteID sprite_id) {
    Vec2 sprite_world_size = sprite_get_world_size(sprite_id);

    Collider result = {
        .shape = COLLIDER_SHAPE_RECT,
        .offset = {0, sprite_world_size.y / 2},
        .width = sprite_world_size.x,
        .height = sprite_world_size.y,
    };

    return result;
}

EntityHandle entity_create_blocker(EntityData* entity_data, EntityType type, Vec2 position, SpriteID sprite_id) {
    Entity* entity = entity_new(entity_data);

    entity->type = type;
    entity->position = position;
    entity->sprite_id = sprite_id;

    Vec2 sprite_size = sprite_get_world_size(sprite_id);
    Vec2 collider_size = {sprite_size.x, sprite_size.y / 2};

    entity->collider = {.shape = COLLIDER_SHAPE_RECT,
                        .offset = {0, collider_size.y / 2},
                        .width = collider_size.x,
                        .height = collider_size.y};

    set(entity->flags, ENTITY_F_BLOCKER);

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_resource(EntityData* entity_data, EntityType entity_type, Vec2 position, SpriteID sprite_id,
                                    uint32 hp, flags opts) {
    Entity* entity = entity_new(entity_data);

    entity->type = entity_type;
    entity->position = position;
    entity->sprite_id = sprite_id;
    entity->collider = entity_rect_collider_from_sprite(sprite_id);
    set(entity->flags, opts);
    set(entity->flags, ENTITY_F_KILLABLE);
    entity->hp = hp;

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_ore_deposit(EntityData* entity_data, EntityType entity_type, Vec2 position,
                                       SpriteID sprite_id) {
    Entity* entity = entity_new(entity_data);

    ASSERT(entity_type == ENTITY_TYPE_IRON_DEPOSIT, "Ore deposit entity must have supported type");

    entity->type = entity_type;
    entity->position = position;
    entity->sprite_id = sprite_id;
    entity->collider = entity_rect_collider_from_sprite(sprite_id);
    set(entity->flags, ENTITY_F_BLOCKER);
    set(entity->flags, ENTITY_F_KILLABLE);
    entity->hp = 1000000;

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_player(EntityData* entity_data, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_PLAYER;
    entity->position = position;
    entity->facing_direction.x = 1;
    entity->facing_direction.y = 0;
    set(entity->flags, ENTITY_F_KILLABLE);
    entity->hp = MAX_HP_PLAYER;

    Vec2 collider_world_size = {11 / BASE_PIXELS_PER_UNIT, 17 / BASE_PIXELS_PER_UNIT};

    collider_world_size.y *= 0.5f;
    // TODO: think about this collider size
    entity->collider = {
        .shape = COLLIDER_SHAPE_RECT,
        .offset = {0, collider_world_size.y / 2},
        .width = collider_world_size.x,
        .height = collider_world_size.y,
    };

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_chest(EntityData* entity_data, Vec2 position, flags opts) {
    Entity* entity = entity_new(entity_data);

    SpriteID sprite_id = entity_info[ENTITY_TYPE_CHEST_IRON].default_sprite;

    entity->type = ENTITY_TYPE_CHEST_IRON;
    entity->position = position;
    entity->sprite_id = sprite_id;
    entity->inventory.row_count = 4;
    entity->inventory.col_count = 5;

    set(entity->flags, opts);
    set(entity->flags, ENTITY_F_BLOCKER);
    set(entity->flags, ENTITY_F_PLAYER_INTERACTABLE);

    entity->collider = entity_rect_collider_from_sprite(sprite_id);

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_gun(EntityData* entity_data, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_GUN;
    entity->position = position;
    set(entity->flags, ENTITY_F_NONSPACIAL);
    set(entity->flags, ENTITY_F_ITEM);
    entity->sprite_id = SPRITE_GUN_GREEN;

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_boar(EntityData* entity_data, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_BOAR;
    entity->position = position;

    Vec2 sprite_size = sprite_get_world_size(SPRITE_BOAR_IDLE_0);

    set(entity->flags, ENTITY_F_KILLABLE);
    entity->hp = MAX_HP_BOAR;

    entity->collider = {.shape = COLLIDER_SHAPE_RECT,
                        .offset = {0, sprite_size.y / 2},
                        .width = sprite_size.x,
                        .height = sprite_size.y};
    entity->brain.type = BRAIN_TYPE_BOAR;

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_warrior(EntityData* entity_data, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_WARRIOR;
    w_play_animation(ANIM_WARRIOR_IDLE, &entity->anim_state);
    entity->position = position;
    set(entity->flags, ENTITY_F_KILLABLE);
    entity->hp = MAX_HP_WARRIOR;

    Vec2 collider_world_size = {11 / BASE_PIXELS_PER_UNIT, 14 / BASE_PIXELS_PER_UNIT};

    entity->collider = {.shape = COLLIDER_SHAPE_RECT,
                        .offset = {0, collider_world_size.y / 2},
                        .width = collider_world_size.x,
                        .height = collider_world_size.y};
    entity->brain.type = BRAIN_TYPE_WARRIOR;

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_projectile(EntityData* entity_data, Vec2 position, float rotation_rads, Vec2 velocity) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_PROJECTILE;
    entity->sprite_id = SPRITE_GREEN_BULLET_STRETCHED_1;
    entity->position = position;
    entity->rotation_rads = rotation_rads;
    entity->velocity = velocity;
    Sprite sprite = sprite_table[SPRITE_GREEN_BULLET_STRETCHED_1];
    entity->collider = {
        .shape = COLLIDER_SHAPE_RECT,
        .offset = {0, 0},
        .width = sprite.w / BASE_PIXELS_PER_UNIT,
        .height = sprite.h / BASE_PIXELS_PER_UNIT,
    };

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_boar_meat(EntityData* entity_data, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    entity->type = ENTITY_TYPE_BOAR_MEAT;
    entity->sprite_id = SPRITE_BOAR_MEAT_RAW;
    entity->position = position;
    entity->collider = entity_rect_collider_from_sprite(entity->sprite_id);

    set(entity->flags, ENTITY_F_ITEM);
    set(entity->flags, ENTITY_F_NONSPACIAL);

    return entity_to_handle(entity, entity_data);
}

EntityHandle entity_create_item(EntityData* entity_data, EntityType type, Vec2 position) {
    Entity* entity = entity_new(entity_data);

    SpriteID sprite_id = entity_info[type].default_sprite;

    ASSERT(sprite_id != SPRITE_UNKNOWN, "Item entities must have sprite specified in entity_sprites\n");

    entity->type = type;
    entity->sprite_id = sprite_id;
    entity->position = position;
    entity->collider = entity_rect_collider_from_sprite(entity->sprite_id);

    set(entity->flags, ENTITY_F_ITEM);
    set(entity->flags, ENTITY_F_NONSPACIAL);

    return entity_to_handle(entity, entity_data);
}

// ~~~~~~~~~~~~~~~~~~~~~~~ entity utilities ~~~~~~~~~~~~~~~~~~~~~~~ //

void entity_spawn_item(EntityType entity_type, Vec2 source_position, GameState* game_state) {
    EntityHandle entity_handle = entity_create_item(&game_state->entity_data, entity_type, source_position);

    Entity* item = entity_find(entity_handle, &game_state->entity_data);

    ASSERT(item != NULL, "Tried to spawn invalid item entity");

    Vec2 random_point = random_point_near_position(source_position, 1, 1);
    Vec2 random_point_direction = w_vec_sub(random_point, source_position);
    Vec2 velocity_direction = w_vec_norm(random_point_direction);
    float velocity_mag = w_random_between(1.5, 3);

    item->velocity = w_vec_mult(velocity_direction, velocity_mag);
    item->z_pos = 0.0001;
    item->z_velocity = 10;
    item->z_acceleration = -40;

    set(item->flags, ENTITY_F_ITEM_SPAWNING);
}

void entity_deal_damage(Entity* target, float damage, GameState* game_state) {
    target->hp = w_clamp_min(target->hp - damage, 0);
    target->damage_taken_tint_cooldown_s = ENTITY_DAMAGE_TAKEN_TINT_COOLDOWN_S;
    EntityItemSpawnInfo spawn_info = game_state->entity_item_spawn_info[target->type];
    target->damage_since_spawn += damage;
    if (spawn_info.spawned_entity_type != ENTITY_TYPE_UNKNOWN &&
        target->damage_since_spawn >= spawn_info.damage_required_to_spawn) {
        target->damage_since_spawn = 0;
        float spawn_roll = w_random_between(0, 1);
        if (spawn_roll <= spawn_info.spawn_chance) {
            entity_spawn_item(spawn_info.spawned_entity_type, target->position, game_state);
        }
    }
}

Vec2 entity_discrete_facing_direction_4_directions(Vec2 facing_direction) {
    float mag_y_direction = w_abs(facing_direction.y);
    float mag_x_direction = w_abs(facing_direction.x);
    Vec2 result = {};

    if (mag_y_direction > mag_x_direction) {
        if (facing_direction.y > 0) {
            result.y = 1;
        } else {
            result.y = -1;
        }
    } else {
        if (facing_direction.x > 0) {
            result.x = 1;
        } else {
            result.x = -1;
        }
    }

    return result;
}

Vec2 entity_held_item_position(Entity* owner, float* z_pos, float* z_index) {
    *z_pos = 0.5;
    Vec2 result_position;

    if (owner->facing_direction.x < 0) {
        result_position = {owner->position.x - 0.3f, owner->position.y};
    } else {
        result_position = {owner->position.x + 0.3f, owner->position.y};
    }

    Vec2 owner_disc_facing_direction = entity_discrete_facing_direction_4_directions(owner->facing_direction);
    if (owner_disc_facing_direction.y > 0) {
        *z_index = owner->z_index - 0.01;
    } else {
        *z_index = owner->z_index + 0.01;
    }

    return result_position;
}

// ~~~~~~~~~~~~~~~~~~~~~~~~ entity animations ~~~~~~~~~~~~~~~~~~~~~~~~ //

void entity_play_animation_with_direction(AnimationID animation_id, AnimationState* anim_state, Vec2 facing_direction,
                                          uint32 anim_state_opts) {
    if (facing_direction.x > 0) {
        set(anim_state_opts, ANIMATION_STATE_F_FLIP_X);
    }
    w_play_animation(animation_id, anim_state, anim_state_opts);
}

void entity_play_animation_with_direction(AnimationID animation_id, AnimationState* anim_state, Vec2 facing_direction) {
    entity_play_animation_with_direction(animation_id, anim_state, facing_direction, 0);
}

// ~~~~~~~~~~~~~~~~~~~~~~~~ entity rendering ~~~~~~~~~~~~~~~~~~~~~~~~~~~ //

Vec2 entity_sprite_world_position(SpriteID sprite_id, Vec2 entity_position, float z_pos, bool flip_x) {
    Sprite sprite = sprite_table[sprite_id];
    Vec2 sprite_position;

    if (sprite.has_ground_anchor) {
        Vec2 sprite_center_px = {sprite.w / 2, sprite.h / 2};

        Vec2 sprite_anchor_from_center_offset = w_vec_sub(sprite_center_px, sprite.ground_anchor);
        Vec2 anchor_from_center_offset_world = w_vec_mult(sprite_anchor_from_center_offset, 1.0 / BASE_PIXELS_PER_UNIT);
        anchor_from_center_offset_world.y *= -1;

        if (flip_x) {
            anchor_from_center_offset_world.x *= -1;
        }

        sprite_position = w_vec_add(entity_position, anchor_from_center_offset_world);
        sprite_position.y += z_pos;
    } else {
        Vec2 sprite_world_size = w_vec_mult((Vec2){sprite.w, sprite.h}, 1.0 / BASE_PIXELS_PER_UNIT);
        sprite_position = {entity_position.x, entity_position.y + (sprite_world_size.y / 2) + z_pos};
    }

    return sprite_position;
}

RenderQuad* entity_render(Entity* entity, RenderGroup* render_group) {
    ASSERT(entity->sprite_id != SPRITE_UNKNOWN || entity->anim_state.animation_id != ANIM_UNKNOWN,
           "Cannot determine entity sprite");

    flags opts = {};
    SpriteID sprite_id;
    RenderQuad* quad;
    if (entity->anim_state.animation_id != ANIM_UNKNOWN) {
        // TODO: can we get rid of this and use the ENTITY_F_SPRITE_FLIP_X
        if (is_set(entity->anim_state.flags, ANIMATION_STATE_F_FLIP_X)) {
            set(opts, RENDER_SPRITE_OPT_FLIP_X);
        }
        // TODO: might want to create util methods for this with assertions and bounds checking?
        Animation animation = animation_table[entity->anim_state.animation_id];
        sprite_id = animation.frames[entity->anim_state.current_frame].sprite_id;
    } else {
        if (is_set(entity->flags, ENTITY_F_SPRITE_FLIP_X)) {
            set(opts, RENDER_SPRITE_OPT_FLIP_X);
        }
        sprite_id = entity->sprite_id;
    }

    Vec2 sprite_position = entity_sprite_world_position(sprite_id, entity->position, entity->z_pos,
                                                        is_set(opts, RENDER_SPRITE_OPT_FLIP_X));

    quad = render_sprite(sprite_position, sprite_id, render_group,
                         {.rotation_rads = entity->rotation_rads, .z_index = entity->z_index, .opts = opts});

    if (entity->damage_taken_tint_cooldown_s > 0) {
        float normalized_elapsed =
            1 - w_clamp_01(entity->damage_taken_tint_cooldown_s / ENTITY_DAMAGE_TAKEN_TINT_COOLDOWN_S);

        float tint_factor = (1 - w_anim_ease_out_quad(normalized_elapsed)) * 5;
        quad->tint = {1 + tint_factor, 1 + tint_factor, 1 + tint_factor, 1};
    }

    return quad;
}

void entity_player_movement_animation_update(Entity* entity, Vec2 player_movement_vec) {
    EntityAnimations animations = entity_info[entity->type].animations;
    Vec2 disc_facing_direction = entity_discrete_facing_direction_4_directions(entity->facing_direction);

    if (w_vec_length(player_movement_vec) > 0) {
        if (disc_facing_direction.x != 0) {
            entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
        } else if (disc_facing_direction.y > 0) {
            w_play_animation(animations.move_up, &entity->anim_state);
        } else {
            w_play_animation(animations.move_down, &entity->anim_state);
        }
    } else {
        entity_play_animation_with_direction(animations.idle, &entity->anim_state, entity->facing_direction);
    }
}

#define ENTITY_CREATE_F_ITEM (1 << 0)

EntityHandle entity_create(EntityData* entity_data, EntityType type, Vec2 position, flags opts) {
    EntityHandle entity_handle;
    switch (type) {
    case ENTITY_TYPE_GUN:
        entity_create_gun(entity_data, position);
        break;
    case ENTITY_TYPE_WARRIOR:
        entity_create_warrior(entity_data, position);
        break;
    case ENTITY_TYPE_BLOCK:
        entity_create_blocker(entity_data, ENTITY_TYPE_BLOCK, position, SPRITE_BLOCK_1);
        break;
    case ENTITY_TYPE_BOAR:
        entity_create_boar(entity_data, position);
        break;
    case ENTITY_TYPE_BOAR_MEAT:
        entity_create_boar_meat(entity_data, position);
        break;
    case ENTITY_TYPE_IRON_DEPOSIT:
        entity_create_ore_deposit(entity_data, ENTITY_TYPE_IRON_DEPOSIT, position, SPRITE_ORE_IRON_0);
        break;
    case ENTITY_TYPE_PLANT_CORN:
        entity_create_resource(entity_data, ENTITY_TYPE_PLANT_CORN, position, SPRITE_PLANT_CORN_3, MAX_HP_PLANT_CORN,
                               0);
        break;
    case ENTITY_TYPE_IRON:
        entity_create_item(entity_data, ENTITY_TYPE_IRON, position);
        break;
    case ENTITY_TYPE_CHEST_IRON:
        if (is_set(opts, ENTITY_CREATE_F_ITEM)) {
            entity_create_item(entity_data, type, position);
        } else {
            entity_create_chest(entity_data, position, opts);
        }
        break;
    case ENTITY_TYPE_PLAYER: {
        bool player_exists = false;
        for (int i = 0; i < entity_data->entity_count; i++) {
            if (entity_data->entities[i].type == ENTITY_TYPE_PLAYER) {
                player_exists = true;
            }
        }

        if (!player_exists) {
            entity_create_player(entity_data, position);
        }
        break;
    }
    default:
        // TODO: this is probably a debug only function, so this assertion is kind of annoying
        // cause it will force me to not call this with ineligible entities. So commenting out
        // for now
        // ASSERT(false, "entity_create does not support this entity type");
        break;
    }

    return entity_handle;
}

EntityHandle entity_create(EntityData* entity_data, EntityType type, Vec2 position) {
    return entity_create(entity_data, type, position, 0);
}

Entity* entity_closest_player_interactable(EntityData* entity_data, Entity* player) {
    Entity* closest_interactable_entity = NULL;
    float closest_distance = 1.1;
    for (int i = 0; i < entity_data->entity_count; i++) {
        Entity* entity = &entity_data->entities[i];
        if (is_set(entity->flags, ENTITY_F_PLAYER_INTERACTABLE)) {
            float distance = w_euclid_dist(player->position, entity->position);
            if (distance <= closest_distance) {
                closest_interactable_entity = entity;
                closest_distance = distance;
            }
        }
    }

    return closest_interactable_entity;
}
