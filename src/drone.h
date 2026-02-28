#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <godot_cpp/templates/vector.hpp>

namespace godot {

class Drone : public Node3D {
    GDCLASS(Drone, Node3D)

private:
    float speed = 6.0f;
    float altitude = 5.0f;
    float dispersion_threshold = 10.0f;
    float offset_distance = 8.0f;

    Vector3 target_move;
    Vector3 target_paddock;
    bool is_collecting = false;

    void create_fallback_model();

protected:
    static void _bind_methods();

public:
    Drone();
    ~Drone();

    void _ready() override;
    void _process(double delta) override;

    void set_speed(float p_speed);
    float get_speed() const;

    void set_altitude(float p_altitude);
    float get_altitude() const;

    void set_target_paddock(Vector3 p_target);
    Vector3 get_target_paddock() const;

    bool get_is_collecting() const;
};

}
