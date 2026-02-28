#include "herd_manager.h"
#include "cattle.h"
#include "drone.h"

#include <godot_cpp/classes/box_mesh.hpp>
#include <godot_cpp/classes/camera3d.hpp>
#include <godot_cpp/classes/csg_box3d.hpp>
#include <godot_cpp/classes/csg_cylinder3d.hpp>
#include <godot_cpp/classes/cylinder_mesh.hpp>
#include <godot_cpp/classes/directional_light3d.hpp>
#include <godot_cpp/classes/environment.hpp>
#include <godot_cpp/classes/label3d.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/multi_mesh.hpp>
#include <godot_cpp/classes/multi_mesh_instance3d.hpp>
#include <godot_cpp/classes/plane_mesh.hpp>
#include <godot_cpp/classes/procedural_sky_material.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/sky.hpp>
#include <godot_cpp/classes/sphere_mesh.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/world_environment.hpp>
#include <godot_cpp/core/math.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cstdlib>
#include <ctime>

using namespace godot;

HerdManager::HerdManager() {
}

HerdManager::~HerdManager() {
}

void HerdManager::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_cattle_count"), &HerdManager::get_cattle_count);
    ClassDB::bind_method(D_METHOD("set_cattle_count", "count"), &HerdManager::set_cattle_count);
    ADD_PROPERTY(PropertyInfo(Variant::INT, "cattle_count"), "set_cattle_count", "get_cattle_count");

    ClassDB::bind_method(D_METHOD("get_field_size"), &HerdManager::get_field_size);
    ClassDB::bind_method(D_METHOD("set_field_size", "size"), &HerdManager::set_field_size);
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "field_size"), "set_field_size", "get_field_size");

    ClassDB::bind_method(D_METHOD("get_target_position"), &HerdManager::get_target_position);
    ClassDB::bind_method(D_METHOD("set_target_position", "position"), &HerdManager::set_target_position);
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "target_position"), "set_target_position", "get_target_position");
}

// ---------------------------------------------------------------------------
// 1. Terrain with procedural shader
// ---------------------------------------------------------------------------
void HerdManager::create_terrain() {
    MeshInstance3D *ground = memnew(MeshInstance3D);
    Ref<PlaneMesh> plane;
    plane.instantiate();
    plane->set_size(Vector2(field_size, field_size));
    plane->set_subdivide_width(10);
    plane->set_subdivide_depth(10);
    ground->set_mesh(plane);

    // Procedural noise shader for grass/dirt mix
    Ref<Shader> shader;
    shader.instantiate();
    shader->set_code(
        "shader_type spatial;\n"
        "uniform vec3 color_grass : source_color = vec3(0.28, 0.52, 0.18);\n"
        "uniform vec3 color_dirt  : source_color = vec3(0.45, 0.35, 0.22);\n"
        "\n"
        "float hash(vec2 p) {\n"
        "    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);\n"
        "}\n"
        "\n"
        "float noise(vec2 p) {\n"
        "    vec2 i = floor(p);\n"
        "    vec2 f = fract(p);\n"
        "    f = f * f * (3.0 - 2.0 * f);\n"
        "    float a = hash(i);\n"
        "    float b = hash(i + vec2(1.0, 0.0));\n"
        "    float c = hash(i + vec2(0.0, 1.0));\n"
        "    float d = hash(i + vec2(1.0, 1.0));\n"
        "    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);\n"
        "}\n"
        "\n"
        "float fbm(vec2 p) {\n"
        "    float v = 0.0;\n"
        "    float a = 0.5;\n"
        "    for (int i = 0; i < 4; i++) {\n"
        "        v += a * noise(p);\n"
        "        p *= 2.0;\n"
        "        a *= 0.5;\n"
        "    }\n"
        "    return v;\n"
        "}\n"
        "\n"
        "void fragment() {\n"
        "    float n = fbm(UV * 12.0);\n"
        "    ALBEDO = mix(color_grass, color_dirt, smoothstep(0.35, 0.65, n));\n"
        "    ROUGHNESS = 0.9;\n"
        "}\n"
    );

    Ref<ShaderMaterial> shader_mat;
    shader_mat.instantiate();
    shader_mat->set_shader(shader);

    ground->set_surface_override_material(0, shader_mat);
    add_child(ground);
}

// ---------------------------------------------------------------------------
// 2. Environment -- fog, glow, warm sun, fill light
// ---------------------------------------------------------------------------
void HerdManager::create_environment() {
    // Sky
    WorldEnvironment *world_env = memnew(WorldEnvironment);
    Ref<Environment> env;
    env.instantiate();

    Ref<Sky> sky;
    sky.instantiate();
    Ref<ProceduralSkyMaterial> sky_mat;
    sky_mat.instantiate();
    sky_mat->set_sky_top_color(Color(0.35f, 0.55f, 0.85f));
    sky_mat->set_sky_horizon_color(Color(0.65f, 0.75f, 0.9f));
    sky_mat->set_ground_bottom_color(Color(0.3f, 0.25f, 0.2f));
    sky_mat->set_ground_horizon_color(Color(0.65f, 0.75f, 0.9f));
    sky->set_material(sky_mat);
    env->set_sky(sky);
    env->set_background(Environment::BG_SKY);

    // Ambient light
    env->set_ambient_source(Environment::AMBIENT_SOURCE_SKY);
    env->set_ambient_light_energy(0.5f);

    // Tonemap
    env->set_tonemapper(Environment::TONE_MAPPER_ACES);

    // Fog
    env->set_fog_enabled(true);
    env->set_fog_density(0.005f);
    env->set_fog_light_color(Color(0.75f, 0.78f, 0.82f));
    env->set_fog_light_energy(0.8f);
    env->set_fog_sun_scatter(0.3f);

    // Glow
    env->set_glow_enabled(true);
    env->set_glow_intensity(0.4f);
    env->set_glow_bloom(0.05f);
    env->set_glow_blend_mode(Environment::GLOW_BLEND_MODE_SOFTLIGHT);

    world_env->set_environment(env);
    add_child(world_env);

    // Main sun -- warm tint
    DirectionalLight3D *sun = memnew(DirectionalLight3D);
    sun->set_rotation(Vector3(Math::deg_to_rad(-45.0f), Math::deg_to_rad(30.0f), 0));
    sun->set_shadow(true);
    sun->set_color(Color(1.0f, 0.95f, 0.85f));
    sun->set_param(Light3D::PARAM_ENERGY, 1.3f);
    add_child(sun);

    // Fill light from opposite angle
    DirectionalLight3D *fill = memnew(DirectionalLight3D);
    fill->set_rotation(Vector3(Math::deg_to_rad(-30.0f), Math::deg_to_rad(-150.0f), 0));
    fill->set_shadow(false);
    fill->set_color(Color(0.6f, 0.7f, 0.9f));
    fill->set_param(Light3D::PARAM_ENERGY, 0.3f);
    add_child(fill);

    // Camera
    Camera3D *cam = memnew(Camera3D);
    cam->set_position(Vector3(0, 35, 30));
    cam->set_rotation(Vector3(Math::deg_to_rad(-50.0f), 0, 0));
    cam->set_fov(50.0f);
    cam->set_current(true);
    add_child(cam);
}

// ---------------------------------------------------------------------------
// 3. Fences -- post-and-rail with MultiMesh
// ---------------------------------------------------------------------------
void HerdManager::create_fences() {
    float half = field_size / 2.0f;

    // Material for wood
    Ref<StandardMaterial3D> wood_mat;
    wood_mat.instantiate();
    wood_mat->set_albedo(Color(0.45f, 0.32f, 0.18f));

    // -- Posts --
    float post_spacing = 2.5f;
    float post_height = 1.2f;
    float post_radius = 0.06f;

    int posts_per_side = (int)(field_size / post_spacing) + 1;
    int total_posts = posts_per_side * 4;

    Ref<CylinderMesh> post_mesh;
    post_mesh.instantiate();
    post_mesh->set_top_radius(post_radius);
    post_mesh->set_bottom_radius(post_radius);
    post_mesh->set_height(post_height);
    post_mesh->set_radial_segments(6);
    post_mesh->set_rings(1);
    post_mesh->set_material(wood_mat);

    Ref<MultiMesh> post_mm;
    post_mm.instantiate();
    post_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    post_mm->set_mesh(post_mesh);
    post_mm->set_instance_count(total_posts);

    int idx = 0;
    // Four sides: +Z, -Z, +X, -X
    for (int side = 0; side < 4; side++) {
        for (int i = 0; i < posts_per_side; i++) {
            float t = -half + i * post_spacing;
            Vector3 pos;
            if (side == 0)      pos = Vector3(t, post_height / 2.0f, half);
            else if (side == 1) pos = Vector3(t, post_height / 2.0f, -half);
            else if (side == 2) pos = Vector3(half, post_height / 2.0f, t);
            else                pos = Vector3(-half, post_height / 2.0f, t);

            Transform3D xf;
            xf.origin = pos;
            post_mm->set_instance_transform(idx, xf);
            idx++;
        }
    }

    MultiMeshInstance3D *post_inst = memnew(MultiMeshInstance3D);
    post_inst->set_multimesh(post_mm);
    add_child(post_inst);

    // -- Rails (two horizontal rails per section) --
    float rail_radius = 0.03f;
    float rail_heights[2] = { 0.4f, 0.85f };

    int sections_per_side = posts_per_side - 1;
    int total_rails = sections_per_side * 4 * 2; // 4 sides, 2 rails each

    Ref<CylinderMesh> rail_mesh;
    rail_mesh.instantiate();
    rail_mesh->set_top_radius(rail_radius);
    rail_mesh->set_bottom_radius(rail_radius);
    rail_mesh->set_height(post_spacing);
    rail_mesh->set_radial_segments(4);
    rail_mesh->set_rings(1);
    rail_mesh->set_material(wood_mat);

    Ref<MultiMesh> rail_mm;
    rail_mm.instantiate();
    rail_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    rail_mm->set_mesh(rail_mesh);
    rail_mm->set_instance_count(total_rails);

    idx = 0;
    for (int side = 0; side < 4; side++) {
        for (int i = 0; i < sections_per_side; i++) {
            float t = -half + i * post_spacing + post_spacing / 2.0f;
            for (int r = 0; r < 2; r++) {
                Vector3 pos;
                Basis basis;
                if (side == 0) {
                    pos = Vector3(t, rail_heights[r], half);
                    basis = Basis(Vector3(0, 0, 1), Math::deg_to_rad(90.0f));
                } else if (side == 1) {
                    pos = Vector3(t, rail_heights[r], -half);
                    basis = Basis(Vector3(0, 0, 1), Math::deg_to_rad(90.0f));
                } else if (side == 2) {
                    pos = Vector3(half, rail_heights[r], t);
                    basis = Basis(Vector3(1, 0, 0), Math::deg_to_rad(90.0f));
                } else {
                    pos = Vector3(-half, rail_heights[r], t);
                    basis = Basis(Vector3(1, 0, 0), Math::deg_to_rad(90.0f));
                }

                Transform3D xf(basis, pos);
                rail_mm->set_instance_transform(idx, xf);
                idx++;
            }
        }
    }

    MultiMeshInstance3D *rail_inst = memnew(MultiMeshInstance3D);
    rail_inst->set_multimesh(rail_mm);
    add_child(rail_inst);
}

// ---------------------------------------------------------------------------
// 4. Grass -- 3000 blades via MultiMesh
// ---------------------------------------------------------------------------
void HerdManager::create_grass() {
    int blade_count = 3000;
    float half = field_size / 2.0f - 1.0f; // small margin from fences

    Ref<BoxMesh> blade_mesh;
    blade_mesh.instantiate();
    blade_mesh->set_size(Vector3(0.05f, 0.3f, 0.01f));

    Ref<MultiMesh> grass_mm;
    grass_mm.instantiate();
    grass_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    grass_mm->set_use_colors(true);
    grass_mm->set_mesh(blade_mesh);
    grass_mm->set_instance_count(blade_count);

    for (int i = 0; i < blade_count; i++) {
        float x = ((float)(rand() % 10000) / 5000.0f - 1.0f) * half;
        float z = ((float)(rand() % 10000) / 5000.0f - 1.0f) * half;
        float rot_y = (float)(rand() % 3600) / 3600.0f * Math_TAU;
        float scale_y = 0.6f + (float)(rand() % 1000) / 1000.0f * 0.8f;

        Basis basis(Vector3(0, 1, 0), rot_y);
        basis = basis.scaled(Vector3(1.0f, scale_y, 1.0f));

        Transform3D xf(basis, Vector3(x, 0.15f * scale_y, z));
        grass_mm->set_instance_transform(i, xf);

        // Green with slight variation
        float g = 0.45f + (float)(rand() % 1000) / 1000.0f * 0.25f;
        float r = 0.15f + (float)(rand() % 1000) / 1000.0f * 0.1f;
        grass_mm->set_instance_color(i, Color(r, g, 0.1f));
    }

    // Material that uses vertex color
    Ref<StandardMaterial3D> grass_mat;
    grass_mat.instantiate();
    grass_mat->set_flag(StandardMaterial3D::FLAG_ALBEDO_FROM_VERTEX_COLOR, true);
    grass_mat->set_cull_mode(StandardMaterial3D::CULL_DISABLED);
    blade_mesh->set_material(grass_mat);

    MultiMeshInstance3D *grass_inst = memnew(MultiMeshInstance3D);
    grass_inst->set_multimesh(grass_mm);
    add_child(grass_inst);
}

// ---------------------------------------------------------------------------
// 5. Trees -- 24 trees on perimeter via MultiMesh
// ---------------------------------------------------------------------------
void HerdManager::create_trees() {
    int tree_count = 24;
    float half = field_size / 2.0f;
    float tree_offset = 3.0f; // outside fences

    // Trunk material
    Ref<StandardMaterial3D> trunk_mat;
    trunk_mat.instantiate();
    trunk_mat->set_albedo(Color(0.4f, 0.28f, 0.15f));

    // Canopy material
    Ref<StandardMaterial3D> canopy_mat;
    canopy_mat.instantiate();
    canopy_mat->set_albedo(Color(0.2f, 0.45f, 0.15f));

    // Trunk mesh
    Ref<CylinderMesh> trunk_mesh;
    trunk_mesh.instantiate();
    trunk_mesh->set_top_radius(0.15f);
    trunk_mesh->set_bottom_radius(0.25f);
    trunk_mesh->set_height(3.0f);
    trunk_mesh->set_radial_segments(6);
    trunk_mesh->set_rings(1);
    trunk_mesh->set_material(trunk_mat);

    // Canopy mesh
    Ref<SphereMesh> canopy_mesh;
    canopy_mesh.instantiate();
    canopy_mesh->set_radius(1.5f);
    canopy_mesh->set_height(2.5f);
    canopy_mesh->set_radial_segments(8);
    canopy_mesh->set_rings(4);
    canopy_mesh->set_material(canopy_mat);

    // Trunk MultiMesh
    Ref<MultiMesh> trunk_mm;
    trunk_mm.instantiate();
    trunk_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    trunk_mm->set_mesh(trunk_mesh);
    trunk_mm->set_instance_count(tree_count);

    // Canopy MultiMesh
    Ref<MultiMesh> canopy_mm;
    canopy_mm.instantiate();
    canopy_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    canopy_mm->set_mesh(canopy_mesh);
    canopy_mm->set_instance_count(tree_count);

    // Distribute trees around perimeter
    float perimeter = 4.0f * field_size;
    float spacing = perimeter / (float)tree_count;

    for (int i = 0; i < tree_count; i++) {
        float d = i * spacing;
        float scale_var = 0.7f + (float)(rand() % 1000) / 1000.0f * 0.6f;
        Vector3 pos;

        float side_len = field_size;
        if (d < side_len) {
            pos = Vector3(-half + d, 0, half + tree_offset);
        } else if (d < side_len * 2.0f) {
            float t = d - side_len;
            pos = Vector3(half + tree_offset, 0, half - t);
        } else if (d < side_len * 3.0f) {
            float t = d - side_len * 2.0f;
            pos = Vector3(half - t, 0, -half - tree_offset);
        } else {
            float t = d - side_len * 3.0f;
            pos = Vector3(-half - tree_offset, 0, -half + t);
        }

        // Trunk transform
        float trunk_h = 3.0f * scale_var;
        Transform3D trunk_xf;
        trunk_xf.origin = pos + Vector3(0, trunk_h / 2.0f, 0);
        trunk_xf.basis = trunk_xf.basis.scaled(Vector3(scale_var, scale_var, scale_var));
        trunk_mm->set_instance_transform(i, trunk_xf);

        // Canopy transform
        Transform3D canopy_xf;
        canopy_xf.origin = pos + Vector3(0, trunk_h + 1.0f * scale_var, 0);
        canopy_xf.basis = canopy_xf.basis.scaled(Vector3(scale_var, scale_var, scale_var));
        canopy_mm->set_instance_transform(i, canopy_xf);
    }

    MultiMeshInstance3D *trunk_inst = memnew(MultiMeshInstance3D);
    trunk_inst->set_multimesh(trunk_mm);
    add_child(trunk_inst);

    MultiMeshInstance3D *canopy_inst = memnew(MultiMeshInstance3D);
    canopy_inst->set_multimesh(canopy_mm);
    add_child(canopy_inst);
}

// ---------------------------------------------------------------------------
// 6. Water trough -- CSG-based
// ---------------------------------------------------------------------------
void HerdManager::create_water_trough() {
    float half = field_size / 2.0f;
    // Place near a corner
    Vector3 trough_pos = Vector3(-half + 5.0f, 0, -half + 5.0f);

    Ref<StandardMaterial3D> wood_mat;
    wood_mat.instantiate();
    wood_mat->set_albedo(Color(0.35f, 0.25f, 0.12f));

    // Base
    CSGBox3D *base = memnew(CSGBox3D);
    base->set_size(Vector3(2.0f, 0.1f, 1.0f));
    base->set_position(trough_pos + Vector3(0, 0.05f, 0));
    base->set_material(wood_mat);
    add_child(base);

    // Walls
    struct WallData { Vector3 size; Vector3 offset; };
    WallData walls[4] = {
        { Vector3(2.0f, 0.5f, 0.08f), Vector3(0, 0.25f,  0.46f) },
        { Vector3(2.0f, 0.5f, 0.08f), Vector3(0, 0.25f, -0.46f) },
        { Vector3(0.08f, 0.5f, 1.0f),  Vector3( 0.96f, 0.25f, 0) },
        { Vector3(0.08f, 0.5f, 1.0f),  Vector3(-0.96f, 0.25f, 0) },
    };

    for (int i = 0; i < 4; i++) {
        CSGBox3D *wall = memnew(CSGBox3D);
        wall->set_size(walls[i].size);
        wall->set_position(trough_pos + walls[i].offset);
        wall->set_material(wood_mat);
        add_child(wall);
    }

    // Water surface
    MeshInstance3D *water = memnew(MeshInstance3D);
    Ref<PlaneMesh> water_plane;
    water_plane.instantiate();
    water_plane->set_size(Vector2(1.8f, 0.8f));
    water->set_mesh(water_plane);
    water->set_position(trough_pos + Vector3(0, 0.4f, 0));

    Ref<StandardMaterial3D> water_mat;
    water_mat.instantiate();
    water_mat->set_albedo(Color(0.2f, 0.4f, 0.6f, 0.7f));
    water_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    water_mat->set_metallic(0.3f);
    water_mat->set_roughness(0.1f);
    water->set_surface_override_material(0, water_mat);
    add_child(water);
}

// ---------------------------------------------------------------------------
// 7. Rocks -- 10 scattered rocks via MultiMesh
// ---------------------------------------------------------------------------
void HerdManager::create_rocks() {
    int rock_count = 10;
    float half = field_size / 2.0f - 2.0f;

    Ref<SphereMesh> rock_mesh;
    rock_mesh.instantiate();
    rock_mesh->set_radius(0.5f);
    rock_mesh->set_height(0.6f); // squashed sphere
    rock_mesh->set_radial_segments(6);
    rock_mesh->set_rings(3);

    Ref<StandardMaterial3D> rock_mat;
    rock_mat.instantiate();
    rock_mat->set_albedo(Color(0.5f, 0.48f, 0.45f));
    rock_mat->set_roughness(0.95f);
    rock_mesh->set_material(rock_mat);

    Ref<MultiMesh> rock_mm;
    rock_mm.instantiate();
    rock_mm->set_transform_format(MultiMesh::TRANSFORM_3D);
    rock_mm->set_mesh(rock_mesh);
    rock_mm->set_instance_count(rock_count);

    for (int i = 0; i < rock_count; i++) {
        float x = ((float)(rand() % 10000) / 5000.0f - 1.0f) * half;
        float z = ((float)(rand() % 10000) / 5000.0f - 1.0f) * half;
        float scale = 0.4f + (float)(rand() % 1000) / 1000.0f * 1.0f;
        float rot_y = (float)(rand() % 3600) / 3600.0f * Math_TAU;

        Basis basis(Vector3(0, 1, 0), rot_y);
        basis = basis.scaled(Vector3(scale, scale * 0.5f, scale));

        Transform3D xf(basis, Vector3(x, 0.15f * scale, z));
        rock_mm->set_instance_transform(i, xf);
    }

    MultiMeshInstance3D *rock_inst = memnew(MultiMeshInstance3D);
    rock_inst->set_multimesh(rock_mm);
    add_child(rock_inst);
}

// ---------------------------------------------------------------------------
// Target marker (unchanged)
// ---------------------------------------------------------------------------
void HerdManager::create_target_marker() {
    CSGCylinder3D *marker = memnew(CSGCylinder3D);
    marker->set_radius(target_radius);
    marker->set_height(0.1f);
    marker->set_position(target_position + Vector3(0, 0.05f, 0));

    Ref<StandardMaterial3D> marker_mat;
    marker_mat.instantiate();
    marker_mat->set_albedo(Color(0.9f, 0.2f, 0.2f, 0.4f));
    marker_mat->set_transparency(StandardMaterial3D::TRANSPARENCY_ALPHA);
    marker->set_material(marker_mat);

    add_child(marker);
}

// ---------------------------------------------------------------------------
// Spawn cattle (unchanged except srand moved to _ready)
// ---------------------------------------------------------------------------
void HerdManager::spawn_cattle() {
    cattle_group = memnew(Node3D);
    cattle_group->set_name("CattleGroup");
    add_child(cattle_group);

    set_meta("field_size", field_size);

    float spawn_range = field_size * 0.3f;

    for (int i = 0; i < cattle_count; i++) {
        Cattle *cow = memnew(Cattle);

        float rx = ((float)(rand() % 1000) / 500.0f - 1.0f) * spawn_range;
        float rz = ((float)(rand() % 1000) / 500.0f - 1.0f) * spawn_range;

        cow->set_position(Vector3(rx, 0, rz));
        cattle_group->add_child(cow);
    }
}

// ---------------------------------------------------------------------------
// Spawn drone (unchanged)
// ---------------------------------------------------------------------------
void HerdManager::spawn_drone() {
    if (!cattle_group) return;

    Drone *drone = memnew(Drone);
    drone->set_target_paddock(target_position);

    Vector3 start_pos = -target_position.normalized() * (field_size * 0.3f);
    start_pos.y = drone->get_altitude();
    drone->set_position(start_pos);

    cattle_group->add_child(drone);
}

// ---------------------------------------------------------------------------
// _ready -- ordered creation
// ---------------------------------------------------------------------------
void HerdManager::_ready() {
    srand((unsigned int)time(nullptr));

    create_terrain();
    create_environment();
    create_fences();
    create_grass();
    create_trees();
    create_water_trough();
    create_rocks();
    create_target_marker();
    spawn_cattle();
    spawn_drone();
}

// ---------------------------------------------------------------------------
// _process (unchanged)
// ---------------------------------------------------------------------------
void HerdManager::_process(double delta) {
    if (herding_complete || !cattle_group) return;

    Vector3 com = Vector3(0, 0, 0);
    int count = 0;

    int child_count = cattle_group->get_child_count();
    for (int i = 0; i < child_count; i++) {
        Cattle *cow = Object::cast_to<Cattle>(cattle_group->get_child(i));
        if (cow) {
            com += cow->get_position();
            count++;
        }
    }

    if (count == 0) return;
    com /= (float)count;
    com.y = 0;

    float dist_to_target = com.distance_to(target_position);
    if (dist_to_target < target_radius) {
        herding_complete = true;
        UtilityFunctions::print("Herding complete! Cattle reached the target paddock.");

        Label3D *label = memnew(Label3D);
        label->set_text("HERDING COMPLETE");
        label->set_position(Vector3(0, 15, 0));
        label->set_font_size(72);
        label->set_billboard_mode(StandardMaterial3D::BILLBOARD_ENABLED);
        label->set_modulate(Color(0.1f, 0.9f, 0.1f));
        add_child(label);
    }
}

void HerdManager::set_cattle_count(int p_count) { cattle_count = p_count; }
int HerdManager::get_cattle_count() const { return cattle_count; }

void HerdManager::set_field_size(float p_size) { field_size = p_size; }
float HerdManager::get_field_size() const { return field_size; }

void HerdManager::set_target_position(Vector3 p_pos) { target_position = p_pos; }
Vector3 HerdManager::get_target_position() const { return target_position; }
