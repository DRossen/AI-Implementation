#pragma once
#include <unordered_map>

namespace bots {

    const unsigned int NO_TARGET = 999999;

    // BitFlag mask. Specifies how we want to evaluate targeting.
    enum TargetingWeight : unsigned int {
        TargetingWeight_None = 0,
        TargetingWeight_Proximity = 1 << 0,
        TargetingWeight_LineOfSight = 1 << 1,
        TargetingWeight_LowHealth = 1 << 2,
        // Add more options as needed. Don't forget to add a case for the scoring within update_target() also.
    };

    inline TargetingWeight operator|(TargetingWeight a, TargetingWeight b) { return static_cast<TargetingWeight>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b)); }
    inline TargetingWeight operator&(TargetingWeight a, TargetingWeight b) { return static_cast<TargetingWeight>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b)); }
    inline TargetingWeight &operator|=(TargetingWeight &a, TargetingWeight b) { a = a | b; return a; }

    // BitFlag mask. Specifies what type of Agents we want to consider as targets.
    enum TargetMask : unsigned int {
        None = 0,
        Players = 1 << 0,
        Bots = 1 << 1,
        AllOpponents = Players | Bots,
    };

    inline TargetMask operator|(TargetMask a, TargetMask b) { return static_cast<TargetMask>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b)); }
    inline TargetMask operator&(TargetMask a, TargetMask b) { return static_cast<TargetMask>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b)); }
    inline TargetMask &operator|=(TargetMask &a, TargetMask b) { a = a | b; return a; }


    // TargetContext defines how we want to 
    struct TargetingContext {

        // BitFlag mask to set which weights we want to use when retrieving targets.
        // If None is set we still fallback to Proximity.
        TargetingWeight weights_mask = TargetingWeight_None;

        // BitFlag mask to set what type of Agents we want to target (bots / players only at the time of writing this).
        TargetMask mask = TargetMask::AllOpponents;

        // This has to be set to true in order to recieve visible information of BotTarget's.
        // We might want to trace for LoS even tho we do not use it as target weight, therefor its own bool.
        bool trace_line_of_sight = false;

        // Optimization to avoid tracing if target is too far.
        // This distance should be relative to our highest range attack.
        double max_los_trace_distance = 1000.0f;

        // The distance that we base the Proximity scoring of.
        // A target further away than this distance will still generate a small score.
        // This is done to avoid not recieving any target at all.
        double proximity_scoring_distance = 1000.0f;

        // Adds a flag to the weights_mask and assign the importance of that criteria.
        void add_weight(TargetingWeight criteria, double weight);
        double get_weight(TargetingWeight criteria) const;

    private:
        std::unordered_map<TargetingWeight, double> weights;
    };

    struct BotTarget {
        unsigned int agent_id = NO_TARGET;
        V3 last_known_position = V3::ZERO;  // Is only set if the bot is tracing for line of sight.
        double distance_squared = 0;        // The distance to the target. Kept as squared for performance reasons. 
        double last_seen_time = 0;          // Is only set if the bot is tracing for line of sight.
        bool visible = false;               // Is only set if the bot is tracing for line of sight.
        bool valid = false;                 // Wont be considered as a target if not valid.
    };

    BotTarget *get_current_bot_target(Agent &agent);
    Agent *get_current_target(Agent &agent);
    Agent *update_targeting(Agent &agent, const TargetingContext &context, bool inform_client_of_target_change = false);

    void engage_combat_recursive(Agent &from_bot, const std::vector<Agent *> &same_team_bots, int iteration);
    void update_combat_state(Agent &bot, AIManager &manager);
}
