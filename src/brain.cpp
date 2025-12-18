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
    EntityAnimations animations = entity_info[entity->type].animations;
    if (w_euclid_dist(entity->position, brain->target_position) <= 1.0f || brain->cooldown_s <= 0) {
        brain->ai_state = AI_STATE_IDLE;
        brain->cooldown_s = w_random_between(1, 6);
    }

    Vec2 wander_direction = w_vec_norm(w_vec_sub(brain->target_position, entity->position));
    entity->velocity = w_vec_mult(wander_direction, 1.0f);
    entity->facing_direction = w_vec_norm(entity->velocity);
    entity_play_animation_with_direction(animations.move, &entity->anim_state, entity->facing_direction);
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
            Vec2 chase_direction = w_vec_norm(w_vec_sub(brain->target_position, entity->position));
            entity->velocity = w_vec_mult(chase_direction, 5.0f);
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
