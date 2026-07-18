#include "parse/xmlparse.hpp"

#include <cmath>
#include <functional>
#include <stdexcept>
#include <tinyxml2.h>

#include "geometry/sphere.hpp"
#include "logging.hpp"
#include "math/matrix.hpp"
#include "illum/lights.hpp"
#include "illum/material.hpp"
#include "image.hpp"
#include "parse/objparse.hpp"
#include "types.hpp"
#include "random.hpp"

using namespace tinyxml2;
using std::operator""sv;

#define ELEM_WITH_PARENT(element, parent) \
    const XMLElement *element {(parent)->FirstChildElement(#element)}; \
    throw_if_nullptr(element, #element)

#define PARSE_ATTR(attrname, element, type, ptr_into) \
    throw_if_faulty_attr((element)->Query##type##Attribute((attrname), (ptr_into)), #element " > " attrname)

namespace {

static constexpr auto throw_if_nullptr {[](const void* ptr, std::string&& nodename) -> void {
    if (ptr == nullptr) {
        throw std::runtime_error("Element/Attribute " + nodename + " was not found in the given XML file.");
    }
}};

static constexpr auto throw_if_faulty_attr {[](XMLError error, std::string&& attrname) -> void {
    if (error != XML_SUCCESS) {
        throw std::runtime_error("Attribute " + attrname + " could not be found or was erroneous.");
    }
}};

static constexpr
auto deg_to_rad(f64 deg) -> f64 {
    return deg * std::numbers::pi / 180.0;
}

static constexpr
void parse_transform(
    HasTransform auto& body,
    const XMLElement *body_element,
    Mat4 *out_transmat = nullptr
) {
    const XMLElement *transform{(body_element)->FirstChildElement("transform")};
    if (transform == nullptr) {
        return;
    }

    Mat4 transmat_inv {};

    for (
        const XMLElement *transform_element {transform->FirstChildElement()};
        transform_element != nullptr;
        transform_element = transform_element->NextSiblingElement()
    ) {
        const char *name {transform_element->Name()};

        if (name == "translate"sv) {
            Vec displacement {};
            PARSE_ATTR("x", transform_element, Double, &displacement.x);
            PARSE_ATTR("y", transform_element, Double, &displacement.y);
            PARSE_ATTR("z", transform_element, Double, &displacement.z);

            transmat_inv = transmat_inv.translate_inv(displacement);
            if (out_transmat != nullptr) {
                *out_transmat = out_transmat->translate(displacement);
            }
            continue;
        }
        if (name == "scale"sv) {
            Vec scalings {};
            PARSE_ATTR("x", transform_element, Double, &scalings.x);
            PARSE_ATTR("y", transform_element, Double, &scalings.y);
            PARSE_ATTR("z", transform_element, Double, &scalings.z);

            transmat_inv = transmat_inv.scale_inv(scalings);
            if (out_transmat != nullptr) {
                *out_transmat = out_transmat->scale(scalings);
            }
            continue;
        }
        if (name == "rotateX"sv) {
            f64 theta {};
            PARSE_ATTR("theta", transform_element, Double, &theta);
            transmat_inv = transmat_inv.rotx_inv(deg_to_rad(theta));
            if (out_transmat != nullptr) {
                *out_transmat = out_transmat->rotx(deg_to_rad(theta));
            }
            continue;
        }
        if (name == "rotateY"sv) {
            f64 theta {};
            PARSE_ATTR("theta", transform_element, Double, &theta);
            transmat_inv = transmat_inv.roty_inv(deg_to_rad(theta));
            if (out_transmat != nullptr) {
                *out_transmat = out_transmat->roty(deg_to_rad(theta));
            }
            continue;
        }
        if (name == "rotateZ"sv) {
            f64 theta {};
            PARSE_ATTR("theta", transform_element, Double, &theta);
            transmat_inv = transmat_inv.rotz_inv(deg_to_rad(theta));
            if (out_transmat != nullptr) {
                *out_transmat = out_transmat->rotz(deg_to_rad(theta));
            }
            continue;
        }

        LOG("No parser found for element ", name);
    }

    body.set_ray_transformation(transmat_inv);
}

static constexpr
auto parse_colour(const XMLElement *parent) -> Colour {
    Colour colour {};

    ELEM_WITH_PARENT(color, parent);
    PARSE_ATTR("r", color, Double, &colour.r);
    PARSE_ATTR("g", color, Double, &colour.g);
    PARSE_ATTR("b", color, Double, &colour.b);

    return colour;
}

static constexpr
void parse_material(
    lum::HasMaterial auto& body,
    const XMLElement *body_element,
    const std::filesystem::path& xmlfilepath
) {
    const XMLElement *material_solid {body_element->FirstChildElement("material_solid")};
    const XMLElement *material_textured {body_element->FirstChildElement("material_textured")};
    if (material_solid == nullptr && material_textured == nullptr) {
        throw std::runtime_error("One body has no material in the given XML file.");
    }

    const XMLElement *material_element {material_solid == nullptr ? material_textured : material_solid};

    lum::BaseMaterial base {};

    ELEM_WITH_PARENT(phong, material_element);
    PARSE_ATTR("ka", phong, Double, &base.ka);
    PARSE_ATTR("kd", phong, Double, &base.kd);
    PARSE_ATTR("ks", phong, Double, &base.ks);
    PARSE_ATTR("exponent", phong, Unsigned, &base.exponent);

    ELEM_WITH_PARENT(reflectance, material_element);
    PARSE_ATTR("r", reflectance, Double, &base.reflectance);

    ELEM_WITH_PARENT(transmittance, material_element);
    PARSE_ATTR("t", transmittance, Double, &base.transmittance);

    ELEM_WITH_PARENT(refraction, material_element);
    PARSE_ATTR("iof", refraction, Double, &base.refraction);

    const char *name {material_element->Name()};
    if (name == "material_solid"sv) {
        Colour colour {parse_colour(material_element)};
        body.set_material(lum::MaterialSolid {base, colour});
        return;
    }
    if (name == "material_textured"sv) {
        ELEM_WITH_PARENT(texture, material_element);
        const char *texture_basefilename {texture->Attribute("name")};
        throw_if_nullptr(texture_basefilename, "texture > name");

        std::filesystem::path texture_fullpath {xmlfilepath.parent_path() / texture_basefilename};
        auto&& [rawimage, imagewidth] {png::decode_image(texture_fullpath)};
        body.set_material(lum::MaterialTextured {base, rawimage, imagewidth});
        return;
    }

    LOG("No parser found for element ", name);
}

static constexpr
void parse_sphere(
    std::vector<Sphere>& spheres,
    const XMLElement *sphere_element,
    const std::filesystem::path& xmlfilepath
) {
    Sphere sphere {};

    PARSE_ATTR("radius", sphere_element, Double, &sphere.radius);

    ELEM_WITH_PARENT(position, sphere_element);
    PARSE_ATTR("x", position, Double, &sphere.position.x);
    PARSE_ATTR("y", position, Double, &sphere.position.y);
    PARSE_ATTR("z", position, Double, &sphere.position.z);

    parse_material(sphere, sphere_element, xmlfilepath);

    Mat4 transmat {};
    parse_transform(sphere, sphere_element, &transmat);

    Coor position_world {transmat.mul(sphere.position)};
    auto cornervecs_object {std::views::iota(0, 8)
        | std::views::transform([&](auto mask) -> Vec {
            return Vec {
                .x = ((mask & 1) != 0 ? -1 : 1) * sphere.radius,
                .y = ((mask & 2) != 0 ? -1 : 1) * sphere.radius,
                .z = ((mask & 4) != 0 ? -1 : 1) * sphere.radius,
            };
    })};
    auto cornervecs_world {cornervecs_object | std::views::transform(std::bind_front(static_cast<Vec (Mat4::*)(Vec) const>(&Mat4::mul), transmat))};
    auto corners_world {cornervecs_world | std::views::transform(std::bind_front(&Coor::add, position_world))};

    AABB first_corner {
        .max = corners_world.front(),
        .min = corners_world.front(),
    };
    sphere.bounds_world = std::ranges::fold_left(corners_world, first_corner, std::mem_fn(static_cast<AABB (AABB::*)(Coor) const>(&AABB::merge)));

    /*
    Coor min {corners_world.front()};
    Coor max {corners_world.front()};
    for (Coor corner : corners_world) {
        min = Coor {
            .x = std::min(min.x, corner.x),
            .y = std::min(min.y, corner.y),
            .z = std::min(min.z, corner.z),
        };
        max = Coor {
            .x = std::max(max.x, corner.x),
            .y = std::max(max.y, corner.y),
            .z = std::max(max.z, corner.z),
        };
    }

    sphere.bounds_world = AABB {
        .max = max,
        .min = min,
    };
    */

    spheres.emplace_back(sphere);
}

static constexpr
void parse_mesh(
    std::vector<Mesh>& meshes,
    const XMLElement *mesh_element,
    const std::filesystem::path& xmlfilepath
) {
    const char *basefilename {mesh_element->Attribute("name")};
    throw_if_nullptr(basefilename, "mesh > name");

    std::filesystem::path fullpath {xmlfilepath.parent_path() / basefilename};

    Mesh& mesh {meshes.emplace_back(Mesh {})};
    obj::parse(mesh, fullpath);
    parse_material(mesh, mesh_element, xmlfilepath);

    Mat4 transmat {};
    parse_transform(mesh, mesh_element, &transmat);

    std::ranges::for_each(mesh.triangles, [&](Mesh::Triangle& triangle) -> void {
        Coor v1_worldpos {transmat.mul(triangle.get_pos(triangle.v1))};
        Coor v2_worldpos {transmat.mul(triangle.get_pos(triangle.v2))};
        Coor v3_worldpos {transmat.mul(triangle.get_pos(triangle.v3))};

        triangle.bounds_world = AABB {
            .max = Coor {
                .x = std::max({v1_worldpos.x, v2_worldpos.x, v3_worldpos.x}),
                .y = std::max({v1_worldpos.y, v2_worldpos.y, v3_worldpos.y}),
                .z = std::max({v1_worldpos.z, v2_worldpos.z, v3_worldpos.z}),
            },
            .min = Coor {
                .x = std::min({v1_worldpos.x, v2_worldpos.x, v3_worldpos.x}),
                .y = std::min({v1_worldpos.y, v2_worldpos.y, v3_worldpos.y}),
                .z = std::min({v1_worldpos.z, v2_worldpos.z, v3_worldpos.z}),
            },
        };
    });
}

static constexpr
auto compute_camera_ray_rotation(Coor camera, Coor lookat, Vec up) -> Mat3 {
    Vec Z {Coor::to_vec(lookat, camera).normalise()};
    Vec X {Vec::cross(up, Z).normalise()};
    Vec Y {Vec::cross(Z, X).normalise()};

    return Mat3::from_columns(X, Y, Z);
}

static constexpr
auto generate_triangle_samples(
    const Mesh::Triangle& triangle,
    const Mat4& worldtransmat,
    const Mat3& normalmat
) -> std::array<std::pair<Coor, Vec>, lum::AreaLight::NUM_SAMPLES_PER_TRIANGLE> {
    const auto generate_one_sample {[&]() -> std::pair<Coor, Vec> {
        f64 rand1 {utils::get_uniform_float_0_to_1()};
        f64 rand2 {utils::get_uniform_float_0_to_1()};
        f64 sqrt_rand1 {std::sqrt(rand1)};

        // Barycentric coordinates
        f64 a {1 - sqrt_rand1};
        f64 b {rand2 * sqrt_rand1};

        const Coor& v1 {worldtransmat.mul(triangle.get_pos(triangle.v1))};
        const Coor& v2 {worldtransmat.mul(triangle.get_pos(triangle.v2))};
        const Coor& v3 {worldtransmat.mul(triangle.get_pos(triangle.v3))};

        Coor point_worldspace {
            .x = (1 - a - b) * v1.x + a * v2.x + b * v3.x,
            .y = (1 - a - b) * v1.y + a * v2.y + b * v3.y,
            .z = (1 - a - b) * v1.z + a * v2.z + b * v3.z,
        };

        Vec n1 {triangle.get_normal(triangle.v1)};
        Vec n2 {triangle.get_normal(triangle.v2)};
        Vec n3 {triangle.get_normal(triangle.v3)};
        Vec normal_objspace {Vec::add(
            n1.scale(1 - a - b),
            n2.scale(a),
            n3.scale(b)
        )};

        Vec normal_worldspace {Mat3::mul(normalmat, normal_objspace).normalise()};
        assert(normal_worldspace.magnitude() == 1.0_eps);

        return std::make_pair(point_worldspace, normal_worldspace);
    }};

    std::array<std::pair<Coor, Vec>, lum::AreaLight::NUM_SAMPLES_PER_TRIANGLE> samples {{}};
    for (usize sample_idx {}; sample_idx < lum::AreaLight::NUM_SAMPLES_PER_TRIANGLE; ++sample_idx) {
        samples[sample_idx] = generate_one_sample();
    }

    return samples;
}

template<typename BvhConstructor>
static constexpr
void parse_into_scene(
    xml::Scene& resultscene,
    const std::filesystem::path& xmlfilepath,
    BvhConstructor bvh_constructor
) {
    XMLDocument xmldoc {};
    XMLError xmlerror;

    xmldoc.LoadFile(xmlfilepath.c_str());
    if (xmldoc.Error()) {
        throw std::runtime_error(std::string {"Error when loading xml file."} + xmldoc.ErrorStr());
    }

    // Scene

    const XMLElement *scene {xmldoc.FirstChildElement("scene")};
    throw_if_nullptr(scene, "scene");

    const char *output_file {scene->Attribute("output_file")};
    throw_if_nullptr(output_file, "scene > output_file");
    resultscene.outputfile = std::filesystem::path {output_file};

    ELEM_WITH_PARENT(background_color, scene);

    PARSE_ATTR("r", background_color, Double, &resultscene.background_colour.r);
    PARSE_ATTR("g", background_color, Double, &resultscene.background_colour.g);
    PARSE_ATTR("b", background_color, Double, &resultscene.background_colour.b);

    // Camera

    ELEM_WITH_PARENT(camera, scene);

    ELEM_WITH_PARENT(position, camera);
    PARSE_ATTR("x", position, Double, &resultscene.camera.position.x);
    PARSE_ATTR("y", position, Double, &resultscene.camera.position.y);
    PARSE_ATTR("z", position, Double, &resultscene.camera.position.z);

    ELEM_WITH_PARENT(lookat, camera);
    PARSE_ATTR("x", lookat, Double, &resultscene.camera.lookat.x);
    PARSE_ATTR("y", lookat, Double, &resultscene.camera.lookat.y);
    PARSE_ATTR("z", lookat, Double, &resultscene.camera.lookat.z);

    ELEM_WITH_PARENT(up, camera);
    PARSE_ATTR("x", up, Double, &resultscene.camera.up.x);
    PARSE_ATTR("y", up, Double, &resultscene.camera.up.y);
    PARSE_ATTR("z", up, Double, &resultscene.camera.up.z);

    resultscene.camera.ray_rotation = compute_camera_ray_rotation(
        resultscene.camera.position,
        resultscene.camera.lookat,
        resultscene.camera.up
    );

    ELEM_WITH_PARENT(resolution, camera);
    PARSE_ATTR("horizontal", resolution, Unsigned, &resultscene.camera.resolution_horizontal);
    PARSE_ATTR("vertical", resolution, Unsigned, &resultscene.camera.resolution_vertical);

    ELEM_WITH_PARENT(horizontal_fov, camera);

    f64 angle_deg {0.0};
    xmlerror = horizontal_fov->QueryDoubleAttribute("angle", &angle_deg);
    throw_if_faulty_attr(xmlerror, "horizontal_fov > angle");
    resultscene.camera.set_fovs_from_angle_deg(angle_deg);

    ELEM_WITH_PARENT(max_bounces, camera);
    PARSE_ATTR("n", max_bounces, Unsigned, &resultscene.camera.max_bounces);

    const XMLElement *dof {camera->FirstChildElement("dof")};
    if (dof != nullptr) {
        PARSE_ATTR("distance", dof, Double, &resultscene.camera.focal_distance);
        PARSE_ATTR("weakness", dof, Double, &resultscene.camera.weakness);
    }

    const XMLElement *render_times {camera->FirstChildElement("render_times")};
    if (render_times != nullptr) {
        PARSE_ATTR("n", render_times, Unsigned, &resultscene.camera.render_times);
    }

    // Light

    std::vector<Sphere> spheres {};
    std::vector<Mesh> meshes {};

    ELEM_WITH_PARENT(lights, scene);

    for (
        const XMLElement *light_element {lights->FirstChildElement()};
        light_element != nullptr;
        light_element = light_element->NextSiblingElement()
    ) {
        const char *name {light_element->Name()};
        lum::BaseLight base {parse_colour(light_element)};

        if (name == "ambient_light"sv) {
            resultscene.lights.emplace_back(lum::AmbientLight {base});
            continue;
        }
        if (name == "parallel_light"sv) {
            ELEM_WITH_PARENT(direction, light_element);

            Vec directionvec {};
            PARSE_ATTR("x", direction, Double, &directionvec.x);
            PARSE_ATTR("y", direction, Double, &directionvec.y);
            PARSE_ATTR("z", direction, Double, &directionvec.z);
            directionvec = directionvec.normalise();

            resultscene.lights.emplace_back(lum::ParallelLight {base, directionvec});
            continue;
        }
        if (name == "point_light"sv) {
            ELEM_WITH_PARENT(position, light_element);

            Coor positioncoor {};
            PARSE_ATTR("x", position, Double, &positioncoor.x);
            PARSE_ATTR("y", position, Double, &positioncoor.y);
            PARSE_ATTR("z", position, Double, &positioncoor.z);

            resultscene.lights.emplace_back(lum::PointLight {base, positioncoor});
            continue;
        }
        if (name == "spot_light"sv) {
            ELEM_WITH_PARENT(position, light_element);
            Coor positioncoor {};
            PARSE_ATTR("x", position, Double, &positioncoor.x);
            PARSE_ATTR("y", position, Double, &positioncoor.y);
            PARSE_ATTR("z", position, Double, &positioncoor.z);

            ELEM_WITH_PARENT(direction, light_element);
            Vec directionvec {};
            PARSE_ATTR("x", direction, Double, &directionvec.x);
            PARSE_ATTR("y", direction, Double, &directionvec.y);
            PARSE_ATTR("z", direction, Double, &directionvec.z);
            directionvec = directionvec.normalise();

            ELEM_WITH_PARENT(falloff, light_element);
            f64 alpha_falloff_begin_deg {};
            f64 alpha_falloff_end_deg {};
            PARSE_ATTR("alpha1", falloff, Double, &alpha_falloff_begin_deg);
            PARSE_ATTR("alpha2", falloff, Double, &alpha_falloff_end_deg);

            resultscene.lights.emplace_back(lum::SpotLight {
                base, positioncoor, directionvec,
                deg_to_rad(alpha_falloff_begin_deg),
                deg_to_rad(alpha_falloff_end_deg),
            });
            continue;
        }
        if (name == "area_light"sv) {
            const char *basefilename {light_element->Attribute("objfile")};
            throw_if_nullptr(basefilename, "area_light > objfile");

            std::filesystem::path fullpath {xmlfilepath.parent_path() / basefilename};
            Mesh& mesh {meshes.emplace_back(Mesh {})};
            obj::parse(mesh, fullpath);

            mesh.set_material(lum::MaterialSolid {
                lum::BaseMaterial {
                    .ka = 1.0,
                    .kd = 0.0,
                    .ks = 0.0,
                    .reflectance = 0.0,
                    .transmittance = 0.0,
                    .refraction = 0.0,
                    .exponent = 0,
                },
                base.colour
            });

            Mat4 transmat {};
            parse_transform(mesh, light_element, &transmat);

            std::ranges::for_each(mesh.triangles, [&](Mesh::Triangle& triangle) -> void {
                Coor v1_worldpos {transmat.mul(triangle.get_pos(triangle.v1))};
                Coor v2_worldpos {transmat.mul(triangle.get_pos(triangle.v2))};
                Coor v3_worldpos {transmat.mul(triangle.get_pos(triangle.v3))};

                triangle.bounds_world = AABB {
                    .max = Coor {
                        .x = std::max({v1_worldpos.x, v2_worldpos.x, v3_worldpos.x}),
                        .y = std::max({v1_worldpos.y, v2_worldpos.y, v3_worldpos.y}),
                        .z = std::max({v1_worldpos.z, v2_worldpos.z, v3_worldpos.z}),
                    },
                    .min = Coor {
                        .x = std::min({v1_worldpos.x, v2_worldpos.x, v3_worldpos.x}),
                        .y = std::min({v1_worldpos.y, v2_worldpos.y, v3_worldpos.y}),
                        .z = std::min({v1_worldpos.z, v2_worldpos.z, v3_worldpos.z}),
                    },
                };
            });
            std::vector<std::array<std::pair<Coor, Vec>, lum::AreaLight::NUM_SAMPLES_PER_TRIANGLE>> samples {};
            samples.reserve(mesh.triangles.size());
            Mat3 normalmat {mesh.ray_transformation.transpose().to_mat3()};

            std::ranges::for_each(mesh.triangles, [&](const Mesh::Triangle& triangle) -> void {
                samples.emplace_back(generate_triangle_samples(triangle, transmat, normalmat));
            });

            resultscene.lights.emplace_back(lum::AreaLight {
                .mesh_idx = meshes.size() - 1, 
                .sample_vertices_normals = samples,
            });
            continue;
        }

        LOG("No parser found for element ", name);
    }

    // Geometry

    ELEM_WITH_PARENT(surfaces, scene);

    for (
        const XMLElement *surface_element {surfaces->FirstChildElement()};
        surface_element != nullptr;
        surface_element = surface_element->NextSiblingElement()
    ) {
        const char *name {surface_element->Name()};

        if (name == "sphere"sv) {
            parse_sphere(spheres, surface_element, xmlfilepath);
            continue;
        }
        if (name == "mesh"sv) {
            parse_mesh(meshes, surface_element, xmlfilepath);
            continue;
        }

        LOG("No parser found for element ", name);
    }

    // Fix dangling mesh use after resultscene.meshes may have relocated,
    // I love the address sanitizer
    for (auto& mesh : meshes) {
        for (auto& triangle : mesh.triangles) {
            triangle.parentmesh = &mesh;
        }
    }

    resultscene.intersectables_bvh = bvh_constructor(std::move(spheres), std::move(meshes));
}

}

auto xml::parse_no_bvh(const std::filesystem::path& xmlfilepath) -> xml::Scene {
    xml::Scene resultscene {};
    parse_into_scene(resultscene, xmlfilepath, bvh::BVH<Sphere, Mesh>::construct_simple_traversal_without_bvh);
    return resultscene;
}

auto xml::parse_with_bvh(const std::filesystem::path& xmlfilepath) -> xml::Scene {
    xml::Scene resultscene {};
    parse_into_scene(resultscene, xmlfilepath, bvh::BVH<Sphere, Mesh>::construct_bvh_with_sah);
    return resultscene;
}

void xml::Camera::set_fovs_from_angle_deg(f64 fovh_deg) {
    if (this->resolution_horizontal == 0 || this->resolution_vertical == 0) {
        WARN("FOV has not been set as aspect is faulty (resolutions are 0).");
        return;
    }

    f64 fovh_rad {2 * std::numbers::pi * fovh_deg / 360};
    f64 fovv_rad {(fovh_rad * this->resolution_horizontal) / this->resolution_vertical};

    this->tan_fovh = std::tan(fovh_rad);
    this->tan_fovv = std::tan(fovv_rad);
}

