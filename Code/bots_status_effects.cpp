#include "bots.h"
#include "bots_status_effects.h"

namespace bots {

	const double MIN_KNOCKBACK_VELOCITY = 250.0;
	const double MAX_KNOCKBACK_VELOCITY = 800.0;
	const double MAX_KNOCKBACK_TIME = 5.0;

	bool is_immune_to_effect(Agent &bot, StatusEffectType effect_type) {
		return bot.bot_state.movement.effect_multipliers[effect_type] <= 0.0;
	}
	bool is_less_effective_than_current(Agent &bot, StatusEffectType type, const double magnitude) {

		StatusEffect &effect = bot.bot_state.movement.movement_effects[type];
		if (!effect.active) { return false; }

		double current_magnitude = effect.current_scalar;

		switch (type) {

			// Add special handling if needed.
			case Speed:
			case Stagger: 
			case Slow: {
			} break;
		}

		return magnitude < current_magnitude;
	}
	bool is_effect_active(Agent &bot, StatusEffectType type) {
		StatusEffect &effect = bot.bot_state.movement.movement_effects[type];
		return effect.active;
	}
	void recompute_movement_speed(Agent &agent) {
		Movement &movement = agent.bot_state.movement;

		double allowed_speed_pct = 1.0;

		for (int i = 0; i < StatusEffectCount; ++i) {
			const StatusEffect &effect = movement.movement_effects[i];
			if (!effect.active) continue;

			switch (effect.type) {
				case Speed: //Fallthrough
				case Slow:
				case Stagger:
					allowed_speed_pct *= effect.current_scalar;
					break;

				case Immobilize: //Fallthrough
				case TimeStop:
					allowed_speed_pct = 0.0;
					break;
			}
		}

		movement.effective_max_speed = movement.max_speed * allowed_speed_pct;
	}

	void update_held_by_agent_effect(Agent &agent, double dt) {

		BotState &bot_state = agent.bot_state;
		Movement &movement = bot_state.movement;
		StatusEffect &held_effect = movement.movement_effects[HeldByAgent];
		
		unsigned int agent_holding = held_effect.u_int;
		Agent *holder = gamestate::get_agent_by_id(netserver::state, agent_holding);
		
		if (!holder || !holder->battle_state.alive) {
			clear_status_effect(agent, HeldByAgent);
			return;
		}

		constexpr double peak_height = 100;
		constexpr double lerp_to_hand_time = 0.3;

		double t = CLAMP(held_effect.elapsed_time / lerp_to_hand_time, 0.0, 1.0);
		double height_offset = 4.0 * peak_height * t * (1.0 - t);

		const V3 local_offset = V3(0, height_offset, 0);
		V3 world_offset = algebra::rotate_point_by_yaw(local_offset, holder->battle_state.rotation_yaw);
		V3 target_position = holder->battle_state.position + world_offset;
		V3 new_pos = algebra::lerp(t, held_effect.vector, target_position);

		agent.reconstructed_velocity = (new_pos - agent.battle_state.position) / dt;
		agent.battle_state.last_position = new_pos;
		agents::set_feet_position(agent, new_pos);
	}
	void update_status_effects(Agent &agent, double dt, OUT V3 &velocity) {

		// Reset control state and allow active control effects to toggle it back on.
		agent.bot_state.crowd_controlled = false;

		for (StatusEffect &effect : agent.bot_state.movement.movement_effects) {

			if (!effect.active) continue;

			switch (effect.type) {

				case Knockback: {
					update_knockback_effect(agent, dt, velocity);
				} break;
				case Immobilize: {
					update_immobilize_effect(agent, dt, velocity);
				} break;
				case TimeStop: {
					update_timeStop_effect(agent, dt, velocity);
					agent.bot_state.crowd_controlled = true;
				} break;
				case Speed: {
					update_speed_modifier(agent, dt);
				} break;
				case Stagger: 
				case Slow: {
					update_slow_modifier(agent, dt, effect.type);
				} break;
				case HeldByAgent: {
					agent.bot_state.crowd_controlled = true;
					update_held_by_agent_effect(agent, dt);
				} break;
			}
			effect.elapsed_time += dt;
		}

		recompute_movement_speed(agent);
	}
	void update_knockback_effect(Agent &agent, double dt, OUT V3 &velocity) {

		BotState &bot_state = agent.bot_state;
		Movement &movement = bot_state.movement;
		StatusEffect &knockback_effect = movement.movement_effects[Knockback];
		V3 &knockbac_velocity = knockback_effect.vector;

		knockbac_velocity.x = CLAMP(knockbac_velocity.x, -MAX_KNOCKBACK_VELOCITY, MAX_KNOCKBACK_VELOCITY);
		knockbac_velocity.y = CLAMP(knockbac_velocity.y, -MAX_KNOCKBACK_VELOCITY, MAX_KNOCKBACK_VELOCITY);
		knockbac_velocity.z = CLAMP(knockbac_velocity.z, -MAX_KNOCKBACK_VELOCITY, MAX_KNOCKBACK_VELOCITY);

		// handle possible infinite falling.
		if (knockback_effect.elapsed_time >= MAX_KNOCKBACK_TIME) {
			agent.battle_state.position = movement.last_safe_position;
			clear_status_effect(agent, Knockback);
			return;
		}

		// We need to ensure collision is enabled during knockback.
		agent.collidable = true;

		V3 ground_pos;
		if (levels::trace_ground(*game::level, agent.battle_state.position, ground_pos) &&
			agent.battle_state.position.distance(ground_pos) < 27) {
			movement.grounded = knockbac_velocity.y <= 0;
		} else {
			movement.grounded = false;
		}

		bool over_max_duration = knockback_effect.elapsed_time > MAX_KNOCKBACK_TIME;
		if ((movement.grounded && knockbac_velocity.y < 0) || over_max_duration) {
			movement.snap_to_navmesh = true;
			clear_status_effect(agent, Knockback);
			knockbac_velocity.y = 0;
			velocity = knockbac_velocity;

			V3 found_position;
			unsigned int nearest_node_index_dummy = UINT_MAX;
			bool is_in_nav_mesh = navmesh::get_nearest_position(navmesh::get_graph(), agent.battle_state.position, V3(100, 100, 100), found_position, nearest_node_index_dummy, gamestate::get_enabled_nav_area_ids(netserver::state), agent.bot_state.movement.path_find_flags);

			V3 landing_position = is_in_nav_mesh ? found_position : movement.last_safe_position;
			agents::set_feet_position(agent, landing_position);
			return;
		}

		knockbac_velocity.y -= vars::phy_gravity * dt;

		// Evaluate next position based on velocity
		V3 next_position_pre = agent.battle_state.position + knockbac_velocity * dt;

		bool dummy_inner_hit;
		unsigned char dummy_hit_player = PLAYER_NONE;
		double coverage = 0;
		V3 contact_normal;
		V3 contact_pos;
		V2 hit_radii = V2(1.0, 1.0);

		V3 direction = (next_position_pre - agent.battle_state.position).normalized_safe();
		double increment = (next_position_pre - agent.battle_state.position).length();

		battle::_hit_scan(__LINE__ + 31400,
			agent.battle_state.position,
			direction,
			game::level,
			dummy_hit_player, dummy_inner_hit, contact_pos, increment, coverage, contact_normal, true,
			0, true, false, hit_radii, nullptr, true, nullptr, 0.0, true,
			&agent, true, false, false, 0, 0, 0, 0);

		if (coverage < 1) {
			V3 reflection_vector = algebra::reflect(direction, contact_normal);
			knockbac_velocity = reflection_vector * 200;
		}

		V3 next_position = agent.battle_state.position + knockbac_velocity * dt;

		V3 last_safe_position;
		unsigned int nearest_node_index_dummy = UINT_MAX;
		bool position_found = navmesh::get_nearest_position(navmesh::get_graph(), agent.battle_state.position - V3(0, -500, 0), V3(100, 1000, 100), last_safe_position, nearest_node_index_dummy, gamestate::get_enabled_nav_area_ids(netserver::state), navmesh::PathFind_Default);

		if (position_found) {
			agent.bot_state.movement.last_safe_position = last_safe_position;
		}

		velocity = knockbac_velocity;
	}
	void update_immobilize_effect(Agent &agent, double dt, OUT V3 &velocity) {

		BotState &bot_state = agent.bot_state;
		StatusEffect &immobilize = bot_state.movement.movement_effects[StatusEffectType::Immobilize];

		velocity.x = 0;
		velocity.z = 0;

		if (immobilize.elapsed_time >= immobilize.duration) {
			clear_status_effect(agent, immobilize.type);
		}
	}
	void update_speed_modifier(Agent &agent, double dt) {
		Movement &movement = agent.bot_state.movement;
		StatusEffect &slow = movement.movement_effects[StatusEffectType::Speed];

		double t = CLAMP(1.0 - (slow.elapsed_time / slow.duration), 0.0, 1.0);
		slow.current_scalar = 1.0 + (slow.base_scalar - 1.0) * t;

		if (slow.elapsed_time >= slow.duration) {
			clear_status_effect(agent, slow.type);
		}
	}
	void update_slow_modifier(Agent &agent, double dt, StatusEffectType slow_type) {
		Movement &movement = agent.bot_state.movement;
		StatusEffect &slow = movement.movement_effects[slow_type];

		double t = CLAMP(1.0 - (slow.elapsed_time / slow.duration), 0.0, 1.0);

		slow.current_scalar = 1.0 - (slow.base_scalar * t);

		if (slow.elapsed_time >= slow.duration) {
			clear_status_effect(agent, slow.type);
		}
	}
	void update_timeStop_effect(Agent &agent, double dt, OUT V3 &velocity) {

		BotState &bot_state = agent.bot_state;
		StatusEffect &time_stop = bot_state.movement.movement_effects[StatusEffectType::TimeStop];

		velocity = V3::ZERO;

		if (time_stop.elapsed_time >= time_stop.duration) {
			agent.bot_state.movement.velocity = time_stop.vector;
			gamestate::broadcast_animation_timestart_event(netserver::state, agent);
			clear_status_effect(agent, time_stop.type);
			time_stop.active = false;
		}
	}

	void set_effect_effectivness(Agent &bot, StatusEffectType type, double effectivness) {
		bot.bot_state.movement.effect_multipliers[type] = CLAMP(effectivness, 0, 1);
	}

	bool reset_effect_effectivness(Agent &bot, StatusEffectType type) {

		BotDefinition *def = get_bot_definition(bot.bot_state.type);
		if (!def) return false;

		bot.bot_state.movement.effect_multipliers[type] = def->effect_multipliers[type];

		return true;
	}

	bool apply_status_effect(Agent &bot, StatusEffectType type, double duration, double scalar, V3 *velocity, StatusEffectVisuals *visuals) {

		switch (type) {
			case Knockback: if (velocity) { apply_knockback(bot, *velocity, visuals); } break;
			case Immobilize:	return apply_immobilize(bot, duration, visuals); break;
			case Slow:			return apply_slow_effect(bot, duration, scalar, visuals); break;
			case Stagger:		return apply_stagger_effect(bot, duration, scalar, visuals); break;
			case Speed:			return apply_speed_effect(bot, duration, scalar, visuals); break;
			case TimeStop:		return apply_timestop(bot, duration, visuals); break;

			default: {
				PRINT("apply_status_effect() recieved invalid Effect Type")
			}
		}

		return false;
	}
	bool clear_status_effect(Agent &bot, StatusEffectType type) {
		auto &state = bot.bot_state.movement.movement_effects;

		if (state[type].vfx_tag.size()) {
			gamestate::server_destroy_agent_attached_particle(netserver::state, &bot, state[type].vfx_tag);
		}

		state[type].active = false;
		state[type].elapsed_time = false;
		state[type].duration = 0;
		state[type].base_scalar = 1;
		state[type].current_scalar = 1;
		state[type].vector = V3::ZERO;

		if (type == HeldByAgent) {
			bot.bot_state.movement.snap_to_navmesh = true;
		}

		return false;
	}

	bool apply_held_by_agent_effect(Agent &bot, unsigned int held_by_agent_id, StatusEffectVisuals *visuals) {

		BotDefinition *def = get_bot_definition(bot.bot_state.type);
		if (!def || def->interact_points.empty()) { return false; }

		bool found_pickup_interact_point = false;
		for (const InteractablePoint &point : def->interact_points) {
			if (!point.pickup) { continue; }

			found_pickup_interact_point = true;
			break;
		}

		if (!found_pickup_interact_point) { return false; }

		Movement &movement = bot.bot_state.movement;
		movement.snap_to_navmesh = false;

		StatusEffect &effect = movement.movement_effects[HeldByAgent];
		effect.type = HeldByAgent;
		effect.u_int = held_by_agent_id;
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;
		effect.duration = 9999;
		effect.vector = bot.battle_state.position;

		return true;
	}
	bool apply_immobilize(Agent &bot, double duration, StatusEffectVisuals *visuals) {
		
		Movement &movement = bot.bot_state.movement;
		double effectivness = movement.effect_multipliers[Immobilize];
		const double scaled_duration = duration * effectivness;

		if (is_immune_to_effect(bot, Immobilize)) { return false; }

		StatusEffect &effect = movement.movement_effects[Immobilize];
		if (scaled_duration < effect.duration) { return false; }

		effect.type = Immobilize;
		effect.duration = duration * effectivness;
		effect.elapsed_time = 0.0;
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;

		movement.velocity = V3::ZERO;

		return true;
	}
	bool apply_speed_effect(Agent &bot, double duration, double speed_pct, StatusEffectVisuals *visuals) {

		Movement &movement = bot.bot_state.movement;
		double effectivness = movement.effect_multipliers[Speed];
		double new_speed_multiplier = (1.0 + speed_pct) * effectivness;


		if (is_immune_to_effect(bot, Speed)) { return false; }
		if (is_less_effective_than_current(bot, Speed, new_speed_multiplier)) { return false; }


		StatusEffect &effect = movement.movement_effects[Speed];
		effect.type = Speed;
		effect.duration = duration * effectivness;
		effect.elapsed_time = 0;
		effect.base_scalar = new_speed_multiplier;
		effect.current_scalar = effect.base_scalar;
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;

		return true;
	}
	bool apply_slow_effect(Agent &bot, double duration, double slow_pct, StatusEffectVisuals *visuals) {

		Movement &movement = bot.bot_state.movement;
		double effectivness = movement.effect_multipliers[Slow];
		const double scaled_slow_pct = CLAMP(slow_pct * effectivness, 0, 1); // Avoid negative slow %

		if (is_immune_to_effect(bot, Slow)) { return false; }
		if (is_less_effective_than_current(bot, Slow, scaled_slow_pct)) { return false; }

		StatusEffect &effect = movement.movement_effects[Slow];
		effect.type = Slow;
		effect.duration = duration * effectivness;
		effect.elapsed_time = 0.0;
		effect.base_scalar = scaled_slow_pct; 
		effect.current_scalar = (1.0 - effect.base_scalar); // current_scalar tracks the speed_pct_allowed
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;

		return true;
	}
	bool apply_stagger_effect(Agent &bot, double duration, double slow_pct, StatusEffectVisuals *visuals) {

		// Lazy way of disabling stagger on Elites and bosses, should probably be done thru script instead.
		if (bot.bot_state.difficulty_type == DifficultyType_Boss || bot.bot_state.difficulty_type == DifficultyType_Elite) { return false; }

		Movement &movement = bot.bot_state.movement;
		const double effectivness = movement.effect_multipliers[Stagger];
		const double scaled_slow_pct = CLAMP(slow_pct * effectivness, 0, 1); // Avoid negative slow %

		if (is_immune_to_effect(bot, Stagger)) { return false; }
		if (is_less_effective_than_current(bot, Stagger, scaled_slow_pct)) { return false; }

		StatusEffect &effect = movement.movement_effects[Stagger];
		effect.type = Stagger;
		effect.duration = duration * effectivness;
		effect.elapsed_time = 0.0;
		effect.base_scalar = scaled_slow_pct; 
		effect.current_scalar = (1.0 - effect.base_scalar); // current_scalar tracks the speed_pct_allowed
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;

		return true;
	}
	bool apply_knockback(Agent &bot, const V3 &knockback_velocity, StatusEffectVisuals *visuals) {

		Movement &movement = bot.bot_state.movement;
		movement.snap_to_navmesh = false;
		double effectivness = movement.effect_multipliers[Knockback];

		// disabled for flying units until proper physics are implemented for it.
		if(movement.flying) { return false; }
		if (is_immune_to_effect(bot, Knockback)) { return false; }

		StatusEffect &effect = movement.movement_effects[Knockback];
		effect.type = Knockback;
		effect.vector = knockback_velocity * effectivness;
		effect.elapsed_time = 0.0;
		effect.duration = MAX_KNOCKBACK_TIME;
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;

		if (effect.vector.length() < MIN_KNOCKBACK_VELOCITY) {
			effect.vector = effect.vector.normalized_safe() * MIN_KNOCKBACK_VELOCITY;
		}

		return true;
	}
	bool apply_timestop(Agent &bot, double duration, StatusEffectVisuals *visuals) {

		Movement &movement = bot.bot_state.movement;
		double effectivness = movement.effect_multipliers[TimeStop];
		const double scaled_duration = duration * effectivness;

		if (is_immune_to_effect(bot, TimeStop)) { return false; }

		StatusEffect &effect = movement.movement_effects[TimeStop];
		if (scaled_duration < effect.duration) { return false; } // Ignore a less effective one

		effect.type = TimeStop;
		effect.duration = scaled_duration;
		effect.elapsed_time = 0.0;
		effect.vfx_tag = visuals ? visuals->attached_vfx_tag : "";
		effect.active = true;
		effect.vector = bot.bot_state.movement.velocity;

		// freezes the animation
		gamestate::broadcast_animation_timestop_event(netserver::state, bot);

		return true;
	}
}