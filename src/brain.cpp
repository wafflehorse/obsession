void brain_move_towards_target(Entity* entity, float velocity_mag) {
    EntityAnimations animations = entity_info[entity->type].animations;

    Vec2 direction = w_vec_norm(w_vec_sub(entity->brain.target_position, entity->position));
    entity->velocity = w_vec_mult(direction, velocity_mag);
    entity->facing_direction = w_vec_norm(entity->velocity);
    entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
}

void brain_idle(Entity* entity) {
    Brain* brain = &entity->brain;
    EntityAnimations animations = entity_info[entity->type].animations;
    if (brain->cooldown_s <= 0) {
        brain->target_position = random_point_near_position(entity->position, 5, 5);
        brain->cooldown_s = 10;
        brain->ai_state = AI_STATE_WANDER;
    }
    entity->velocity = {0, 0};
    entity_play_animation_with_direction(animations.idle, &entity->anim_state, entity->facing_direction);
}

void brain_wander(Entity* entity) {
    Brain* brain = &entity->brain;
    if (w_euclid_dist(entity->position, brain->target_position) <= 1.0f || brain->cooldown_s <= 0) {
        brain->ai_state = AI_STATE_IDLE;
        brain->cooldown_s = w_random_between(1, 6);
    }

    brain_move_towards_target(entity, 1.0f);
}

void brain_dead(Entity* entity) {
    EntityAnimations animations = entity_info[entity->type].animations;

    entity->velocity = {0, 0};
    set(entity->flags, ENTITY_F_NONSPACIAL);
    set(entity->flags, ENTITY_F_DELETE_AFTER_ANIMATION);
    if (animations.death != ANIM_UNKNOWN) {
        entity_play_animation_with_direction(animations.death, &entity->anim_state, entity->facing_direction);
    }
}

void brain_update_warrior(Entity* entity, GameState* game_state, float dt_s) {
    Brain* brain = &entity->brain;
    Entity* player = game_state->player;
    float distance_to_player = w_euclid_dist(entity->position, player->position);
    if (distance_to_player < 5 && brain->ai_state != AI_STATE_ATTACK && brain->ai_state != AI_STATE_DEAD) {
        brain->ai_state = AI_STATE_CHASE;
    }
    EntityAnimations animations = entity_info[entity->type].animations;

    switch (brain->ai_state) {
    case AI_STATE_IDLE:
        brain_idle(entity);
        break;
    case AI_STATE_WANDER: {
        brain_wander(entity);
        break;
    }
    case AI_STATE_CHASE:
        if (distance_to_player >= 5) {
            brain->ai_state = AI_STATE_IDLE;
            brain->cooldown_s = w_random_between(1, 6);
        } else if (distance_to_player < 0.5) {
            brain->ai_state = AI_STATE_ATTACK;
        } else {
            brain->target_position = player->position;
            brain_move_towards_target(entity, 5.0f);
        }
        entity->facing_direction = w_vec_norm(entity->velocity);
        entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
        break;
    case AI_STATE_ATTACK: {
        entity->velocity = {0, 0};
        if (player->position.x < entity->position.x) {
            entity->facing_direction.x = -1;
        } else {
            entity->facing_direction.x = 1;
        }
        entity_play_animation_with_direction(animations.attack, &entity->anim_state, entity->facing_direction);
        if (entity->attack_id == 0) {
            entity->attack_id = get_next_attack_id(&game_state->attack_id_next);
        }
        if (w_animation_complete(&entity->anim_state, dt_s)) {
            entity->attack_id = 0;
            remove_collision_rules(entity->attack_id, game_state->collision_rule_hash,
                                   &game_state->collision_rule_free_list);
            brain->ai_state = AI_STATE_CHASE;
            brain->cooldown_s = w_random_between(1, 6);
        }
        break;
    }
    case AI_STATE_DEAD:
        brain_dead(entity);
        break;
    default:
        break;
    }
}

void brain_update_boar(Entity* entity, GameState* game_state, float dt_s) {
    Brain* brain = &entity->brain;
    switch (brain->ai_state) {
    case AI_STATE_IDLE:
        brain_idle(entity);
        break;
    case AI_STATE_WANDER: {
        brain_wander(entity);
        break;
    }
    case AI_STATE_DEAD:
        brain_dead(entity);
        break;
    default:
        break;
    }
}

void brain_update_robot_gatherer(Entity* entity, GameState* game_state, double dt_s) {
    Brain* brain = &entity->brain;
    switch (brain->ai_state) {
    case AI_STATE_IDLE:
        brain_idle(entity);
        break;
    case AI_STATE_DEAD:
        brain_dead(entity);
        break;
    case AI_STATE_SEARCHING: {
        float vision_radius = 4;
        float searching_dimension = 32;
        Vec2 search_center = {0, 0};
        Vec2 search_bounds_x = {search_center.x - searching_dimension, search_center.x + searching_dimension};
        Vec2 search_bounds_y = {search_center.y - searching_dimension, search_center.y + searching_dimension};

        if (!is_set(brain->flags, BRAIN_F_SEARCHING_INITIALIZED)) {
            brain->searching_state = AI_SEARCHING_STATE_STARTING;
            set(brain->flags, BRAIN_F_SEARCHING_INITIALIZED);
        }

        brain->target_position = brain->searching_target_position;

        switch (brain->searching_state) {
        case AI_SEARCHING_STATE_STARTING: {
            brain->searching_target_position = {search_bounds_x.x + vision_radius, search_bounds_y.x + vision_radius};
            float distance_to_target = w_euclid_dist(entity->position, brain->target_position);

            if (distance_to_target < 0.1) {
                brain->searching_direction = {1, 0};
                brain->searching_target_position = {search_bounds_x.y - vision_radius,
                                                    search_bounds_y.x + vision_radius};
                brain->searching_state = AI_SEARCHING_STATE_SEARCHING;
            }
            break;
        }
        case AI_SEARCHING_STATE_SEARCHING: {
            float distance_to_target = w_euclid_dist(entity->position, brain->target_position);
            if (brain->searching_direction.y != 0) {
                if (distance_to_target < 0.1) {
                    brain->searching_direction.y = 0;
                    brain->searching_direction.x *= -1;
                    if (brain->searching_direction.x > 0) {
                        brain->searching_target_position.x = search_bounds_x.y - vision_radius;
                    } else {
                        brain->searching_target_position.x = search_bounds_x.x + vision_radius;
                    }
                }
            } else if (brain->searching_direction.x != 0) {
                if (distance_to_target < 0.1) {
                    brain->searching_direction.y = 1;
                    brain->searching_target_position.y += vision_radius * 2;

                    if (brain->searching_target_position.y > search_bounds_y.y) {
                        brain->searching_state = AI_SEARCHING_STATE_RETURNING;
                    }
                }
            }
            break;
        }
        case AI_SEARCHING_STATE_RETURNING: {
            brain->target_position = search_center;
            float distance_to_target = w_euclid_dist(entity->position, brain->target_position);
            if (distance_to_target < 0.1) {
                brain->ai_state = AI_STATE_IDLE;
                unset(brain->flags, BRAIN_F_SEARCHING_INITIALIZED);
            }
            break;
        }
        }

        brain_move_towards_target(entity, 4.0f);

        float min_corn_distance = vision_radius;
        for (int i = 0; i < game_state->entity_data.entity_count; i++) {
            Entity* ent = &game_state->entity_data.entities[i];

            if (ent->type == ENTITY_TYPE_PLANT_CORN) {
                float distance_from_robot = w_euclid_dist(entity->position, ent->position);
                if (distance_from_robot < min_corn_distance) {
                    min_corn_distance = distance_from_robot;
                    brain->target_handle = entity_to_handle(ent, &game_state->entity_data);
                    brain->ai_state = AI_STATE_HARVESTING;
                }
            }
        }

        break;
    }
    case AI_STATE_HARVESTING: {
        Entity* target = entity_find(brain->target_handle, &game_state->entity_data);

        brain->harvesting_cooldown_s = w_clamp_min(brain->harvesting_cooldown_s - g_sim_dt_s, 0);

        if (target) {
            float distance_to_target = w_euclid_dist(entity->position, target->position);
            if (distance_to_target < 0.5) {
                entity->velocity = {0, 0};
                if (brain->harvesting_cooldown_s <= 0) {
                    entity_deal_damage(target, 1, game_state);
                    brain->harvesting_cooldown_s = BRAIN_HARVESTING_COOLDOWN_S;
                }
            } else {
                brain->target_position = target->position;
                brain_move_towards_target(entity, 4.0f);
            }
        } else {
            brain->ai_state = AI_STATE_SEARCHING;
            brain->harvesting_cooldown_s = 0;
        }

        break;
    }
    default:
        break;
    }
}

void brain_update(Entity* entity, GameState* game_state, double dt_s) {
    Brain* brain = &entity->brain;
    brain->cooldown_s = w_clamp_min(brain->cooldown_s - dt_s, 0);

    switch (brain->type) {
    case BRAIN_TYPE_WARRIOR:
        brain_update_warrior(entity, game_state, dt_s);
        break;
    case BRAIN_TYPE_BOAR:
        brain_update_boar(entity, game_state, dt_s);
        break;
    case BRAIN_TYPE_ROBOT_GATHERER:
        brain_update_robot_gatherer(entity, game_state, dt_s);
        break;
    default:
        break;
    }
}
