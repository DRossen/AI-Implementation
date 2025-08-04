#pragma once
#include "bots_status_effects.h"

namespace bots {

    struct Movement {
        std::array<StatusEffect, StatusEffectCount> movement_effects = {};  // Currently holds all kinds of debuffs that can be applied to bots.
        std::array<double, StatusEffectCount> effect_multipliers;           // The effectivness that each debuff has on the bot.

        navmesh::PathFindFlags path_find_flags = navmesh::PathFindFlags::PathFind_Default; // Controls which navmesh triangles we can navigate within.  

        V3 velocity;                            // The velocity that we currently have.
        V3 desired_velocity;                    // The velocity that we try to achieve.
        V3 separation;                          // Separation velocity (Not used within movement update)
        V3 avoidance;                           // Avoidance velocity is used to negate any velocity that would cause a collision with another bot. (pre-calculated each frame within "update_avoidance_velocity()").

        double avoidance_radius = 0.0;          // TODO -> replace hardcoded stuff within get_avoidance_radius_by_type() and pass this thru script instead.
        double effective_max_speed = 0.0;       // How fast we can currently move (modified by StatusEffects). NOTE: to change actual max speed you need to change "max_speed".
        double max_speed = 0.0;                 // How fast we can move. (does not get modified by StatusEffects).
        double rotation_speed = 1.0;            // How fast the bot should rotate.
        double acceleration_multiplier = 3.0;   // Controls how fast we ramp up to max speed and also affects the bots ability to turn (adjust its velocity to the next target position).
        double deceleration_multiplier = 1.0;   // Controls how sharply we should slowdown once we have no target move position.

        bool flying = false;                    // Enables pathfinding to go thru NavGrid instead of Navmesh.
        bool grounded = false;                  // Used within knockback physics simulation to track grounded state.
        bool snap_to_navmesh = true;            // Enables / Disables constrain to navmesh. (disable this temporary turing jumps or similar physics simulations).
        bool avoidance_enabled = true;          // Enables / Disabled avoidance movement.
        bool rotate_with_steering = true;       // Enables / Disables rotation towards our next waypoint & velocity (blended).
        bool move_with_root_motion = false;     // Enables / Disables root motion movement. (Only affects our velocity / movement update if current playing animation has root motion).
        bool rotate_pitch_along_surface = false;// Rotate the pitch of the character when moving up/down slopes. (example was previous chargerm, OBS: haven't been used for quite some time).

        navmesh::Path path;                     // Our current path that we follow. 
        V3 last_safe_position = V3::ZERO;       // Used within knockback physics simulation to save the last valid "land" position in case of infinite falling.

#ifdef PRIVATE_BUILD
        std::vector<V3> recent_position_history;
#endif
    };

    V3 get_navigation_direction(Agent &agent, bool use_height = false);
    double get_avoidance_radius_by_type(const Agent &agent);
    void get_current_anim_root_motion(Agent &agent, const V3 &desired_velocity, OUT V3 &root_motion_delta, OUT double &yaw_delta);

    bool move_towards(Agent &agent, Agent &target);
    bool move_towards(Agent &agent, const V3 &target_pos, unsigned int end_node = UINT_MAX /*optional: only useful if end_node has been pre-calculated*/);

    void rotate_towards(Agent &agent, const V3 &target_pos, double dt, bool rotate_pitch = false);
    void rotate_with_surface_normal(Agent &agent, const V3 &surface_normal, const V3 &more_direction, double dt);

    void update_velocity(const std::vector<Agent *> &agents, double dt);
    void update_avoidance_velocity(const std::vector<Agent *> &agents, double dt);
    void update_movement(Agent &agent, double dt);

    // Note: This will overwrite any existing flags, and Default will not be set if not included.
    void set_navigation_flags(Movement &movement, std::initializer_list<navmesh::PathFindFlags> flags);

}
