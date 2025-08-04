#pragma once

struct Agent;

namespace bots {

    struct BotState;

    // Used when Attacks contain animation chaining to keep track of the state of the Action / Attack.
    enum AttackState {
        Windup = 1,
        Hold = 2,
        Execute = 3,
        Winddown = 4,
    };
    // Blob of different var's used for Attacks. 
    // This one is next in line for a proper cleanup / implementation.
    struct AttackDef {
        AttackState attack_state = Windup;
        const char *anim_name = "";
        bool attack_start = false;
        bool attack_ended = false;
        int weapon_idx = -1;
        int probability = 0; //0-100%
        int current_attack = 0;
        int num_attacks = 1;
        int current_attack_num = 0;
        double cooldown_max = 0.0;
        double cooldown_min = 0.0;
        double max_range = 0.0;
        double min_range = 0.0;
        double max_angle_from_forward = 0.0;
        double current_angle = 0.0;
        double starting_angle = 0.0;
        double target_angle = 0.0;
        double current_range = 0.0;
        double delay = 0.0;
        double windup = 0.0;
        double duration = 0.0;
        double retry_attack_delay = 0.0;
        double activation_time = 0.0;
        double available_time = 0.0;
        double last_attack_time = 0.0;
        V3 target_position;
        std::vector<int> hit_targets;
    };

    double get_dot_towards_position(Agent &agent, const V3 &position, bool ignore_pitch = false);
    double get_health_percentage_normalized(Agent &agent);
    double get_current_path_length(Agent &agent);
    void set_attack_on_cooldown(AttackDef &attack_def);
    bool attack_off_cooldown(Agent &agent, AttackDef &attack_def, bool ignore_global_cooldown = false);
    bool within_attack_range(const AttackDef &attack_def, const double distance);
    bool is_health_over_percentage(Agent &agent, float percentage);
    bool is_point_in_agent_view(Agent &agent, const V3 &point, double angle_threshold);
    bool within_cone(const V3 &origin, const V3 &target, const V3 &cone_dir, double cone_half_width_rad, double cone_length);
    bool within_cone_xz(const V3 &origin, const V3 &target, const V3 &cone_dir, double cone_half_width_rad, double cone_length);
    bool within_range_span_xz(const V3 &p1, const V3 &p2, double min_distance, double max_distance);
    bool within_distance_xz(Agent &agent, const V3 &target, double distance);
    bool within_distance_xz(const V3 &origin, const V3 &position, double distance);
    bool has_los_to_player(const BotState &bot_state, int player_id);
    bool is_path_to_position_straight(const V3 &start_feet_pos, const V3 &end_feet_pos, OUT V3 &nearest_target, double distance_behind_target = 100.0, unsigned int flags_mask = navmesh::PathFind_All);

}
