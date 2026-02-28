#include "cattle.h"

#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_sphere3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdlib>

using namespace godot;

Cattle::Cattle() : velocity(Vector3(0, 0, 0)) {
}

Cattle::~Cattle() {
}

void Cattle::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_max_speed"), &Cattle::get_max_speed);
    ClassDB::bind_method(D_METHOD("set_max_speed", "speed"), &Cattle::set_max_speed);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "max_speed"), "set_max_speed", "get_max_speed");

    ClassDB::bind_method(D_METHOD("get_separation_radius"), &Cattle::get_separation_radius);
    ClassDB::bind_method(D_METHOD("set_separation_radius", "radius"), &Cattle::set_separation_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "separation_radius"), "set_separation_radius", "get_separation_radius");

    ClassDB::bind_method(D_METHOD("get_flee_radius"), &Cattle::get_flee_radius);
    ClassDB::bind_method(D_METHOD("set_flee_radius", "radius"), &Cattle::set_flee_radius);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "flee_radius"), "set_flee_radius", "get_flee_radius");

    ClassDB::bind_method(D_METHOD("get_velocity"), &Cattle::get_velocity);
}

void Cattle::create_fallback_model() {
    // Body: brown box
    CSGBox3D *body = memnew(CSGBox3D);
    body->set_size(Vector3(0.8f, 0.6f, 1.2f));
    body->set_position(Vector3(0, 0.3f, 0));

    Ref<StandardMaterial3D> body_mat;
    body_mat.instantiate();
    body_mat->set_albedo(Color(0.55f, 0.35f, 0.17f));
    body->set_material(body_mat);

    add_child(body);

    // Head: smaller sphere
    CSGSphere3D *head = memnew(CSGSphere3D);
    head->set_radius(0.25f);
    head->set_position(Vector3(0, 0.45f, 0.7f));

    Ref<StandardMaterial3D> head_mat;
    head_mat.instantiate();
    head_mat->set_albedo(Color(0.6f, 0.4f, 0.2f));
    head->set_material(head_mat);

    add_child(head);
}

void Cattle::_ready() {
    // Try loading GLB model
    ResourceLoader *loader = ResourceLoader::get_singleton();
    if (loader->exists("res://models/cow.glb")) {
        Ref<PackedScene> scene = loader->load("res://models/cow.glb");
        if (scene.is_valid()) {
            Node *instance = scene->instantiate();
            add_child(instance);
            return;
        }
    }

    // Fallback to CSG shapes
    create_fallback_model();
}

void Cattle::_process(double delta) {
    Node *parent = get_parent();
    if (!parent) return;

    Vector3 pos = get_position();
    Vector3 flee_vec = Vector3(0, 0, 0);
    Vector3 sep_vec = Vector3(0, 0, 0);
    Vector3 coh_vec = Vector3(0, 0, 0);
    Vector3 wander_vec = Vector3(0, 0, 0);

    // Find drone and other cattle
    Vector3 com = Vector3(0, 0, 0);
    int cattle_count = 0;
    int child_count = parent->get_child_count();

    for (int i = 0; i < child_count; i++) {
        Node *child = parent->get_child(i);

        // Check if it's a Drone - flee from it
        if (child->get_class() == "Drone") {
            Node3D *drone = Object::cast_to<Node3D>(child);
            if (drone) {
                Vector3 drone_pos = drone->get_position();
                drone_pos.y = 0;
                Vector3 my_pos_flat = pos;
                my_pos_flat.y = 0;
                float dist = my_pos_flat.distance_to(drone_pos);
                if (dist < flee_radius && dist > 0.01f) {
                    flee_vec = (my_pos_flat - drone_pos).normalized() * flee_strength;
                }
            }
        }

        // Check if it's another Cattle - separation and cohesion
        Cattle *other = Object::cast_to<Cattle>(child);
        if (other && other != this) {
            Vector3 other_pos = other->get_position();
            other_pos.y = 0;
            Vector3 my_pos_flat = pos;
            my_pos_flat.y = 0;

            float dist = my_pos_flat.distance_to(other_pos);

            // Separation
            if (dist < separation_radius && dist > 0.01f) {
                sep_vec += (my_pos_flat - other_pos).normalized() / dist;
            }

            // Cohesion (accumulate for COM)
            com += other_pos;
            cattle_count++;
        }
    }

    // Cohesion: steer toward center of mass
    if (cattle_count > 0) {
        com /= (float)cattle_count;
        Vector3 my_pos_flat = pos;
        my_pos_flat.y = 0;
        coh_vec = (com - my_pos_flat) * cohesion_weight;
    }

    // Wander: gentle random noise
    float rx = ((float)(rand() % 1000) / 500.0f) - 1.0f;
    float rz = ((float)(rand() % 1000) / 500.0f) - 1.0f;
    wander_vec = Vector3(rx, 0, rz) * wander_strength;

    // Combine forces
    velocity += (flee_vec + sep_vec + coh_vec + wander_vec) * (float)delta;

    // Clamp speed
    float speed = velocity.length();
    if (speed > max_speed) {
        velocity = velocity.normalized() * max_speed;
    }

    // Apply damping
    velocity *= 0.98f;

    // Update position (keep on ground plane)
    pos += velocity * (float)delta;
    pos.y = 0;

    // Clamp to field bounds (will be set by HerdManager via metadata)
    float field_half = 24.0f; // default, slightly inside fence
    Node *mgr = parent->get_parent();
    if (mgr) {
        Variant field_var = mgr->get_meta("field_size", Variant(50.0f));
        field_half = ((float)field_var / 2.0f) - 1.0f;
    }

    pos.x = CLAMP(pos.x, -field_half, field_half);
    pos.z = CLAMP(pos.z, -field_half, field_half);

    set_position(pos);

    // Rotate to face movement direction
    if (velocity.length() > 0.1f) {
        Vector3 look_dir = velocity.normalized();
        float angle = atan2(look_dir.x, look_dir.z);
        set_rotation(Vector3(0, angle, 0));
    }
}

void Cattle::set_max_speed(float p_speed) { max_speed = p_speed; }
float Cattle::get_max_speed() const { return max_speed; }

void Cattle::set_separation_radius(float p_radius) { separation_radius = p_radius; }
float Cattle::get_separation_radius() const { return separation_radius; }

void Cattle::set_flee_radius(float p_radius) { flee_radius = p_radius; }
float Cattle::get_flee_radius() const { return flee_radius; }

Vector3 Cattle::get_velocity() const { return velocity; }
