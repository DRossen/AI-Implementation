#include "bots.h"

namespace bots {

	BotDefinition bot_definitions[BotType_COUNT];

	BotDefinition *get_bot_definition(BotType bot_type) {

		if (bot_type < 0 || bot_type >= BotType_COUNT) {
			return nullptr;
		}

		return &bot_definitions[static_cast<int>(bot_type)];
	}
	bool apply_bot_definition(Agent &agent) {
		SERVER_AND_CLIENT_SIDE;

		BotDefinition *bot_def = bots::get_bot_definition(agent.bot_state.type);
		if (!bot_def) return false;

		agent.agent_scale = bot_def->agent_scale;
		agent.team = bot_def->team;
		agent.username = bot_def->name;
		agent.collidable = bot_def->collidable;
		agent.hitboxes_active = bot_def->hitboxes_active;
		agent.show_hp_bar = bot_def->show_hp_bar;
		agent.show_name = bot_def->show_name;

		double range_based_health_addition = MAX(0, randomizer.rand(bot_def->max_hp - bot_def->min_hp));

		BattleState &battle_state = agent.battle_state;
		battle_state.hp = bot_def->min_hp + range_based_health_addition;
		battle_state.set_starting_max_hp(battle_state.hp);
		battle_state.rotate_node_with_pitch = bot_def->rotate_node_with_pitch;
		battle_state.invulnerable = bot_def->invulnerable;

		BotState &bot_state = agent.bot_state;
		bot_state.difficulty_type = bot_def->difficulty_type;
		bot_state.interactable = bot_def->interact_points.size();

		Movement &movement = bot_state.movement;
		movement.acceleration_multiplier = bot_def->acceleration_multiplier;
		movement.deceleration_multiplier = bot_def->deceleration_multiplier;
		movement.max_speed = bot_def->min_speed + randomizer.rand(bot_def->max_speed - bot_def->min_speed);
		movement.effective_max_speed = movement.max_speed;
		movement.rotation_speed = bot_def->rotation_speed;
		movement.snap_to_navmesh = bot_def->snap_to_navmesh;
		movement.rotate_with_steering = bot_def->rotate_with_steering;
		movement.flying = bot_def->flying;

		// Set effect multipliers //
		for (int i = 0; i < bot_def->effect_multipliers.size(); i++) {
			movement.effect_multipliers[i] = bot_def->effect_multipliers[i];
		}

		return true;
	}
	void parse_bot_definitions(const std::string &buf, const std::string &file_name) {

		std::vector<std::string> lines = split(buf, "\n");

		enum ParseMode {
			BotTypeSearch = 0,
			Generic_values = 1,
			Container = 2,
		}; ParseMode parse_mode = BotTypeSearch;

		enum ParseContainerType {
			Container_None = 0,
			Container_Interacting = 1,
		}; ParseContainerType parse_type = Container_None;

		int depth = 0;
		BotType current_bot = BotType::BotType_COUNT;
		BotDefinition *bot = nullptr;
		InteractablePoint *interact_point = nullptr;


		for (int i = 0; i < lines.size(); i++) {
			std::string line = trim(lines[i]);

			bool can_parse_values = (current_bot != BotType_COUNT && bot != nullptr);

			if (line.empty()) continue;
			if (line.size() >= 2 && line[0] == '/' && line[1] == '/') continue;
			if (line == "{") {
				if (current_bot == BotType::BotType_COUNT) {
					LOG("Error, missing bot name in file " + file_name + " line " + toString(i));
				}
				depth++;
				parse_mode = static_cast<ParseMode>(depth);
			} else if (line == "}") {
				depth--;
				parse_mode = static_cast<ParseMode>(depth);
				parse_type = Container_None;
				interact_point = nullptr;

				if (depth == BotTypeSearch) {
					current_bot = BotType::BotType_COUNT;
					bot = nullptr;
				}
			} else {
				if (depth == BotTypeSearch) {
					std::vector<std::string> line_toks = split(line);
					if (line_toks.empty()) continue;

					current_bot = string_to_enum(line_toks[0]);

					if (current_bot == BotType_COUNT) {
						LOG("Error, unknown bot name in file " + file_name + " line " + toString(i));
						continue;
					}

					bot_definitions[current_bot] = BotDefinition();
					bot = &bots::bot_definitions[current_bot];
					bot->parsed = true;

				} else if (depth == Generic_values && can_parse_values) {

					std::vector<std::string> toks = split(line);
					if (toks.empty()) continue;
					if (toks.size() < 2) {
						platform::log("Error parsing bot definition in file " + file_name + " line " + toString(i) + " variable: " + toks[0]);
						continue;
					}

					std::string_view key = toks[0];
					const std::string &value = toks[1];
					const std::string &value2 = toks.size() > 2 ? toks[2] : toks[1];

					if (key == "name") {

						bot->name = "";

						if (toks.size() <= 1) {
							bot->name = "Bot";
						} else {
							for (int j = 1; j < toks.size(); j++) {
								bot->name += toks[j] + " ";
							}
						}
					} else if (key == "model") {
						bot->model = value;
					} else if (key == "agent_scale") {
						bot->agent_scale = STRTOF(value);
					} else if (key == "material") {
						bot->material = value;
					} else if (key == "collidable") {
						bot->collidable = strutil::parse_bool(value, false);
					} else if (key == "show_hp_bar") {
						bot->show_hp_bar = strutil::parse_bool(value, false);
					} else if (key == "show_name") {
						bot->show_name = strutil::parse_bool(value, false);
					} else if (key == "collision_radius") {
						bot->agent_radius = STRTOF(value);
					} else if (key == "rotation_speed") {
						bot->rotation_speed = STRTOF(value);
					} else if (key == "snap_to_navmesh") {
						bot->snap_to_navmesh = strutil::parse_bool(value, false);
					} else if (key == "rotate_node_with_pitch") {
						bot->rotate_node_with_pitch = strutil::parse_bool(value, false);
					} else if (key == "flying") {
						bot->flying = strutil::parse_bool(value, false);
					} else if (key == "max_hp") {
						bot->min_hp = STRTOI(value);
						bot->max_hp = STRTOI(value2);
					}else if (key == "max_speed") {
						bot->min_speed = STRTOF(value);
						bot->max_speed = STRTOF(value2);
					} else if (key == "acceleration_multiplier") {
						bot->acceleration_multiplier = STRTOF(value);
					} else if (key == "deceleration_multiplier") {
						bot->deceleration_multiplier = STRTOF(value);
					} else if (key == "team") {
						bot->team = MAX(STRTOI(value), 0); // Ensure team is not negative
					} else if (key == "knockback_multiplier") {
						bot->effect_multipliers[StatusEffectType::Knockback] = STRTOF(value);
					} else if (key == "timestop_multiplier") {
						bot->effect_multipliers[StatusEffectType::TimeStop] = STRTOF(value);
					} else if (key == "slow_multiplier") {
						bot->effect_multipliers[StatusEffectType::Slow] = STRTOF(value);
					} else if (key == "speed_multiplier") {
						bot->effect_multipliers[StatusEffectType::Speed] = STRTOF(value);
					} else if (key == "immobilize_multiplier") {
						bot->effect_multipliers[StatusEffectType::Immobilize] = STRTOF(value);
					} else if (key == "invulnerable") {
						bot->invulnerable = strutil::parse_bool(value, false);
					} else if (key == "difficulty_type") {
						if (value == "heavy") {
							bot->difficulty_type = DifficultyType_Heavy;
						} else if (value == "elite") {
							bot->difficulty_type = DifficultyType_Elite;
						} else if (value == "boss") {
							bot->difficulty_type = DifficultyType_Boss;
						}
					} else if (key == "interactpoint") {

						bot->interact_points.emplace_back();
#ifdef PRIVATE_BUILD
						bot->interact_points.back().debug_parsed_joint_name = value;
#endif
						interact_point = &bot->interact_points.back();
						parse_type = Container_Interacting;
					}
				} else if (depth == Container && can_parse_values) {

					std::vector<std::string> toks = split(line);
					if (toks.size() < 2) {
						platform::log("Error parsing bot Container value in file " + file_name + " line " + toString(i));
						continue;
					}

					std::string_view key = toks[0];
					std::string &value = toks[1];

					if (parse_type == Container_Interacting && interact_point) {

						if (key == "joint") {
							interact_point->hashed_joint_name = HashedString(value.c_str(), value.size());
						} else if (key == "radius") {
							interact_point->interact_radius = STRTOF(value);
						} else if (key == "ui_text") {

							for (int j = 1; j < toks.size(); j++) {
								interact_point->ui_text += toks[j] + " ";
							}
						} else if (key == "pickup") {
							interact_point->pickup = true;
						}
					}
				}
			}
		}
	}
	double get_bot_definition_range_based_hp(BotType bot_type) {
		BotDefinition *bot_def = bots::get_bot_definition(bot_type);
		if (!bot_def) return 1.0;

		return bot_def->min_hp + MAX(0, randomizer.rand(bot_def->max_hp - bot_def->min_hp));
	}
	double get_bot_definition_range_based_speed(BotType bot_type) {
		BotDefinition *bot_def = bots::get_bot_definition(bot_type);
		if (!bot_def) return 1.0;

		return bot_def->min_speed + MAX(0, randomizer.rand(bot_def->max_speed - bot_def->min_speed));
	}

	const char *enum_to_string(BotType value) {
		switch (value) {
			BOT_TYPE_LIST(BOT_ENUM_TO_STRING)
		}

		return nullptr;
	}
	const char *enum_to_case_string(BotType value) {
		switch (value) {
			BOT_TYPE_LIST(BOT_ENUM_TO_CASE_STRING)
		}

		return nullptr;
	}
	BotType string_to_enum(std::string key) {

		if (!startsWith(toLower(key), "bottype_")) {
			key = "bottype_" + key;
		}

		BOT_TYPE_LIST(BOT_STRING_TO_ENUM)
			return BotType::BotType_COUNT;
	}
	std::string enum_to_string_without_identifier(const std::string &key) {
		return key.substr(8);
	}

	void initialize_bot_state_by_type(Agent &agent) {
		BotState &bot_state = agent.bot_state;
		bot_state.type = agent.bot_state.type;

		for (int i = 0; i < StatusEffectType::StatusEffectCount; i++) {
			StatusEffect &effect = bot_state.movement.movement_effects[i];
			effect.type = static_cast<StatusEffectType>(i);
			effect.active = false;
		}

		bot_state.movement.effect_multipliers[StatusEffectType::Knockback] = 0.0;

		apply_bot_definition(agent);

		ai_manager_on_bot_death(netserver::state, agent);

		HookParameters params_cleanup;
		params_cleanup.target = &agent;
		params_cleanup.source = &agent;
		battle_callbacks::fire_event_on_agent(params_cleanup, EVENT_ON_BOT_CLEANUP, &agent);
		battle_callbacks::clear_and_apply_bot_callbacks(agent);


		ascension::add_ascension_buffs(agent);

		//Recalculate HP here
		agent.battle_state.hp = agents::get_max_hp(agent);

		//hook system
		battle_callbacks::EventServerBotInit params;
		params.agent = &agent;
		bots::BotDefinition *bot_def = get_bot_definition(bot_state.type);
		if (bot_def) {
			battle_callbacks::CallbackContext bot_context;
			bot_context.agent = &agent;
			bot_context.bot_state = &bot_state;
			bot_context.bot_definition = bot_def;
			battle_callbacks::fire_event_server_bot_init(bot_context, params);
		}
	}

	void update_bots_pre(const std::vector<Agent*>& active_bots, double dt) {

		// In order for Avoidance to know our movement intention, we need to update our velocity beforehand. 
		// Otherwise Avoidance will miss-judge by a tiny bit which creates a big miss over multiple frames.
		update_intended_velocity(active_bots, dt);

		// We pre-calculate avoidance for all bots by using a "snapshot" the current state.
		// This ensures consistent behavior by removing dependencies on the update order,
		// preventing bots from reacting to partially updated states of other bots.
		update_avoidance_velocity(active_bots, dt);
	}
	void update_bots(const std::vector<Agent *> &active_bots, double dt, const unsigned int turn) {

		for (Agent *bot : active_bots) {
			update_movement(*bot, dt);
			update_behavior(*bot, dt);
		}
	}
	void update_bots_post(const std::vector<Agent *> &active_bots, double dt) {
		// <---Not shown in showcase--> //
	}
}