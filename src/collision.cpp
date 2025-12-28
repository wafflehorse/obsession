
uint32 sort_and_get_collision_hash_bucket(uint32* a_id, uint32* b_id) {
    if (*a_id > *b_id) {
        uint32 temp = *a_id;
        *a_id = *b_id;
        *b_id = temp;
    }

    uint32 hash_bucket = *a_id & (MAX_COLLISION_RULES - 1);
    return hash_bucket;
}

CollisionRule* find_collision_rule(uint32 a_id, uint32 b_id, GameState* game_state) {
    CollisionRule** hash = game_state->collision_rule_hash;

    uint32 hash_bucket = sort_and_get_collision_hash_bucket(&a_id, &b_id);

    CollisionRule* rule = 0;
    CollisionRule* found = 0;
    for (rule = hash[hash_bucket]; rule; rule = rule->next_rule) {
        if (rule->a_id == a_id && rule->b_id == b_id) {
            found = rule;
            break;
        }
    }

    return found;
}

void add_collision_rule(uint32 a_id, uint32 b_id, bool should_collide, GameState* game_state) {
    CollisionRule** hash = game_state->collision_rule_hash;
    CollisionRule** free_list = &game_state->collision_rule_free_list;

    CollisionRule* found = find_collision_rule(a_id, b_id, game_state);
    uint32 hash_bucket = sort_and_get_collision_hash_bucket(&a_id, &b_id);

    if (!found) {
        found = *free_list;
        if (!found) {
            found = (CollisionRule*)w_arena_alloc(&game_state->main_arena, sizeof(CollisionRule));
        } else {
            *free_list = found->next_rule;
        }
        found->a_id = a_id;
        found->b_id = b_id;
        found->next_rule = hash[hash_bucket];
        hash[hash_bucket] = found;
    }

    found->should_collide = should_collide;
}

// TODO: Do we need to make sure that entities marked for deletion don't collide?
bool should_collide(Entity* entity_a, Entity* entity_b, CollisionRule** hash) {
    float distance_between = w_euclid_dist(entity_a->position, entity_b->position);
    if (entity_a->id == entity_b->id || is_set(entity_a->flags, ENTITY_F_NONSPACIAL) ||
        is_set(entity_b->flags, ENTITY_F_NONSPACIAL) || distance_between > 10) {
        return false;
    }

    bool should_collide = true;
    if (is_set(entity_a->flags, ENTITY_F_KILLABLE) && !is_set(entity_b->flags, ENTITY_F_BLOCKER)) {
        should_collide = false;
    }

    if (entity_a->id > entity_b->id) {
        Entity* temp = entity_a;
        entity_a = entity_b;
        entity_b = temp;
    }

    uint32 hash_bucket = entity_a->id & (MAX_COLLISION_RULES - 1);
    CollisionRule* rule = 0;
    for (rule = hash[hash_bucket]; rule; rule = rule->next_rule) {
        if (rule->a_id == entity_a->id && rule->b_id == entity_b->id) {
            should_collide = rule->should_collide;
            break;
        }
    }

    return should_collide;
}

void handle_collision(Entity* subject, Entity* target, Vec2 collision_normal, double dt_collision_s,
                      bool* can_move_freely, GameState* game_state) {
    *can_move_freely = true;

    if (subject->type == ENTITY_TYPE_PROJECTILE) {
        if (is_set(target->flags, ENTITY_F_KILLABLE)) {
            entity_deal_damage(target, 1, game_state);
        }

        if (is_set(target->flags, ENTITY_F_KILLABLE) || is_set(target->flags, ENTITY_F_BLOCKER)) {
            set(subject->flags, ENTITY_F_MARK_FOR_DELETION);
        }

        add_collision_rule(subject->id, target->id, false, game_state);
    }

    if (is_set(target->flags, ENTITY_F_BLOCKER)) {
        subject->position =
            w_calc_position(subject->acceleration, subject->velocity, subject->position, dt_collision_s);
        float collision_normal_velocity_penetration = w_dot_product(subject->velocity, collision_normal);
        Vec2 collision_normal_velocity = w_vec_mult(collision_normal, collision_normal_velocity_penetration);
        subject->velocity = w_vec_sub(subject->velocity, collision_normal_velocity);

        float collision_normal_acceleration_penetration = w_dot_product(subject->acceleration, collision_normal);
        Vec2 collision_normal_acceleration = w_vec_mult(collision_normal, collision_normal_acceleration_penetration);
        subject->acceleration = w_vec_sub(subject->acceleration, collision_normal_acceleration);
        *can_move_freely = false;
    }
}
