#include "bots.h"

extern Randomizer randomizer;

namespace bots {

    bool is_point_in_agent_view(Agent &agent, const V3 &point, double vision_width_angle) {

        V3 to_point = (point - agent.battle_state.position).normalized_safe();
        V3 forward = agents::get_agent_forward(agent, false);

        double dot = forward.dot(to_point);
        double angle = acos(dot);

        return angle <= DEG2RAD(vision_width_angle);
    }
    bool within_cone(const V3 &origin, const V3 &target, const V3 &cone_dir, double cone_half_width_rad, double cone_length) {

        V3 to_target = (target - origin);
        if (to_target.length_squared() > cone_length * cone_length) { return false; }

        double dot = to_target.normalized_safe().dot(cone_dir);
        return dot >= cos(cone_half_width_rad); 
    }
    bool within_cone_xz(const V3 &origin, const V3 &target, const V3 &cone_dir, double cone_half_width_rad, double cone_length) {
        
        // Not required to cache it like this but preffered readability here.
        const V2 origin_xz(origin.x, origin.z);
        const V2 target_xz(target.x, target.z);
        const V2 dir_xz(cone_dir.x, cone_dir.z);

        V2 to_target = target_xz - origin_xz;
        if (to_target.length_squared() > cone_length * cone_length) { return false; }

        double dot = to_target.normalized().dot(dir_xz.normalized());
        return dot >= cos(cone_half_width_rad);
    }
    bool within_range_span_xz(const V3 &p1, const V3 &p2, double min_distance, double max_distance) {
        return
            (p1 - p2).xz().length_squared() >= (min_distance * min_distance) &&
            (p1 - p2).xz().length_squared() <= (max_distance * max_distance);
    }
    bool within_distance_xz(Agent &agent, const V3 &target, double min_distance) {
        return (agent.battle_state.position - target).xz().length_squared() <= (min_distance * min_distance);
    }
    bool within_distance_xz(const V3 &origin, const V3 &position, double min_distance) {
        return (origin - position).xz().length_squared() <= (min_distance * min_distance);
    }

	double get_health_percentage_normalized(Agent &agent) {
		return (double)agent.battle_state.hp / (double)agents::get_max_hp(agent);
	}
    bool is_health_over_percentage(Agent &agent, float percentage) {
        float hp_percentage = (float)agent.battle_state.hp / (float)agents::get_max_hp(agent);
        bool is_over_percentage = hp_percentage > percentage;

        return is_over_percentage;
    }

    bool get_nearest_navmesh_position(const V3 &position, unsigned int path_flags, OUT V3 &out_position, const V3 &search_area) {
        unsigned int nearest_node_index_dummy = UINT_MAX;
        return get_nearest_navmesh_position(
            position,
            path_flags,
            out_position,
            nearest_node_index_dummy,
            search_area
        );
    }
    bool get_nearest_navmesh_position(const V3 &position, unsigned int path_flags, OUT V3 &out_position, OUT unsigned int &nearest_node_index, const V3 &search_area) {
        return navmesh::get_nearest_position(
            navmesh::get_graph(),
            position,
            search_area,
            out_position,
            nearest_node_index,
            gamestate::get_enabled_nav_area_ids(netserver::state),
            path_flags
        );
    }
    bool is_path_to_position_straight(const V3 &start, const V3 &end, OUT V3 &nearest_target, double distance_behind_target, unsigned int flags_mask) {

        if (!game::level) return false;

        V3 dir_to_target = (end - start).normalized_safe();
        V3 position_behind_target = end + V3(dir_to_target.x, 0.0, dir_to_target.z) * distance_behind_target;

        V3 out_position;
        if (!navmesh::trim_segment_to_target(navmesh::get_graph(), start, position_behind_target, out_position, gamestate::get_enabled_nav_area_ids(netserver::state), flags_mask, true)) {
            return false;
        }

        double current_distance = (start - end).length_squared();
        double trimmed_distance = (start - out_position).length_squared();

        if (current_distance > trimmed_distance) {
            return false;
        }

        nearest_target = out_position;

        return true;
    }
    double get_current_path_length(Agent &agent) {

        if (agent.bot_state.movement.path.empty()) { return 0; }
        std::vector<V3> &path = agent.bot_state.movement.path;

        double length_squared = (path.back() - agent.battle_state.position).length_squared();

        for (int i = 0; i < path.size() - 1; i++) {
            V3 &from = path[i];
            V3 &to = path[i + 1];

            length_squared += (from - to).length_squared();
        }

        return sqrt(length_squared);
    }

    void set_attack_on_cooldown(AttackDef &attack_def) {
        double cd = 0;
        if (attack_def.cooldown_max && attack_def.cooldown_min) {
            cd = attack_def.cooldown_min + randomizer.rand(attack_def.cooldown_max - attack_def.cooldown_min);
        } else if (attack_def.cooldown_max) {
            cd = attack_def.cooldown_max;
        } else if (attack_def.cooldown_min) {
            cd = attack_def.cooldown_min;
        }
        attack_def.available_time = timing::elapsed_time_seconds + cd;
        attack_def.attack_state = AttackState::Prepare;
    }
    bool attack_off_cooldown(Agent &agent, AttackDef &attack_def, bool ignore_global_cooldown) {

        if (!ignore_global_cooldown) {

            double elapsed_since_attack = timing::elapsed_time_seconds - attack_def.last_attack_time;
            if (elapsed_since_attack < agent.bot_state.global_action_cooldown) { return false; }
        }

        return timing::elapsed_time_seconds > attack_def.available_time;
    }
    bool within_attack_range(const AttackDef &attack_def, const double distance) {
        return (distance > attack_def.min_range && distance < attack_def.max_range);
    }

    bool has_los_to_player(const BotState &bot_state, int player_id) {

        for (const BotTarget &target : bot_state.targets) {

            if (target.agent_id == player_id) {
                return target.visible;
            }
        }

        return false;
    }

    double get_dot_towards_position(Agent &agent, const V3 &position, bool ignore_pitch) {

        double pitch = ignore_pitch ? 0 : agent.battle_state.rotation_pitch;
        V3 forward = V3::ZERO;
        algebra::nautical_to_normal(agent.battle_state.rotation_yaw, pitch, 1, 0, forward);
        V3 dir_to_target = (position - agent.battle_state.position).normalized_safe();

        if (ignore_pitch) {
            dir_to_target.y = 0.0;
            dir_to_target = dir_to_target.normalized_safe();
        }

        return forward.dot(dir_to_target);
    }
}