#include "bots_state_handling.h"

namespace bots {

	std::unordered_map<bots::BotType, std::unordered_map<unsigned int, bots::State>> &get_bot_state_map() {
		static std::unordered_map<bots::BotType, std::unordered_map<unsigned int, bots::State>> bot_state_map;
		return bot_state_map;
	}
	std::unordered_map<bots::BotType, unsigned int> &get_fallback_states() {
		static std::unordered_map<bots::BotType, unsigned int> fallback_states;
		return fallback_states;
	}
	void set_bot_fallback_state(bots::BotType bot_type, unsigned int fallback_state) {
		get_fallback_states()[bot_type] = fallback_state;
	}
	unsigned int get_bot_fallback_state(bots::BotType bot_type) {
		auto &fallback_states = get_fallback_states();
		return fallback_states.find(bot_type) != fallback_states.end() ? fallback_states[bot_type] : 0;
	}

	State *get_current_state(Agent &agent) {

		GlobalStateMap &bot_state_map = get_bot_state_map();
		GlobalStateMap::iterator it = bot_state_map.find(agent.bot_state.type);
		if (it == bot_state_map.end()) { return nullptr; }

		StateMachine &sm = agent.bot_state.state_machine;

		StateMap &state_map = bot_state_map.at(agent.bot_state.type);
		StateMap::iterator state_it = state_map.find(sm.current_state);
		return state_it != state_map.end() ? &state_it->second : nullptr;
	}
	State *get_state_by_id(BotType type, unsigned int state_id) {

		GlobalStateMap &bot_state_map = get_bot_state_map();
		GlobalStateMap::iterator it = bot_state_map.find(type);
		if (it == bot_state_map.end()) { return nullptr; }

		StateMap &state_map = bot_state_map.at(type);
		StateMap::iterator state_it = state_map.find(state_id);
		return state_it != state_map.end() ? &state_it->second : nullptr;
	}

	bool is_state_valid(BotType type, unsigned int state) {

		GlobalStateMap &bot_state_map = get_bot_state_map();
		GlobalStateMap::iterator it = bot_state_map.find(type);
		if (it == bot_state_map.end()) { return false; }

		StateMap &state_map = bot_state_map.at(type);
		StateMap::iterator state_it = state_map.find(state);
		return state_it != state_map.end();
	}
	bool change_state(Agent &agent, unsigned int target_state, bool force_transition) {

		BotType bot_type = agent.bot_state.type;

		if (!is_state_valid(bot_type, target_state)) {
			PRINT("Invalid state transition " + std::to_string(enum_to_string(bot_type)) + ". State ID: " + std::to_string(target_state));
			return false;
		}

		StateMap &state_definitions = get_bot_state_map().at(bot_type);
		StateMachine &sm = agent.bot_state.state_machine;

#ifdef PRIVATE_BUILD
		agent.bot_state.recent_statemachine_states.push_front(sm.current_state);
#endif

		is_state_valid(bot_type, sm.current_state) ? state_definitions.at(sm.current_state).exit(agent) : void();
		sm.previous_state = sm.current_state;
		sm.current_state = target_state;
		sm.time_in_state = 0;
		state_definitions.at(target_state).enter(agent);

		return true;
	}

	void _state_machine_update(Agent &agent, double dt) {

		BotType bot_type = agent.bot_state.type;

		// Check if there's any states defined for the type.
		GlobalStateMap &global_state_map = get_bot_state_map();
		GlobalStateMap::iterator state_map_it = global_state_map.find(bot_type);
		if (state_map_it == global_state_map.end()) {
			return;
		}

		StateMap &state_definitions = state_map_it->second;
		StateMachine &sm = agent.bot_state.state_machine;

		// Ensure our current state is valid.
		if (!is_state_valid(bot_type, sm.current_state)) {
			return;
		}

		// Update our current state
		const State &state = state_definitions.at(sm.current_state);
		StateStatus status = state.update(agent, dt);
		sm.time_in_state += dt;

		// Check if any transition gives thumbs up for state change.
		for (const StateTransition &transition : state.transitions) {
			if (transition.condition(agent)) {
				change_state(agent, transition.to_state);
				return;
			}
		}

		// Handle the status of our current state.
		switch (status) {

			case Success: {

				if (is_state_valid(bot_type, state.success_state)) {
					change_state(agent, state.success_state);
				} else {
					unsigned int fallback_state = get_bot_fallback_state(bot_type);
					change_state(agent, fallback_state);
				}
			} break;
			case Failure: {

				unsigned int fallback_state = get_bot_fallback_state(bot_type);
				change_state(agent, fallback_state);
			} break;
			case Running: {

			} break;
		}
	}
	void update_behavior(Agent &agent, double dt) {

		if (bots::BotDefinition *bot_def = bots::get_bot_definition(agent.bot_state.type)) {

			// We always run the main update.
			battle_callbacks::EventServerActiveBotUpdate params(dt);
			battle_callbacks::CallbackContext bot_context;
			bot_context.agent = &agent;
			bot_context.bot_state = &agent.bot_state;
			bot_context.bot_definition = bot_def;
			battle_callbacks::fire_event_server_active_bot_update(bot_context, params);
		}

		// State update on the other hand can be disabled by control effects.
		if (!agent.bot_state.crowd_controlled) {
			_state_machine_update(agent, dt);
		}
	}

}