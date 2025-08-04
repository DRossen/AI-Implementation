#pragma once

struct Agent;

namespace bots {

    struct InteractablePoint {
        std::string ui_text = "";       // The ui indicator text that will appear once within interact_radius.
        size_t hashed_joint_name = 0;   // The Hash of the name of the joint that is interactable.
        double interact_radius = 50.0;  // Required distance to the joint in order to be interactable.
        bool pickup = false;            // If true, the bot will apply "HeldByAgent" StatusEffect onto it if joint is interacted with.

#ifdef PRIVATE_BUILD
        std::string debug_parsed_joint_name = "";
#endif 
    };

    bool can_bot_be_held(Agent &agent);
    bool is_bot_type_interactable(Agent &agent);
    void on_server_bot_interacted(Agent *player, Agent *bot, size_t join_hash_name);
}
