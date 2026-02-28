#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class Cattle : public Node3D {
    GDCLASS(Cattle, Node3D)

private:
    float max_speed = 3.0f;
    float separation_radius = 2.0f;
    float flee_radius = 8.0f;
    float flee_strength = 5.0f;
    float cohesion_weight = 0.3f;
    float wander_strength = 0.5f;

    Vector3 velocity;

    void create_fallback_model();

protected:
    static void _bind_methods();

public:
    Cattle();
    ~Cattle();

    void _ready() override;
    void _process(double delta) override;

    void set_max_speed(float p_speed);
    float get_max_speed() const;

    void set_separation_radius(float p_radius);
    float get_separation_radius() const;

    void set_flee_radius(float p_radius);
    float get_flee_radius() const;

    Vector3 get_velocity() const;
};

}
