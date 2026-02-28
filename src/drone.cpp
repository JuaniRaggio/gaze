#include "drone.h"
#include "cattle.h"

#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_cylinder3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

Drone::Drone() : target_move(Vector3(0, 5, 0)), target_paddock(Vector3(0, 0, 0)) {
}

Drone::~Drone() {
}

void Drone::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_speed"), &Drone::get_speed);
    ClassDB::bind_method(D_METHOD("set_speed", "speed"), &Drone::set_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed"), "set_speed", "get_speed");

    ClassDB::bind_method(D_METHOD("get_altitude"), &Drone::get_altitude);
    ClassDB::bind_method(D_METHOD("set_altitude", "altitude"), &Drone::set_altitude);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "altitude"), "set_altitude", "get_altitude");

    ClassDB::bind_method(D_METHOD("get_target_paddock"), &Drone::get_target_paddock);
    ClassDB::bind_method(D_METHOD("set_target_paddock", "target"), &Drone::set_target_paddock);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_paddock"), "set_target_paddock", "get_target_paddock");

    ClassDB::bind_method(D_METHOD("get_is_collecting"), &Drone::get_is_collecting);
}

void Drone::create_fallback_model() {
    // Central body
    CSGBox3D *body = memnew(CSGBox3D);
    body->set_size(Vector3(0.6f, 0.15f, 0.6f));

    Ref<StandardMaterial3D> body_mat;
    body_mat.instantiate();
    body_mat->set_albedo(Color(0.3f, 0.3f, 0.35f));
    body->set_material(body_mat);

    add_child(body);

    // 4 arms
    Ref<StandardMaterial3D> arm_mat;
    arm_mat.instantiate();
    arm_mat->set_albedo(Color(0.2f, 0.2f, 0.25f));

    float arm_offset = 0.5f;
    Vector3 offsets[4] = {
        Vector3(arm_offset, 0, arm_offset),
        Vector3(-arm_offset, 0, arm_offset),
        Vector3(arm_offset, 0, -arm_offset),
        Vector3(-arm_offset, 0, -arm_offset)
    };

    for (int i = 0; i < 4; i++) {
        CSGCylinder3D *arm = memnew(CSGCylinder3D);
        arm->set_radius(0.04f);
        arm->set_height(0.7f);
        arm->set_position(offsets[i] * 0.5f);
        arm->set_rotation(Vector3(0, 0, Math_PI / 4.0f));
        arm->set_material(arm_mat);
        add_child(arm);

        // Rotor disc
        CSGCylinder3D *rotor = memnew(CSGCylinder3D);
        rotor->set_radius(0.2f);
        rotor->set_height(0.02f);
        rotor->set_position(offsets[i]);

        Ref<StandardMaterial3D> rotor_mat;
        rotor_mat.instantiate();
        rotor_mat->set_albedo(Color(0.1f, 0.1f, 0.1f, 0.5f));
        rotor_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
        rotor->set_material(rotor_mat);
        add_child(rotor);
    }
}

void Drone::_ready() {
    ResourceLoader *loader = ResourceLoader::get_singleton();
    if (loader->exists("res://models/drone.glb")) {
        Ref<PackedScene> scene = loader->load("res://models/drone.glb");
        if (scene.is_valid()) {
            Node *instance = scene->instantiate();
            add_child(instance);
            return;
        }
    }

    create_fallback_model();
}

void Drone::_process(double delta) {
    Node *parent = get_parent();
    if (!parent) return;

    // Gather cattle positions and find COM
    Vector3 com = Vector3(0, 0, 0);
    int cattle_count = 0;
    float max_dist_to_com = 0.0f;
    Vector3 farthest_cattle_pos = Vector3(0, 0, 0);

    int child_count = parent->get_child_count();
    // First pass: compute COM
    for (int i = 0; i < child_count; i++) {
        Cattle *cattle = Object::cast_to<Cattle>(parent->get_child(i));
        if (cattle) {
            com += cattle->get_position();
            cattle_count++;
        }
    }

    if (cattle_count == 0) return;

    com /= (float)cattle_count;
    com.y = 0;

    // Second pass: find farthest cattle from COM
    for (int i = 0; i < child_count; i++) {
        Cattle *cattle = Object::cast_to<Cattle>(parent->get_child(i));
        if (cattle) {
            Vector3 cpos = cattle->get_position();
            cpos.y = 0;
            float dist = cpos.distance_to(com);
            if (dist > max_dist_to_com) {
                max_dist_to_com = dist;
                farthest_cattle_pos = cpos;
            }
        }
    }

    // Strombom herding algorithm
    if (max_dist_to_com > dispersion_threshold) {
        // COLLECT mode: go toward the farthest cattle
        is_collecting = true;
        target_move = farthest_cattle_pos;
        target_move.y = altitude;
    } else {
        // DRIVE mode: position behind the herd (opposite to target paddock)
        is_collecting = false;
        Vector3 drive_dir = (target_paddock - com).normalized();
        Vector3 behind_pos = com - drive_dir * offset_distance;
        behind_pos.y = altitude;
        target_move = behind_pos;
    }

    // Smooth movement via lerp
    Vector3 pos = get_position();
    float lerp_factor = speed * (float)delta;
    if (lerp_factor > 1.0f) lerp_factor = 1.0f;
    pos = pos.lerp(target_move, lerp_factor);
    set_position(pos);

    // Face movement direction
    Vector3 flat_vel = target_move - get_position();
    flat_vel.y = 0;
    if (flat_vel.length() > 0.1f) {
        float angle = atan2(flat_vel.x, flat_vel.z);
        set_rotation(Vector3(0, angle, 0));
    }
}

void Drone::set_speed(float p_speed) { speed = p_speed; }
float Drone::get_speed() const { return speed; }

void Drone::set_altitude(float p_altitude) { altitude = p_altitude; }
float Drone::get_altitude() const { return altitude; }

void Drone::set_target_paddock(Vector3 p_target) { target_paddock = p_target; }
Vector3 Drone::get_target_paddock() const { return target_paddock; }

bool Drone::get_is_collecting() const { return is_collecting; }
