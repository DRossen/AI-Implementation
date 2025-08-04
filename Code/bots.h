#pragma once
#define AI_SUPPORT

#include "bots_movement.h"
#include "bots_status_effects.h"
#include "bots_state_handling.h"
#include "bots_targeting.h"
#include "bots_utility.h"
#include "bots_constants.h"
#include <deque>

struct Agent;

namespace levels {
    struct Level;
}

namespace gamestate {
    struct GameState;
}

namespace bots {

    extern BotDefinition bot_definitions[BotType_COUNT];

    // Teams in the context of bots.
    enum Team {
        PlayerTeam = 0,
        EnemyTeam = 1
    };
    enum DifficultyType {
        DifficultyType_NotSet,
        DifficultyType_Heavy,
        DifficultyType_Elite,
        DifficultyType_Boss,
    };
    struct BotDefinition {
        BotDefinition() { effect_multipliers.fill(1.0); }
        std::array<double, StatusEffectType::StatusEffectCount> effect_multipliers; // Defaulted to 1.0 in apply_bot_definition

        bool parsed = false;
        DifficultyType difficulty_type = DifficultyType_NotSet;
        std::string name = "Bot";
        std::string model = "";
        std::string material = "";

        bool flying = false;
        bool collidable = true;
        bool show_hp_bar = false;
        bool show_name = false;
        bool interactable = false;
        bool snap_to_navmesh = true;
        bool invulnerable = false;
        bool hitboxes_active = true;
        bool rotate_node_with_pitch = false;
        bool rotate_with_steering = true;

        int team = EnemyTeam;
        int min_hp = 0;
        int max_hp = 0;
        int min_speed = 0;
        int max_speed = 0;

        double agent_scale = 1.0;
        double agent_radius = 0;
        double agent_height = 0;
        double acceleration_multiplier = 3.0;
        double deceleration_multiplier = 1.0;
        double rotation_speed = 0;

        std::vector<InteractablePoint> interact_points;

        // Every bot definition can map to a set of callbacks that are defined in that bots unit and added to this map in a setup function
        battle_callbacks::BattleCallbacks battle_callbacks;
    };
    struct BotState {

        std::string bot_tag = "";
        BotType type = BotType_None;
        DifficultyType difficulty_type = DifficultyType_NotSet;

        Movement movement;
        StateMachine state_machine;

        TargetingContext target_context;    // Setup for each bot's targeting criterias. Per default set to None which fallbacks to Proximity.
        std::vector<BotTarget> targets;     // Container of potential targets and history related to them.
        int     target_agent_id = NO_TARGET; 
        double  last_los_check_time = 0;    // Timestamp for last line of sight check.

        bool    inside_fog = true;          // Survivors specific. Indicates wether we are within fog or light and is used to apply damage reduction (if not in light).
        bool    interactable = false;       // Indicates wether the bot is interactable or not.
        bool    crowd_controlled = false;   // Used by StatusEffects to indicate if we can run the behavior update or not.
        bool    engaged_combat = false;     // Used by aggro system and is toggled to true once a target is detected within aggro range.
        double  spawn_timestamp = -1;
        double  global_action_cooldown = 0;
        double  time_outside_player_sight = 0; 
        double  last_damaged_time = 0;
        V3      spawn_position = V3::ZERO;
        V2      aggro_range = { INT_MAX, 1000 }; // { Horizontal , Vertical }

#ifdef PRIVATE_BUILD    
        std::deque<unsigned int> recent_statemachine_states;
#endif
    };

    const char *enum_to_string(BotType value);
    const char *enum_to_case_string(BotType value);
    BotType string_to_enum(std::string key);
    std::string enum_to_string_without_identifier(const std::string &key);

    BotDefinition *get_bot_definition(BotType bot_type);
    void parse_bot_definitions(const std::string &buf, const std::string &file_name);
    bool apply_bot_definition(Agent &agent);
    double get_bot_definition_range_based_hp(BotType bot_type);
    double get_bot_definition_range_based_speed(BotType bot_type);

    void initialize_bot_state_by_type(Agent &agent);

    void update_bots_pre(const std::vector<Agent *> &active_bots, double dt);
    void update_bots(const std::vector<Agent *> &active_bots, double dt, const unsigned int turn);
    void update_bots_post(const std::vector<Agent *> &active_bots, double dt);
}




