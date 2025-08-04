#pragma once

extern const V3 DEFAULT_SEARCH_AREA;

namespace bots {

#ifdef PRIVATE_BUILD
	extern bool debug_bots_name;
	extern bool debug_bots_movement;
	extern bool debug_bots_targeting;
	extern bool debug_bots_state;
	extern bool debug_position_history_drawing;
	extern bool debug_navigation_path_drawing;
	extern bool debug_spawn_position_drawing;
	extern bool debug_ranged_attack_drawing;
#endif

	// Forward declarations of const variables.

	extern const int MOVEMENT_NO_SEPARATION;
	extern const int MOVEMENT_WAYPOINT_ARRIVE_THRESHOLD;
	extern const double MOVEMENT_VELOCITY_DAMPING;

	extern const unsigned int NO_TARGET;
	extern const int CHAIN_TRIGGER_MAX_COUNT;
	extern const double CHAIN_AGGRO_RANGE;
	extern const double LOS_CHECK_TIME_INTERVAL;
	extern const double STICKY_TARGETING_WEIGHT;

	extern const double MIN_KNOCKBACK_VELOCITY;
	extern const double MAX_KNOCKBACK_VELOCITY;
	extern const double MAX_KNOCKBACK_TIME;

	extern const double PER_PLAYER_HEALTH_MULTIPLIER;

/*
	====================================================================================

		  Below macros allow us to define the enum in one place (BOT_TYPE_LIST),
		  and automatically generate conversions for below.
		  This automatically reflects new options to the editor etc and is convenient for parsing etc:
			- The enum definition (BotType)
			- String-to-enum conversion
			- Enum-to-string conversion
			- Enum-to-string pairs (for UI options)

	====================================================================================
*/

#define BOT_ENUM(key, value) key = value,
#define BOT_STRING_TO_ENUM(enum_name, value) if (toLower(key) == toLower(#enum_name)) return BotType::enum_name;
#define BOT_ENUM_TO_STRING(key, value) case key: return toLower(#key).c_str();
#define BOT_ENUM_TO_CASE_STRING(key, value) case key: return #key;
#define BOT_ENUM_TO_STRING_PAIR(key, value) {toString(value), enum_to_string_without_identifier(#key)},

#define BOT_TYPE_LIST(FUNC)\
	FUNC(BotType_None, 0) \
	FUNC(BotType_Horde, 1) \
	/// --> ADD TO THE BOTTOM WHEN ADDING NEW <-- ///

	enum BotType : unsigned int {
		BOT_TYPE_LIST(BOT_ENUM)
		BotType_COUNT
	};
}
