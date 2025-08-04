#include "bots_targeting.h"

namespace bots {

    const int CHAIN_TRIGGER_MAX_COUNT = 5;
    const double CHAIN_AGGRO_RANGE = 400.0;
    const double LOS_CHECK_TIME_INTERVAL = 0.5;
    const double STICKY_TARGETING_WEIGHT = 1.5;
    const double VISIBILITY_DECAY_DURATION = 3.0;

    Agent *get_current_target(Agent &agent) {
        const auto &it = gamestate::get_agents(netserver::state).find(agent.bot_state.target_agent_id);

        if (agent.bot_state.target_agent_id == NO_TARGET || it == gamestate::get_agents(netserver::state).end()) {
            return nullptr;
        }

        return it->second;
    }
    BotTarget *get_current_bot_target(Agent &agent) {

        if (agent.bot_state.target_agent_id == NO_TARGET) { return nullptr; }

        for (BotTarget &target : agent.bot_state.targets) {
            if (target.agent_id == agent.bot_state.target_agent_id) {
                return &target;
            }
        }
        return nullptr;
    }

    BotTarget &_find_or_add_target(std::vector<BotTarget> &targets, unsigned int agent_id) {
        for (BotTarget &t : targets) {
            if (t.agent_id == agent_id) return t;
        }
        BotTarget &new_target = targets.emplace_back();
        new_target.agent_id = agent_id;
        return new_target;
    }
    double _get_trace_distance_sqrd_with_padding(double max_los_trace_distance) {

        // We add some padding onto max trace dist to avoid targets toggling between visible true/false
        // when a bot just reached it's range to perform a attack.
        constexpr double MIN_PADDING = 300.0;
        constexpr double MAX_PADDING = 1000.0;
        constexpr double PADDING_SCALAR = 1.25;

        double padding = (max_los_trace_distance * PADDING_SCALAR) - max_los_trace_distance;
        padding = CLAMP(padding, MIN_PADDING, MAX_PADDING);

        double trace_distance = max_los_trace_distance + padding;

        // Return dist as sqrd due to targeting system caches distances squared.
        return (trace_distance * trace_distance);
    }
    double _compute_target_score(const Agent &self, const BotTarget &bot_target, const TargetingContext &context) {

        double score = 0.0;
        const double proximity_scoring_dist_sqrd = context.proximity_scoring_distance * context.proximity_scoring_distance;

        // Proximity
        if (context.weights_mask & TargetingWeight_Proximity || context.weights_mask == TargetingWeight_None) {
            double normalized_dist = bot_target.distance_squared / proximity_scoring_dist_sqrd;
            double proximity_score = 1.0 - CLAMP(normalized_dist, 0.0, 1.0);

            // Even if beyond max dist, allow a minimal score to avoid not retrieving any valid target at all.
            if (proximity_score <= 0.0) {
                proximity_score = (proximity_scoring_dist_sqrd / bot_target.distance_squared) * 0.001;
            }

            score += proximity_score * context.get_weight(TargetingWeight_Proximity);
        }

        // Line of Sight
        if (context.weights_mask & TargetingWeight_LineOfSight) {
            double time_since_seen = MAX(0.0, (timing::elapsed_time_seconds - bot_target.last_seen_time) - LOS_CHECK_TIME_INTERVAL);
            double visibility_score = 1.0 - CLAMP(time_since_seen / VISIBILITY_DECAY_DURATION, 0.0, 1.0);

            score += visibility_score * context.get_weight(TargetingWeight_LineOfSight);
        }

        // Low Health
        if (context.weights_mask & TargetingWeight_LowHealth) {

            if (Agent *target = gamestate::get_agent_by_id(netserver::state, bot_target.agent_id)) {
                double max_hp = target->battle_state.get_starting_max_hp();
                double cur_hp = target->battle_state.hp;

                double health_normalized = cur_hp / max_hp;
                double health_score = 1.0 - CLAMP(health_normalized, 0.0, 1.0) * context.get_weight(TargetingWeight_LowHealth);

                score += health_score;
            }
        }

        return score;
    }
    void _set_bot_target(Agent &agent, unsigned int new_target, bool send_target_to_client) {

        BotState &bot_state = agent.bot_state;

        unsigned int previous_target = bot_state.target_agent_id;
        bot_state.target_agent_id = new_target;

        AIManager &manager = gamestate::get_ai_manager(netserver::state);
        auto &target_tracker_map = manager.bot_target_tracker;
        target_tracker_map[previous_target] -= 1;
        target_tracker_map[new_target] += 1;

        if (send_target_to_client) {
            gamestate::broadcast_property(netserver::state, PROPERTY_KEY_BOT_TARGET, bot_state.target_agent_id, agent.player_id);
        }
    }

    void TargetingContext::add_weight(TargetingWeight criteria, double weight) {

        if (weight <= 0) return;

        weights_mask |= criteria;
        weights[criteria] = weight;
    }
    double TargetingContext::get_weight(TargetingWeight criteria) const {

        // If we have no weights set at all, we still use proximity as fallback.
        if (weights.empty() && criteria == TargetingWeight_Proximity) { return 1.0; }

        auto it = weights.find(criteria);
        return it != weights.end() ? it->second : 0.0f;
    }

    // checks if a player is within aggro range and triggers combat state.
    void update_combat_state(Agent &bot, AIManager &manager) {

        const double ver_aggro_range = bot.bot_state.aggro_range.y * bot.bot_state.aggro_range.y;
        const double hor_aggro_range_sqrd = bot.bot_state.aggro_range.x * bot.bot_state.aggro_range.x;

        // Check if we're within aggro distance to any player.
        for (Agent *player : manager.alive_active_players) {
            if (!player) { continue; }

            V3 to_target = player->battle_state.position - bot.battle_state.position;
            double hor_distance_sqrd = to_target.length_xz_squared();
            double ver_distance = to_target.y;

            if (ver_distance > ver_aggro_range || hor_distance_sqrd > hor_aggro_range_sqrd) {
                //outside aggro range
                continue;
            }

            // Trigger combat and check if any nearby enemies should tag along.
            bot.bot_state.engaged_combat = true;
            engage_combat_recursive(bot, gamestate::get_active_agents_by_team(netserver::state, bot.team), CHAIN_TRIGGER_MAX_COUNT);

            //no need to check more, we engaged combat.
            return;
        }
    }
    void engage_combat_recursive(Agent &from_bot, const std::vector<Agent *> &same_team_bots, int iteration) {

        ai_manager_on_bot_combat_triggered(from_bot);

        // Skip if chain trigger itteration is depleted.
        if (iteration <= 0) { return; }

        double range_pct_reduction_per_iteration = 0.05;
        double range_pct_reduction = range_pct_reduction_per_iteration * (CHAIN_TRIGGER_MAX_COUNT - iteration);
        double cur_range = CHAIN_AGGRO_RANGE * (1.0 - range_pct_reduction);

        for (Agent *bot : same_team_bots) {
            if (!bot || bot->bot_state.engaged_combat) continue;

            V3 to_target = bot->battle_state.position - from_bot.battle_state.position;

            if (to_target.length_xz_squared() < cur_range * cur_range) {
                bot->bot_state.engaged_combat = true;
                engage_combat_recursive(*bot, same_team_bots, --iteration);
            }
        }
    }

    // Updates the BotTarget container to ensure update_targeting works with only valid targets.
    void _update_bot_targets(Agent &agent, const TargetingContext &context, const std::vector<Agent *> &potential_targets) {
        BotState &bot_state = agent.bot_state;

        // remove invalid or stale targets
        for (int i = (int)bot_state.targets.size() - 1; i >= 0; --i) {
            BotTarget &target = bot_state.targets[i];
            Agent *opponent = gamestate::get_agent_by_id(netserver::state, target.agent_id);

            bool is_stale = !opponent || !opponent->targetable_by_bots || !opponent->battle_state.alive;
            if (is_stale) {
                std::swap(bot_state.targets[i], bot_state.targets.back());
                bot_state.targets.pop_back();
            }
        }

        // check if current update should perform the line of sight tests.
        bool perform_los_check = false;
        if (context.trace_line_of_sight) {

            double current_time = timing::logic_elapsed_time_seconds;
            double time_since_last_trace = current_time - bot_state.last_los_check_time;

            if (time_since_last_trace >= LOS_CHECK_TIME_INTERVAL) {
                perform_los_check = true;
                bot_state.last_los_check_time = current_time;
            }
        }

        // Add new targets and update existing ones.
        for (Agent *opponent : potential_targets) {

            // Skip invalid targets.
            if (!opponent || !opponent->targetable_by_bots || !opponent->battle_state.alive) { continue; }

            BotTarget &current = _find_or_add_target(bot_state.targets, opponent->player_id);
            current.distance_squared = (opponent->battle_state.position - agent.battle_state.position).length_squared();
            current.valid = true; 

            if (!current.valid) continue;

            double max_trace_distance_sqrd = _get_trace_distance_sqrd_with_padding(context.max_los_trace_distance);

            if (current.distance_squared > max_trace_distance_sqrd) {
                current.visible = false;
            } else if (perform_los_check) {



                V3 bot_view_pos = agent.battle_state.position + V3(0, agents::get_agent_collision_profile(agent).radius_top, 0);
                V3 opponent_view_pos = opponent->battle_state.position + V3(0, agents::get_agent_collision_profile(*opponent).radius_top, 0);

                // Plane padding might solve issues where we retrieve LOS just around a corner(?)
                // Try to adjust this if bots need to get around corners more before retrieving LOS.
                double plane_padding = 0.0;

                RayTrace trace;
                levels::trace(
                    *game::level, trace,
                    bot_view_pos, opponent_view_pos,
                    V3::ZERO, V3::ZERO,
                    true, true, true, plane_padding,
                    true, 812731223, false
                );

                current.visible = (trace.coverage >= 1.0);
                if (current.visible) {
                    current.last_known_position = opponent->battle_state.position;
                    current.last_seen_time = timing::elapsed_time_seconds;
                }
            }
        }
    }

    // Main targeting update.
    Agent *update_targeting(Agent &agent, const TargetingContext &context, bool inform_client_of_target_change) {

        BotState &bot_state = agent.bot_state;
        AIManager &ai_manager = gamestate::get_ai_manager(netserver::state);

        if (!bot_state.engaged_combat) {
            update_combat_state(agent, ai_manager);
            return nullptr;
        }

        Team target_team = agent.team == EnemyTeam ? PlayerTeam : EnemyTeam;
        const std::vector<Agent *> &potential_targets = gamestate::get_active_agents_by_team(netserver::state, target_team);

        _update_bot_targets(agent, context, potential_targets);

        // Target scoring and selection
        double best_score = 0;
        BotTarget *best_target = nullptr;

        for (BotTarget &target : bot_state.targets) {
            Agent *opponent = gamestate::get_agent_by_id(netserver::state, target.agent_id);
            if (!opponent) continue;

            TargetMask type = opponent->is_bot_server ? TargetMask::Bots : TargetMask::Players;
            if ((type & context.mask) == TargetMask::None) continue;

            const unsigned int new_target = target.agent_id;
            const unsigned int current_target = bot_state.target_agent_id;

            double score = _compute_target_score(agent, target, bot_state.target_context);

            if (new_target == current_target) {
                score *= STICKY_TARGETING_WEIGHT;
            }

            if (best_score < score) {
                best_score = score;
                best_target = &target;
            }
        }

        unsigned int new_target = best_target ? best_target->agent_id : NO_TARGET;

        if (bot_state.target_agent_id != new_target) {
            _set_bot_target(agent, new_target, inform_client_of_target_change);
        }

        return gamestate::get_agent_by_id(netserver::state, bot_state.target_agent_id);
    }

}