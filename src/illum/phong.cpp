#include "illum/phong.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

#include "geometry/ray.hpp"
#include "logging.hpp"
#include "math/vec.hpp"
#include "parse/xmlparse.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace {

struct PhongPerLight {
    const Intersection& intersection {};
    const lum::Material& material {};
    const xml::Scene& xmlscene {};
    lum::MaterialGetColour material_get_colour {};

    constexpr
    auto calculate_diffuse_specular_with_shadowray(Vec L, Colour lightcolour, f64 max_t = std::numeric_limits<f64>::infinity()) -> Colour {
        // Cast shadowray

        Ray shadowray {
            .origin = this->intersection.point,
            .direction = L,
        };
        Intersection shadowray_intersection {};
        /*
        isec::determine_intersections(this->xmlscene.spheres, shadowray, shadowray_intersection);
        isec::determine_intersections(this->xmlscene.meshes, shadowray, shadowray_intersection);
        */
        this->xmlscene.intersectables_bvh.determine_intersections(shadowray_intersection, shadowray);

        if (shadowray_intersection.has_intersected() && shadowray_intersection.t < utils::PermissiveFloat {max_t}) {
            return Colour::BLACK;
        }

        Colour result_colour {Colour::BLACK};

        Colour material_colour {this->material | this->material_get_colour};
        Colour blended_colour {Colour::blend(material_colour, lightcolour)};

        // Compute diffuse and specular component

        f64 kd {this->material | lum::get_kd};
        f64 ks {this->material | lum::get_ks};

        f64 cos_light_normal {Vec::dot(L, this->intersection.normal)};
        if (kd > 0.0 && cos_light_normal > 0.0) {
            Colour diffuse_colour {blended_colour.scale(kd * cos_light_normal)};
            result_colour = Colour::saturating_add(result_colour, diffuse_colour);
        }

        if (ks > 0.0 && cos_light_normal > 0.0) {
            const Vec& V {this->intersection.incident_ray_direction.scale(-1.0)};
            Vec R {Vec::subtract(this->intersection.normal.scale(cos_light_normal * 2), L)};

            u32 exponent {this->material | lum::get_exponent};
            f64 cos_r_v {std::max(Vec::dot(R, V), 0.0)};

            result_colour = Colour::saturating_add(result_colour, lightcolour.scale(ks * std::pow(cos_r_v, exponent)));
        }

        return result_colour;
    }

    constexpr
    auto operator()(const lum::ParallelLight& light) -> Colour {
        Vec L {light.direction.scale(-1)};
        return this->calculate_diffuse_specular_with_shadowray(L, light.colour);
    }

    constexpr
    auto operator()(const lum::PointLight& light) -> Colour {
        Vec lightvec {Coor::to_vec(this->intersection.point, light.position)};
        f64 max_t {lightvec.magnitude()};
        return this->calculate_diffuse_specular_with_shadowray(lightvec.normalise(), light.colour, max_t);
    }

    constexpr
    auto operator()(const lum::AmbientLight& light) -> Colour {
        Colour material_colour {this->material | this->material_get_colour};
        Colour blended_colour {Colour::blend(material_colour, light.colour)};
        f64 ka {this->material | lum::get_ka};
        return blended_colour.scale(ka);
    }

    constexpr
    auto operator()(const lum::SpotLight& light) -> Colour {
        Vec lightvec {Coor::to_vec(this->intersection.point, light.position)};
        f64 max_t {lightvec.magnitude()};

        Vec lightvec_norm {lightvec.normalise()};
        f64 falloff_cos {-Vec::dot(lightvec_norm, light.direction)};
        f64 falloff_rad {std::acos(falloff_cos)};

        if (falloff_rad >= light.alpha_falloff_end) {
            return Colour::BLACK;
        }

        f64 falloff_scale {(falloff_rad <= light.alpha_falloff_begin)
                ? 1.0
                : 1.0 - ((falloff_rad - light.alpha_falloff_begin)
                    / (light.alpha_falloff_end - light.alpha_falloff_begin))};
        return this->calculate_diffuse_specular_with_shadowray(lightvec_norm, light.colour, max_t).scale(falloff_scale);
    }

    constexpr
    auto operator()(const lum::AreaLight& light) -> Colour {
        const std::vector<Mesh>& scenemeshes {std::get<std::vector<Mesh>>(xmlscene.intersectables_bvh.intersectables)};
        const Mesh& mesh {scenemeshes[light.mesh_idx]};
        const usize num_triangles {mesh.triangles.size()};

        const auto is_intersection_in_front {[&](const auto& item) -> bool {
            auto&& [sample_point, normal] {item};
            Vec lightvec {Coor::to_vec(this->intersection.point, sample_point)};
            return Vec::dot(lightvec, normal) < 0;
        }};

        const auto calculate_illum {[&, num_triangles](const auto& item) -> Colour {
            auto&& [sample_point, normal] {item};
            Vec lightvec {Coor::to_vec(this->intersection.point, sample_point)};
            f64 max_t {lightvec.magnitude()};

            Colour lightcolour {mesh.material | this->material_get_colour};
            Colour illum_colour {this->calculate_diffuse_specular_with_shadowray(lightvec.normalise(), lightcolour, max_t)};
            return illum_colour.scale((1.0 / lum::AreaLight::NUM_SAMPLES_PER_TRIANGLE) / static_cast<f64>(num_triangles));
        }};

        auto computed_illuminations {
            light.sample_vertices_normals
            | std::views::join
            | std::views::filter(is_intersection_in_front)
            | std::views::transform(calculate_illum)
        };

        return std::ranges::fold_left(computed_illuminations, Colour::BLACK, Colour::saturating_add);
    }
};

}

auto lum::get_phong_colour(
    const xml::Scene& xmlscene,
    const lum::Material& material,
    const Intersection& intersection
) -> Colour {
    Colour resultcolour;
    PhongPerLight phong_functor {
        .intersection = intersection,
        .material = material,
        .xmlscene = xmlscene,
        .material_get_colour = MaterialGetColour {
            .u = intersection.u,
            .v = intersection.v,
        },
    };

    std::ranges::for_each(xmlscene.lights, [&](const auto& light) -> void {
        Colour colour {light | phong_functor};
        resultcolour = Colour::saturating_add(resultcolour, colour);
    });

    return resultcolour;
}

