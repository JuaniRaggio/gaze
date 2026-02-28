#pragma once

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {

class HerdManager : public Node3D {
    GDCLASS(HerdManager, Node3D)

private:
    int cattle_count = 15;
    float field_size = 50.0f;
    Vector3 target_position = Vector3(20, 0, 20);
    float target_radius = 5.0f;
    bool herding_complete = false;

    Node3D *cattle_group = nullptr;

    void create_terrain();
    void create_environment();
    void create_fences();
    void create_grass();
    void create_trees();
    void create_water_trough();
    void create_rocks();
    void create_target_marker();
    void spawn_cattle();
    void spawn_drone();

protected:
    static void _bind_methods();

public:
    HerdManager();
    ~HerdManager();

    void _ready() override;
    void _process(double delta) override;

    void set_cattle_count(int p_count);
    int get_cattle_count() const;

    void set_field_size(float p_size);
    float get_field_size() const;

    void set_target_position(Vector3 p_pos);
    Vector3 get_target_position() const;
};

}
