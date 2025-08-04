#include "bots_interaction.h"
#include "bots_status_effects.h"
#include "bots.h"

namespace bots {

	void handle_bot_pickup_interaction(Agent &player, Agent &bot) {

		if (is_effect_active(bot, StatusEffectType::HeldByAgent)) {

			V3 out_forward;
			algebra::nautical_to_normal(player.battle_state.rotation_yaw, -1.0, 1, 0, out_forward);

			clear_status_effect(bot, StatusEffectType::HeldByAgent);

			// TODO -> make knockback_velocity part of the network msg instead.
			const V3 knockback_velocity = out_forward * 550.0;
			apply_knockback(bot, knockback_velocity);

		} else {
			
			if (apply_held_by_agent_effect(bot, player.player_id)) {

				messages::MsgEntityEvent2 pkg;
				pkg.event_key = MSG_ENTITY_EVENT2_PICKUP_BOT;
				pkg.argument2 = player.player_id;
				pkg.argument3 = bot.player_id;

				for (const auto &[id, agent_ptr] : gamestate::get_agents(netserver::state)) {
					if (!agent_ptr || agent_ptr->is_bot_server) continue;

					netserver::send_message(agent_ptr->peer, agents::get_net_channel(*agent_ptr, NET_CHANNEL_TYPE_RELIABLE), &pkg, sizeof(pkg));
				}
			}
		}
	}
	bool can_bot_be_held(Agent &agent) {

		Movement &movement = agent.bot_state.movement;
		return !movement.movement_effects[StatusEffectType::HeldByAgent].active;
	}
	bool is_bot_type_interactable(Agent &agent) {

		BotDefinition *def = get_bot_definition(agent.bot_state.type);
		if (!def || def->interact_points.empty()) return false;

		return true;
	}
	void on_server_bot_interacted(Agent *player, Agent *bot, size_t join_hash_name) {
		if (!player || !bot) { return; }

		BotDefinition *def = get_bot_definition(bot->bot_state.type);
		if (!def) return;

		if (is_effect_active(*bot, StatusEffectType::HeldByAgent)) {

			V3 out_forward;
			algebra::nautical_to_normal(player->battle_state.rotation_yaw, -1.0, 1, 0, out_forward);

			clear_status_effect(*bot, StatusEffectType::HeldByAgent);

			const V3 knockback_velocity = out_forward * 550.0;
			apply_knockback(*bot, knockback_velocity);
		}

		const InteractablePoint *interact_point = nullptr;
		for (const InteractablePoint &point : def->interact_points) {
			if (join_hash_name != point.hashed_joint_name) { continue; }

			interact_point = &point;
			break;
		}

		// No defined interact points found, interaction not possible.
		if (!interact_point) return;


		if (interact_point->pickup) {
			handle_bot_pickup_interaction(*player, *bot);
		}

		battle_callbacks::EventServerActiveBotInteracted params;
		params.joint_hash_name = join_hash_name;
		params.player = player;

		battle_callbacks::CallbackContext bot_context;
		bot_context.agent = bot;
		bot_context.bot_state = &bot->bot_state;
		bot_context.bot_definition = def;
		battle_callbacks::fire_event_server_active_bot_interacted(bot_context, params);
	}
}