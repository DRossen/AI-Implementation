#include "bots.h"

namespace bots {

	const int MOVEMENT_NO_SEPARATION = -1;
	const int MOVEMENT_WAYPOINT_ARRIVE_THRESHOLD = 10;
	const double MOVEMENT_AVOIDANCE_PREDICTION_TIME = 1.0f;

	void _clamp_to_max_speed(V3 &velocity, const double max_speed) {

		double velocity_size_sqrd = velocity.length_squared();
		if (velocity_size_sqrd > max_speed * max_speed) {
			velocity = velocity.normalized() * max_speed;
		}
	}
	V3 _get_obstacle_avoidance_repulsion(Agent &agent, double agent_radius, const V3 &obstacle_position, double obstacle_radius) {

		V3 result = V3::ZERO;

		Movement &movement = agent.bot_state.movement;
		const V3 &agent_pos = agent.battle_state.position;

		const V3 to_entity = obstacle_position - agent_pos;
		const double dist_sq = to_entity.length_squared();
		const double combined_radius = agent_radius + obstacle_radius;

		if (dist_sq > combined_radius * combined_radius) { return V3::ZERO; }

		if (dist_sq < 0.01f) { return V3::ZERO; }

		const double dist = sqrt(dist_sq);
		const double penetration = combined_radius - dist;
		const V3 dir_to_entity = to_entity / dist;

		// Remove the part of avoidance that takes us further inside the entity radius
		const double inward_component = movement.avoidance.dot(dir_to_entity);
		if (inward_component > 0.0f) {
			result += -(dir_to_entity * inward_component);
		}

		const V3 moving_dir = movement.velocity.normalized_safe();
		const double dot_towards_obstacle = moving_dir.dot(dir_to_entity); // How much we're moving towards the entity

		// If we are heading towards the avoid entity
		if (dot_towards_obstacle > 0.0f) {
			V3 reflection_dir = moving_dir - dir_to_entity * dot_towards_obstacle; // Subtract the part of final_dir that's heading into the entity
			reflection_dir = reflection_dir.length_squared() > 0.001f ? reflection_dir.normalized() : -dir_to_entity;
			V3 slide_avoidance = reflection_dir * penetration; // proportional to penetration
			result += slide_avoidance;
		}

		// Since we only discard component taking us inside, we might end up too close over time.
		// If too close, apply a repulsion force.
		if (penetration > obstacle_radius * 0.25) {
			const V3 repulsion = -dir_to_entity * penetration * 0.25f;
			result += repulsion;
		}

		return result;
	}

	V3 _get_sharp_turn_deceleration_velocity(Movement &movement, const V3 &feet_position, double dt) {

		// a turn must have 2 points
		if (movement.path.size() < 2) {
			return V3::ZERO;
		}

		V3 next_waypoint = movement.path.back();
		V3 after_next_waypoint = movement.path[movement.path.size() - 2];

		// Ignore y axis for calculations
		V3 start_xz = V3(feet_position.x, 0, feet_position.z);
		V3 first_waypoint_xz = V3(next_waypoint.x, 0, next_waypoint.z);
		V3 second_waypoint_xz = V3(after_next_waypoint.x, 0, after_next_waypoint.z);

		double distance_to_waypoint = feet_position.distance(next_waypoint);

		// Define slowdown parameters
		const double slowdown_radius = movement.effective_max_speed * 0.5;
		const double slowdown_weight = 0.75; 
		const double brake_weight = 3.0;
		double base = 1.0 - CLAMP(slowdown_weight, 0, 1);

		if (distance_to_waypoint >= slowdown_radius) {
			return V3::ZERO;
		}

		V3 dir_to_waypoint = (first_waypoint_xz - start_xz).normalized_safe();
		V3 dir_to_next_waypoint = (second_waypoint_xz - first_waypoint_xz).normalized_safe();

		double dot = dir_to_waypoint.dot(dir_to_next_waypoint);
		double turn_angle_rad = acos(dot);
		double turn_sharpness = turn_angle_rad / TRIG_PI;

		// Increase slowdown near the next waypoint.
		double t = (distance_to_waypoint + MOVEMENT_WAYPOINT_ARRIVE_THRESHOLD) / slowdown_radius;
		t = sw::EaseInCubic(CLAMP(t, 0, 1));
		double slowdown_factor = t * (base + (slowdown_weight * turn_sharpness));

		V3 velocity_with_slowdown = dir_to_waypoint * movement.effective_max_speed * slowdown_factor;
		double brake_force = 0.8 + (turn_sharpness * brake_weight);

		V3 deceleration = (velocity_with_slowdown - movement.velocity) * brake_force;

		return deceleration;
	}
	V3 _get_forward_aligned_to_surface(const V3 &direction, const V3 &up_vector) {
		return (direction - up_vector * direction.dot(up_vector)).normalized_safe();
	}
	void _send_root_motion_blend_value_to_client(const Agent &agent, const V3 &blend_velocity, bool stop_root_motion) {

		messages::MsgEntityEvent pkg;
		pkg.event_key = ENTITY_EVENT_ROOT_MOTION_BLEND_VALUE;
		pkg.object_id = agent.player_id;
		pkg.argument = stop_root_motion;
		pkg.pos_x = blend_velocity.x;
		pkg.pos_y = blend_velocity.y;
		pkg.pos_z = blend_velocity.z;
		gamestate::_broadcast_message(netserver::state, &pkg, sizeof(pkg));
	}

	double get_avoidance_radius_by_type(const Agent &agent) {

		// TODO -> should be moved to script.

		double radius = 20.0;
		switch (agent.bot_state.type) {

			case BotType_Survivors_Spider: radius = 60; break;
			case BotType_Survivors_FlyingMinion: radius = 90; break;
			case BotType_Survivors_Spitter: radius = 30; break;
			case BotType_Survivors_Grunt: radius = 35; break;
			case BotType_Survivors_Hatguy: radius = 55; break;
			case BotType_Survivors_Lizard: radius = 35; break;
			case BotType_Survivors_HeavySpider: radius = 40; break;
			case BotType_Survivors_Bomber: radius = 41; break;
			case BotType_Survivors_Charger: radius = 125; break;
			case BotType_Survivors_Horde: radius = 50; break;
			case BotType_Survivors_Obelisk: radius = 140; break;
			case BotType_Survivors_Bat: radius = 60; break;
			case BotType_Survivors_Blinker: radius = 60; break;
			case BotType_Survivors_ConeHead: radius = 80; break;
			case BotType_Survivors_Dino: radius = 80; break;
			case BotType_Survivors_Butcher: radius = 65; break;
			case BotType_Survivors_Slime: radius = 100; break;
			case BotType_Survivors_Brute: radius = 95; break;
			case BotType_Survivors_Necromancer: radius = 100; break;
			case BotType_Survivors_Imp: radius = 60; break;
			case BotType_Survivors_HeavyHorde: radius = 60; break;
			case BotType_Survivors_Summoner: radius = 60; break;
			case BotType_Survivors_HandWalker: radius = 120; break;
			default: radius = 20; break;
		}

		return radius * agent.agent_scale;
	}
	void get_current_anim_root_motion(Agent &agent, const V3 &desired_velocity, OUT V3 &root_motion_delta, OUT double &yaw_delta) {

		// We need to tell the animation system at what velocity we desire root motion for.
		agent.override_reconstructed_velocity = true;
		agent.reconstructed_velocity_override = desired_velocity;

		// Retrieve the raw root motion from our current animation.
		V3 raw_root_motion_delta;
		double delta_yaw = 0;
		agents::get_animation_root_motion(agent, raw_root_motion_delta, delta_yaw);


		// Convert the local space root motion to world space motion.
		DirectX::XMMATRIX world_matrix = agents::get_model_world_matrix(agent);
		DirectX::XMVECTOR deltaPosVec = DirectX::XMVectorSet(raw_root_motion_delta.x, raw_root_motion_delta.y, raw_root_motion_delta.z, 1.0f); // w = 1.0 for position
		DirectX::XMVECTOR transformedPos = DirectX::XMVector3TransformNormal(deltaPosVec, world_matrix);

		root_motion_delta = transformedPos;
		yaw_delta = delta_yaw;
	}
	V3 get_navigation_direction(Agent &agent, bool use_height) {
		Movement &movement = agent.bot_state.movement;

		if (movement.path.empty()) { return V3::ZERO; }

		V3 direction = movement.path.back() - agents::get_feet_position(agent);
		if (!movement.flying && !use_height) {
			direction.y = 0.0;
		}

		return direction.normalized_safe();
	}

	void set_navigation_flags(Movement &movement, std::initializer_list<navmesh::PathFindFlags> flags) {

		unsigned int combined_flags = 0;
		for (const auto &flag : flags) {
			combined_flags |= static_cast<unsigned int>(flag);
		}

		movement.path_find_flags = static_cast<navmesh::PathFindFlags>(combined_flags);
	}

	void update_velocity(const std::vector<Agent *> &agents, double dt) {

		// NOTE: This is done as the pre-calculation of avoidance needs to know about
		// the full intended velocity of this frame in order to apply avoidance correctly.
		// If we would use just the velocity (which is the last frame's velocity during the pre-update), we 
		// will miss a slight amount of avoidance which causes bots to drift into eachother over time.

		for (Agent *agent : agents) {
			if (!agent) { continue; }

			Movement &movement = agent->bot_state.movement;
			movement.avoidance = V3::ZERO;
			navmesh::clean_path(agents::get_feet_position(*agent), movement.path, MOVEMENT_WAYPOINT_ARRIVE_THRESHOLD);

			const V3 navigation_direction = get_navigation_direction(*agent);

			// Our desired velocity is always at full speed in the direction of where we need to navigate.
			// TODO -> Scale desired_velocity based on proximity (we might want to slow down at certain threshold).
			movement.desired_velocity = navigation_direction * movement.effective_max_speed;

			// When moving with root motion, we request a animation delta from the animation system.
			if (movement.move_with_root_motion) {
				V3 root_motion_delta = V3::ZERO;
				double yaw_delta_dummy = 0.0;
				get_current_anim_root_motion(*agent, movement.desired_velocity, root_motion_delta, yaw_delta_dummy);
				movement.velocity = root_motion_delta / dt;
			} else {

				// If we've no desired velocity, we should slow down and use the deaceleration multiplier.
				const double velocity_multiplier = movement.desired_velocity.length_squared()
					? movement.acceleration_multiplier
					: movement.deceleration_multiplier;

				const V3 velocity_change = (movement.desired_velocity - movement.velocity) * velocity_multiplier * dt;
				movement.velocity += velocity_change;
			}

			_clamp_to_max_speed(movement.velocity, movement.effective_max_speed);
		}
	}
	void update_avoidance_velocity(const std::vector<Agent *> &agents, double dt) {

		std::vector<component::AvoidanceEntityData> avoid_entitites;
		component::get_avoidance_entities(*game::level, avoid_entitites);

		constexpr double SPEED_DIFF_EPSILON = 0.001f;

		// [Optimization] Calculate pairwise avoidance.
		for (size_t a = 0; a < agents.size(); ++a) {
			Agent *agent = agents[a];
			if (!agent) { continue; }

			Movement &movement_a = agent->bot_state.movement;
			
			const double radius = get_avoidance_radius_by_type(*agent);

			// +1 ensures we loop through each pair of agents exactly once.
			for (size_t oa = a + 1; oa < agents.size(); ++oa) {
				Agent *other_agent = agents[oa];
				if (!other_agent || other_agent->team != agent->team) { continue; }

				Movement &movement_b = other_agent->bot_state.movement;
				const double other_radius = get_avoidance_radius_by_type(*other_agent);

				const V3 relative_vel = movement_b.velocity - movement_a.velocity;
				const double rel_speed_sq = relative_vel.length_squared();

				if (rel_speed_sq < SPEED_DIFF_EPSILON) { continue; }

				const V3 relative_pos = other_agent->battle_state.position - agent->battle_state.position;
				const double t = CLAMP(relative_pos.dot(relative_vel) / rel_speed_sq, 0.0f, MOVEMENT_AVOIDANCE_PREDICTION_TIME);

				const V3 future_pos = agent->battle_state.position + movement_a.velocity * t;
				const V3 future_pos_other = other_agent->battle_state.position + movement_b.velocity * t;
				const V3 future_disp = future_pos_other - future_pos;

				const double future_dist_sq = future_disp.length_squared();
				const double combined_radius = radius + other_radius;
				if (future_dist_sq >= combined_radius * combined_radius) { continue; }

				const double future_dist = sqrt(future_dist_sq);
				if (!future_dist) { continue; }

				const double penetration = combined_radius - future_dist;
				const double weight = 1.0 - (t / MOVEMENT_AVOIDANCE_PREDICTION_TIME);
				const V3 push_dir = -future_disp / future_dist;
				const V3 avoidance = push_dir * penetration * weight;

				// Decide which agent that should yield (add the avoidance).
				// Currently using difficulty type, setup something more explicit if needed.
				if (agent->bot_state.difficulty_type == other_agent->bot_state.difficulty_type) {

					// Both has same priority to move and yield by half each.
					movement_a.avoidance += avoidance * 0.5 * movement_a.avoidance_enabled;
					movement_b.avoidance -= avoidance * 0.5 * movement_b.avoidance_enabled;

				} else if(agent->bot_state.difficulty_type > other_agent->bot_state.difficulty_type) {

					// A has priority and B should yield.
					movement_b.avoidance -= avoidance * 0.5 * movement_b.avoidance_enabled;
				} else {
					// Otherwise A must yield.
					movement_a.avoidance += avoidance * 0.5 * movement_a.avoidance_enabled;
				}
			}

			// Checks for nearby level-avoidance entities that we should avoid.
			for (const component::AvoidanceEntityData &entity : avoid_entitites) {
				movement_a.avoidance += _get_obstacle_avoidance_repulsion(*agent, radius, entity.position, entity.radius);
			}
		}
	}
	void update_movement(Agent &agent, double dt) {

		Movement &movement = agent.bot_state.movement;

		movement.velocity += movement.avoidance;
		_clamp_to_max_speed(movement.velocity, movement.effective_max_speed);

		// allow potential movement effects to adjust our velocity before we move.
		update_status_effects(agent, dt, OUT movement.velocity);

		const V3 feet_position = agents::get_feet_position(agent);
		V3 new_feet_position = feet_position + movement.velocity * dt;

		// while active, we need to ensure the new position is on the navmesh.
		if (movement.snap_to_navmesh && movement.velocity.length_squared() > 0.01) {

			V3 snapped_feet_position;
			unsigned int snapped_node_index = UINT_MAX;
			const V3 SEARCH_AREA = V3(300.0, 300.0, 300.0);

			if (get_nearest_navmesh_position(new_feet_position, movement.path_find_flags, snapped_feet_position, snapped_node_index, SEARCH_AREA)) {
				new_feet_position = snapped_feet_position;

				//OPTIMIZATION: cache the result so that later pathfinding calls can re-use the nearest node.
				agent.battle_state._nearest_navmesh_node_cached_args.position = new_feet_position;
				agent.battle_state._nearest_navmesh_node_cached_args.area_mask = gamestate::get_enabled_nav_area_ids(netserver::state);
				agent.battle_state._nearest_navmesh_node_cached_args.flags_mask = movement.path_find_flags;
				agent.battle_state._nearest_navmesh_node_cached_result = snapped_node_index;
			} 

			// Even though we did not retrieve a valid position, we let the bot move to avoid it being stuck.
		}

		agent.reconstructed_velocity = (new_feet_position - feet_position) / dt;
		agent.battle_state.last_position = agent.battle_state.position;
		agents::set_feet_position(agent, new_feet_position);

		// When root motion is toggled on/off, notify the client so it can adjust animation accordingly.
		if (agent.override_reconstructed_velocity != movement.move_with_root_motion) {
			agent.override_reconstructed_velocity = movement.move_with_root_motion;

			// This sends a message to the client to enable or disable using the overridden velocity for animation.
			_send_root_motion_blend_value_to_client(agent, agent.reconstructed_velocity_override, agent.override_reconstructed_velocity);
		}

		// If root motion is active, always send the current override velocity to the client.
		if (movement.move_with_root_motion) {
			// WHY WE DO THIS:
			// For animation driven root motion we toggle on a separate velocity that the animation system uses instead (if enabled),
			// to blend the animation correctly. If we were to use the delta between last and current position it would not represent the velocity we move with,
			// due to root motion in general not having linear movement deltas (changes a lot based on the root motion within the animation).
			_send_root_motion_blend_value_to_client(agent, agent.reconstructed_velocity_override, true);
		}

		// if enabled adjust our rotation towards where we are going.
		bool rotate_with_steering = movement.rotate_with_steering && !movement.path.empty();
		if (rotate_with_steering) {

			double cur_speed = movement.velocity.length();
			double max_speed = MAX(movement.max_speed, 0.001); // avoid div zero

			// Weight velocity direction more as speed approaches max_speed.
			// When almost standing still (speed near 0), navigation_direction dictates.
			double velocity_weight = CLAMP(cur_speed / max_speed, 0.0, 1.0);
			double navigation_weight = 1.0 - velocity_weight;

			V3 navigation_direction = get_navigation_direction(agent, true);
			V3 velocity_direction = movement.velocity.normalized_safe();
			V3 blended_direction = (navigation_direction * navigation_weight) + (velocity_direction * velocity_weight);

			// Fallback to navigation direction if the blended dir is very small.
			if (blended_direction.length_squared() < 0.01) {
				blended_direction = navigation_direction; 
			}

			V3 rotate_target_position = agent.battle_state.position + blended_direction.normalized_safe() * 20.0;
			rotate_towards(agent, rotate_target_position, dt);
		}
	}

	bool move_towards(Agent &agent, Agent &target) {

		//OPTIMIZATION: only recompute the agent's nearest node if necessary. this makes it so that
		//if we've already pre-calculated it elsehwere, we can reuse the cached result.
		BattleState::NearestNavmeshNodeCachedArgs args{};
		args.position = target.battle_state.position;
		args.area_mask = gamestate::get_enabled_nav_area_ids(netserver::state);
		//PITFALL: different bots may have different flags, but if sufficiently many have the same flag then the optimization should apply anyway
		args.flags_mask = agent.bot_state.movement.path_find_flags;

		//Only recompute the nearest navmesh node if the args have changed
		if (args != target.battle_state._nearest_navmesh_node_cached_args) {
			target.battle_state._nearest_navmesh_node_cached_args = args;
			target.battle_state._nearest_navmesh_node_cached_result = navmesh::find_nearest_node(
				navmesh::get_graph(), args.position, false, args.area_mask, args.flags_mask);

			if (target.battle_state._nearest_navmesh_node_cached_result == UINT_MAX) {
				PRINT("[Navigation Error] Could not find nearest poly (End) in bots::move_towards()");
				return false;
			}
		}

		return move_towards(agent, target.battle_state.position, target.battle_state._nearest_navmesh_node_cached_result);
	}
	bool move_towards(Agent &agent, const V3 &target_pos, unsigned int end_node /*optional: only useful if end_node has been pre-calculated*/) {

		bots::Movement &movement = agent.bot_state.movement;

		if (movement.flying) {
			return path_find_navgrid(
				gamestate::get_navgrid(netserver::state),
				agent.battle_state.position,
				target_pos,
				movement.path,
				true
			);
		}

		//OPTIMIZATION: only recompute the agent's nearest node if necessary. this makes it so that
		//if we've already pre-calculated it elsehwere, we can reuse the cached result.
		BattleState::NearestNavmeshNodeCachedArgs args{};
		args.position = agents::get_feet_position(agent);
		args.area_mask = gamestate::get_enabled_nav_area_ids(netserver::state);
		args.flags_mask = movement.path_find_flags;

		//Only recompute the nearest navmesh node if the args have changed
		if (args != agent.battle_state._nearest_navmesh_node_cached_args) {
			agent.battle_state._nearest_navmesh_node_cached_args = args;
			agent.battle_state._nearest_navmesh_node_cached_result = navmesh::find_nearest_node(
				navmesh::get_graph(), args.position, false, args.area_mask, args.flags_mask);
			if (agent.battle_state._nearest_navmesh_node_cached_result == UINT_MAX) {
				if (vars::dev_debug_navigation) {
					PRINT("[Navigation Error] Could not find nearest poly (Start) in bots::move_towards()");
				}
				return false;
			}
		}

		return path_find_navmesh(
			args.position,
			target_pos,
			movement.path,
			args.area_mask,
			(navmesh::PathFindFlags)args.flags_mask,
			agent.battle_state._nearest_navmesh_node_cached_result, //start_node
			end_node
		);
	}

	void rotate_towards(Agent &agent, const V3 &target_pos, double dt, bool rotate_pitch) {

		V3 dir_to_target = (target_pos - agent.battle_state.position).normalized_safe();
		if (!rotate_pitch) {
			dir_to_target.y = 0;
			dir_to_target = dir_to_target.normalized_safe();
		}

		// Increase rotation speed as our velocity aligns with the direction to the target.
		const Movement &movement = agent.bot_state.movement;
		double speed_pct = movement.velocity.length() / MAX(movement.effective_max_speed, 1.0);
		double dot = movement.velocity.normalized_safe().dot(dir_to_target) * speed_pct;
		double extra_rotation_speed = CLAMP(dot, 0, 1);

		double target_pitch, target_yaw;
		algebra::normal_to_nautical(dir_to_target, target_yaw, target_pitch);

		double rotation_speed = agent.bot_state.movement.rotation_speed;
		double scaled_rotation_speed = agent.bot_state.movement.rotation_speed * 0.4;

		double yaw_delta = algebra::wrap_pos_neg_pi(target_yaw - agent.battle_state.rotation_yaw);
		double yaw_smoothing_speed = algebra::get_mapped_range_value(abs(yaw_delta), rotation_speed + extra_rotation_speed, scaled_rotation_speed, 0, TRIG_PI);
		double yaw_adjustment = yaw_delta * yaw_smoothing_speed * dt;
		agent.battle_state.rotation_yaw += yaw_adjustment;

		if (rotate_pitch) {
			double pitch_delta = algebra::wrap_pos_neg_pi(target_pitch - agent.battle_state.rotation_pitch);
			double pitch_smoothing_speed = algebra::get_mapped_range_value(abs(pitch_delta), rotation_speed, scaled_rotation_speed, 0, TRIG_PI);
			double pitch_adjustment = pitch_delta * pitch_smoothing_speed * dt;
			agent.battle_state.rotation_pitch += pitch_adjustment;
		}
	}
	void rotate_with_surface_normal(Agent &agent, const V3 &surface_normal, const V3 &move_direction, double dt) {

		double rotation_speed = agent.bot_state.movement.rotation_speed;

		V3 right_vector = surface_normal.cross(move_direction.normalized_safe()).normalized_safe();
		V3 forward_vector = right_vector.cross(surface_normal);

		double lerped_forward_y = std::lerp(forward_vector.y > 0.0 ? 1.0 : -1.0, forward_vector.y, surface_normal.y);

		double target_pitch = -asin(CLAMP(lerped_forward_y, -1, 1)); //clamp to prevent nan_ind value.
		double target_yaw = atan2(right_vector.x, right_vector.z) - TRIG_HALF_PI;

		double pitch_diff = algebra::wrap_pos_neg_pi(target_pitch - agent.battle_state.rotation_pitch);
		double yaw_diff = algebra::wrap_pos_neg_pi(target_yaw - agent.battle_state.rotation_yaw);

		double PITCH_SMOOTHING_SPEED = algebra::get_mapped_range_value(abs(pitch_diff), 10, 1, 0, TRIG_PI);
		double YAW_SMOOTHING_SPEED = algebra::get_mapped_range_value(abs(yaw_diff), rotation_speed, 1, 0, TRIG_PI);

		double pitch_adjustment = pitch_diff * PITCH_SMOOTHING_SPEED * dt;
		double yaw_adjustment = yaw_diff * YAW_SMOOTHING_SPEED * dt;

		agent.battle_state.rotation_pitch += pitch_adjustment;
		agent.battle_state.rotation_yaw += yaw_adjustment;
	}

}