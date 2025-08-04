#pragma once

struct Agent;

namespace bots {

    struct State;
    enum BotType : unsigned int;
    typedef unsigned int StateEnum;

    typedef std::unordered_map<StateEnum, State> StateMap;
    typedef std::unordered_map<BotType, StateMap> GlobalStateMap;

    GlobalStateMap &get_bot_state_map();
    std::unordered_map<bots::BotType, unsigned int> &get_fallback_states();

    // This call creates a map on startup for the specified BotType to allow us to pre-define States.
    // fallback state is the state we always go back to if a State->Update returns "Failure".
#define SETUP_BOT_STATES(bot_type, fallback_state) \
	void setup_states(unsigned int type); \
    static bool registered = (get_bot_state_map()[bot_type] = std::unordered_map<unsigned int, State>(), set_bot_fallback_state(bot_type, fallback_state), setup_states(bot_type), true); \
    void setup_states(unsigned int type) // Function declaration follows

    // Used within SETUP_BOT_STATES scope to add a state to the state map.
    // "success_state" is the state we will transition into once when State->Update returns Success.
    // This allow us to avoid setting up a lot of simple transitions or transitions that would require information about our current state Update.
    // Last argument "..." is a VA_ARGS Macro argument which allows us to take in any number of "ADD_TRANSITION" calls.
#define ADD_STATE(enum_state, enter, update, exit, success_state, ...) \
    { \
        get_bot_state_map()[BotType(type)][enum_state] = State(#enum_state, enter, update, exit, success_state); \
	    State& new_state = get_bot_state_map()[BotType(type)][enum_state]; \
	    new_state.transitions.insert(new_state.transitions.end(), { __VA_ARGS__ } ); \
    } 

    // Used within ADD_STATE scope to add a possible state to transition to based on the specified condition.
#define ADD_TRANSITION(to_state, condition) \
      { to_state, condition }

    enum StateStatus {
        Running,    // Keeps the current state
        Success,    // Progress to the next state.
        Failure     // Progress to the fallback state.
    };
    struct StateTransition {
        unsigned int to_state;                // The enum state to transition into.
        bool (*condition)(Agent &) = nullptr; // Lambda function to check if we should transition
    };
    struct State {
        const char *name = "not set";
        void (*enter)(Agent &) = nullptr;
        StateStatus(*update)(Agent &, double dt) = nullptr;
        void (*exit)(Agent &) = nullptr;
                                                                                                             
        unsigned int success_state = 0; // The state we transition into if "Success" was returned by the current state.
        std::vector<StateTransition> transitions; // Container of possible transitions for a state. assign transitions by macro ADD_TRANSITION.
    };
    struct StateMachine {
        unsigned int previous_state = UINT_MAX;
        unsigned int current_state = UINT_MAX;
        double time_in_state = 0;   // Incremented by dt each tick while active.
    };

    unsigned int get_bot_fallback_state(bots::BotType bot_type);
    void set_bot_fallback_state(bots::BotType bot_type, unsigned int fallback_state);

    State *get_current_state(Agent &agent);
    State *get_state_by_id(BotType type, unsigned int state_id);

    bool change_state(Agent &agent, unsigned int target_state, bool force_transition = false);
    bool is_state_valid(BotType type, unsigned int state);

    void update_behavior(Agent &agent, double dt);
}
