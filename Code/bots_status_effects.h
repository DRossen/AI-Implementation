#pragma once

namespace bots {

    enum StatusEffectType {
        Knockback,      // Fully locks the Movement.
        Immobilize,     // Fully locks the Movement.
        Slow,
        Stagger,        // Short duration slow
        Speed,
        TimeStop,       // Freeze effect, (disables both movement and behavior)
        HeldByAgent,    
        StatusEffectCount
    };

    // Ensures that the attached VFX with the given tag gets removed once the StatusEffect expires.
    struct StatusEffectVisuals {
        std::string attached_vfx_tag = "";
    };
    struct StatusEffect {
        StatusEffect() = default;
        StatusEffect(StatusEffectType _type) : type(_type) {}

        StatusEffectType type = StatusEffectType::StatusEffectCount;
        bool active = false;
        bool crowd_control = false;     // Indicated wether we should run the state update or not
        unsigned int u_int = 0;         // Generic u_int, used by HeldByAgent StatusEffect to keep track of the Agent holding the bot.
        double duration = 0;
        double elapsed_time = 0;
        double base_scalar = 1.0;       // The initial strength of the effect. Used to interpolate the magnitude of the effect over the duration.
        double current_scalar = 1.0;    // The current strength of the effect.
        V3 vector = V3::ZERO;           // Generic V3, used by Physics simulation to adjust the knockback velocity over time.
        std::string vfx_tag = "";       // If tag is assigned in apply_effect function, the vfx's with this tag will be removed when the StatusEffect expires. 
    };

    void update_status_effects(Agent &agent, double dt, OUT V3 &velocity);
    void update_knockback_effect(Agent &agent, double dt, OUT V3 &velocity);
    void update_immobilize_effect(Agent &agent, double dt, OUT V3 &velocity);
    void update_timeStop_effect(Agent &agent, double dt, OUT V3 &velocity);
    void update_speed_modifier(Agent &agent, double dt);
    void update_slow_modifier(Agent &agent, double dt, StatusEffectType slow_type);
    void recompute_movement_speed(Agent &agent);
    void set_effect_effectivness(Agent &bot, StatusEffectType type, double effectivness);
    bool reset_effect_effectivness(Agent &bot, StatusEffectType type);
    bool is_effect_active(Agent &bot, StatusEffectType type);
    bool is_less_effective_than_current(Agent &bot, StatusEffectType type, const double scalar);
    bool is_immune_to_effect(Agent &bot, StatusEffectType effect_type);
    bool apply_status_effect(Agent &bot, StatusEffectType type, double duration, double scalar, V3 *velocity = nullptr, StatusEffectVisuals *visuals = nullptr);
    bool clear_status_effect(Agent &bot, StatusEffectType type);
    bool is_effect_active(Agent &bot, StatusEffectType type);


    // Disables the bots movement update and makes it move together with the held agent id.
    bool apply_held_by_agent_effect(Agent &bot, unsigned int held_by_agent_id, StatusEffectVisuals *visuals = nullptr);

    // Disables the bots movement but still allows it to perform behavior update.
    bool apply_immobilize(Agent &bot, double duration, StatusEffectVisuals *visuals = nullptr);

    // Increases the speed based on the speed_pct (example: 0.2 equals 20% increase)
    bool apply_speed_effect(Agent &bot, double duration, double speed_pct, StatusEffectVisuals *visuals = nullptr);

    // Reduces the speed based on the slow_pct (example: 0.2 equals 20% decrease)
    bool apply_slow_effect(Agent &bot, double duration, double slow_pct, StatusEffectVisuals *visuals = nullptr);

    // Same as slow really but its own thing for now to avoid it overriding long persistent slows.
    bool apply_stagger_effect(Agent &bot, double duration, double slow_pct, StatusEffectVisuals *visuals = nullptr);

    // knockback_velocity has to be over 250 in size currently due to small knockbacks had some weird behavior.
    // this should be looked into and solved!
    bool apply_knockback(Agent &bot, const V3 &knockback_velocity, StatusEffectVisuals *visuals = nullptr);

    // Disables Movement & Behavior, freezes the animation pose also.
    bool apply_timestop(Agent &bot, double duration, StatusEffectVisuals *visuals = nullptr);
}
