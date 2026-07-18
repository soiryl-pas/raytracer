#include "traceray.hpp"

#include <cassert>
#include <cmath>
#include <execution>
#include <functional>
#include <iostream>
#include <numbers>
#include <ranges>
#include <vector>

#include "geometry/ray.hpp"
#include "illum/phong.hpp"
#include "image.hpp"
#include "intersect.hpp"
#include "logging.hpp"
#include "parse/xmlparse.hpp"
#include "random.hpp"
#include "utils.hpp"

namespace {

static constexpr
auto get_initial_framebuffer(const xml::Scene& xmlscene) -> std::vector<Colour> {
    return {xmlscene.camera.num_pixel(), Colour::BLACK };
}

static constexpr
auto get_direction_view_from_uv(
    usize width,
    usize height,
    f64 tan_fovh,
    f64 tan_fovv
) {
    const auto normalise_and_center_uv {[=](auto uv) {
        auto&& [u, v] {uv};
        return std::array<f64, 2> {
            (static_cast<f64>(u) + 0.5) / static_cast<f64>(width),
            (static_cast<f64>(v) + 0.5) / static_cast<f64>(height)
        };
    }};

    static constexpr auto map_to_image_plane {[](auto xy) {
        auto&& [x, y] {xy};
        return std::array<f64, 2> { 2 * x - 1, 2 * y - 1 };
    }};

    const auto apply_fov {[=](auto xy) {
        auto [x, y] {xy};
        return std::array<f64, 2> { x * tan_fovh, y * tan_fovv };
    }};

    static constexpr auto pair_to_normalised_direction {[](auto xy) -> Vec {
        auto [x, y] {xy};
        return Vec {x, y, -1}.normalise();
    }};

    auto composed_transforms {
        std::views::transform(normalise_and_center_uv)
        | std::views::transform(map_to_image_plane)
        | std::views::transform(apply_fov)
        | std::views::transform(pair_to_normalised_direction)
    };
    return composed_transforms;
}

static constexpr
auto get_camera_rays(
    const xml::Camera& camera,
    std::vector<Colour>& framebuffer
) -> std::vector<PixelRay> {
    const usize width  {camera.resolution_horizontal};
    const usize height {camera.resolution_vertical};
    const f64 tan_fovh {camera.tan_fovh};
    const f64 tan_fovv {camera.tan_fovv};
    const Coor& origin  {camera.position};
    const Mat3& ray_rotation {camera.ray_rotation};
    const f64 focal_distance {camera.focal_distance};
    const u32 render_times {camera.render_times};

    const auto idx_to_uv {[=](usize idx) -> std::array<usize, 2> {
        return { idx % width, idx / width };
    }};

    const auto camera_direction_to_ray {[&](auto&& item) -> PixelRay {
        auto&& [index, camera_direction] {item};
        index = index % framebuffer.size();

        Coor rayorigin {origin};
        Vec raydirection {Mat3::mul(ray_rotation, camera_direction)};

        // Displace origin for depth of field
        if (focal_distance != 0.0) {
            f64 radius {utils::get_uniform_float_0_to_1() / camera.weakness};
            f64 theta {utils::get_uniform_float_0_to_1() * 2 * std::numbers::pi};

            Vec disp {
                .x = std::cos(theta) * radius,
                .y = std::sin(theta) * radius,
                .z = 0
            };

            /*
            Coor image_point {Coor {}.add(disp).add(camera_direction.scale(-1.0 / camera_direction.z))};
            Coor uv_plane_point { .x = (image_point.x / tan_fovh + 1.0) / 2.0, .y = (image_point.y / tan_fovv + 1.0) / 2.0, .z = -1.0};
            usize index_x {static_cast<usize>(std::round(uv_plane_point.x * static_cast<f64>(width) - 0.5))};
            usize index_y {static_cast<usize>(std::round(uv_plane_point.y * static_cast<f64>(height) - 0.5))};
            index = index_x + index_y * width;

            if (index >= framebuffer.size()) { index = framebuffer.size() - 1; }
            */

            Vec disp_world {Mat3::mul(ray_rotation, disp)};
            rayorigin = origin.add(disp_world);

            Vec focal_vec {raydirection.scale(-focal_distance / camera_direction.z )};
            //Vec focal_vec {raydirection.scale(focal_distance)};
            raydirection = Vec {
                .x = origin.x + focal_vec.x - rayorigin.x,
                .y = origin.y + focal_vec.y - rayorigin.y,
                .z = origin.z + focal_vec.z - rayorigin.z,
            }.normalise();
        }

        return PixelRay {
            Ray {
                .origin = rayorigin,
                .direction = raydirection,
            },
            framebuffer[static_cast<usize>(index)],
            1.0 / static_cast<f64>(render_times),
        };
    }};

    return std::views::repeat(std::views::iota(0uz, framebuffer.size()), render_times)
        | std::views::join
        | std::views::transform(idx_to_uv)
        | get_direction_view_from_uv(width, height, tan_fovh, tan_fovv)
        | std::views::enumerate
        | std::views::transform(camera_direction_to_ray)
        | utils::view_to_vec;
}

/**
 * Refracts the ingoing vector according to the given parameters.
 * Also takes total internal reflection into account.
 * It only models refraction between material and air with iof = 1.0.
 */
static constexpr
auto refract(Vec ingoing, Vec normal, f64 material_iof) -> Vec {
    f64 cos_ingoing_normal {- Vec::dot(ingoing, normal)};

    f64 iof_prop {cos_ingoing_normal > 0.0 ? 1.0 / material_iof : material_iof / 1.0};
    f64 discriminant {1 - iof_prop * iof_prop * (1 - cos_ingoing_normal * cos_ingoing_normal)};

    if (discriminant < 0.0) {
        // Total internal reflection
        Vec reflect_direction {Vec::add(normal.scale(2 * cos_ingoing_normal), ingoing)};
        return reflect_direction;
    }

    Vec t_orth {normal
        .scale(std::sqrt(discriminant))
        .scale(cos_ingoing_normal < 0.0 ? 1.0 : -1.0)
    };
    Vec t_par {Vec::add(ingoing, normal.scale(cos_ingoing_normal)).scale(iof_prop)};
    return Vec::add(t_orth, t_par);
}

static constexpr
void trace_rays_once(
    const xml::Scene& xmlscene,
    std::vector<PixelRay>& rays,
    std::vector<PixelIntersection>& intersections
) {
    assert (rays.size() == intersections.size());
    auto rays_and_intersects {std::views::zip(rays, intersections)};

    // Update pixel with lights and ray multipliers
    const auto update_ray_colour_with_phong {[&](PixelIntersection& intersection) -> void {
        Colour dimmed_phongcolour {xmlscene.background_colour.scale(intersection.colour_multiplier)};

        if (intersection.has_intersected()) {
            const lum::Material& material {*intersection.material};
            f64 phong_contribution {material | lum::get_phong_contribution};
            Colour phongcolour {lum::get_phong_colour(xmlscene, material, intersection)};
            dimmed_phongcolour = phongcolour.scale(phong_contribution * intersection.colour_multiplier);
        }

        *intersection.pixel = Colour::saturating_add(*intersection.pixel, dimmed_phongcolour);
    }};

    LOG("Determine intersections");
    std::for_each(std::execution::par_unseq, rays_and_intersects.begin(), rays_and_intersects.end(), [&](auto&& item) -> void {
        auto&& [ray, intersection] {item};
        xmlscene.intersectables_bvh.determine_intersections(intersection, ray);
        /*
        isec::determine_intersections(xmlscene.spheres, ray, intersection);
        isec::determine_intersections(xmlscene.meshes, ray, intersection);
        */
    });

    rays.clear();

    LOG("Updating colours");
    std::ranges::for_each(intersections, update_ray_colour_with_phong);

    const auto update_rays_reflect_and_refract {[&](const PixelIntersection& intersection) -> void {
        assert(intersection.has_intersected());

        f64 cos_normal_incident {- Vec::dot(intersection.incident_ray_direction, intersection.normal)};

        // Reflection

        f64 reflectance {*intersection.material | lum::get_reflectance};
        if (reflectance > 0.0 && cos_normal_incident >= 0.0) {
            Vec reflect_direction {Vec::add(intersection.normal.scale(2 * cos_normal_incident), intersection.incident_ray_direction)};

            assert(reflect_direction.magnitude() == 1.0_eps);

            rays.emplace_back(
                Ray {
                    .origin = intersection.point,
                    .direction = reflect_direction,
                },
                *intersection.pixel,
                reflectance * intersection.colour_multiplier
            );
        }

        // Refraction

        f64 transmittance {*intersection.material | lum::get_transmittance};
        if (transmittance > 0.0) {
            Vec refract_direction {refract(
                intersection.incident_ray_direction,
                intersection.normal,
                *intersection.material | lum::get_refraction
            )};

            assert(refract_direction.magnitude() == 1.0_eps);

            rays.emplace_back(
                Ray {
                    .origin = intersection.point,
                    .direction = refract_direction,
                },
                *intersection.pixel,
                transmittance * intersection.colour_multiplier
            );
        }
    }};

    LOG("Pushing reflections and refractions as new rays");
    auto valid_intersections {intersections | std::views::filter(&Intersection::has_intersected)};
    std::ranges::for_each(valid_intersections, update_rays_reflect_and_refract);

    intersections.resize(rays.size());
    std::for_each(std::execution::par_unseq, intersections.begin(), intersections.end(), std::mem_fn(&Intersection::set_no_intersection));
}

}

auto trace::trace_rays(const xml::Scene& xmlscene) -> png::Image {
    std::vector<Colour> framebuffer {get_initial_framebuffer(xmlscene)};
    LOG("Framebuffer with ", framebuffer.size(), " pixels.");

    std::vector<PixelRay> camera_rays {get_camera_rays(xmlscene.camera, framebuffer)};
    std::vector<PixelIntersection> intersections {camera_rays.size(), PixelIntersection {}};

    camera_rays.reserve(camera_rays.size() * 8);
    intersections.reserve(camera_rays.capacity());

    for (u32 bounce {}; bounce <= xmlscene.camera.max_bounces; bounce++) {
        std::cout << "Bounce " << bounce;
        if (bounce == 0) {
            std::cout << " (Camera)";
        }
        std::cout << " with " << camera_rays.size() << " rays.\n";
        trace_rays_once(xmlscene, camera_rays, intersections);
    }

    std::vector<u8> rgbvec {
        framebuffer
        | std::views::transform(std::mem_fn(&Colour::operator std::array<u8, 3>))
        | std::views::join
        | utils::view_to_vec
    };

    return std::make_pair(rgbvec, xmlscene.camera.resolution_horizontal);
}

// DBG

auto trace::dbg::colour_by_ray_dir() -> png::Image {
    static constexpr Coor ORIGIN {};
    static constexpr usize WIDTH {1000uz};
    static constexpr usize HEIGHT {1000uz};
    static constexpr f64 FOVH {std::numbers::pi / 4.0};

    static constexpr usize NUM_RAYS {WIDTH * HEIGHT};
    static constexpr f64 FOVV {(FOVH * HEIGHT) / WIDTH};

    const f64 tan_fovh {std::tan(FOVH)};
    const f64 tan_fovv {std::tan(FOVV)};

    static constexpr auto idx_to_uv {[](usize idx) -> std::array<usize, 2> {
        return { idx % WIDTH, idx / WIDTH };
    }};

    const auto direction_to_ray {[&](Vec direction) -> Ray {
        return { .origin = ORIGIN, .direction = direction };
    }};

    std::vector<Ray> rays {
        std::views::iota(0uz, NUM_RAYS)
        | std::views::transform(idx_to_uv)
        | get_direction_view_from_uv(WIDTH, HEIGHT, tan_fovh, tan_fovv)
        | std::views::transform(direction_to_ray)
        | utils::view_to_vec
    };

    // Use ray direction as colour

    static constexpr auto dir_f64_to_u8 {[](f64 val) -> u8 {
        f64 norm_val {(val + 1.0) / 2.0};
        return static_cast<u8>(norm_val * 0xff);
    }};

    static constexpr auto ray_to_rgb {[](const Ray& ray) -> std::array<u8, 3> {
        return {
            dir_f64_to_u8(ray.direction.x),
            dir_f64_to_u8(ray.direction.y),
            0,
        };
    }};

    std::vector<u8> rgbvec {
        rays
        | std::views::transform(ray_to_rgb)
        | std::views::join
        | utils::view_to_vec
    };

    return std::make_pair(rgbvec, WIDTH);
}

auto trace::dbg::colour_by_normal(const xml::Scene& xmlscene) -> png::Image {
    std::vector<Colour> framebuffer {get_initial_framebuffer(xmlscene)};
    std::vector<PixelRay> camera_rays {get_camera_rays(xmlscene.camera, framebuffer)};
    std::vector<PixelIntersection> intersections {camera_rays.size(), PixelIntersection {}};

    auto rays_and_intersecs {std::views::zip(camera_rays, intersections)};
    std::ranges::for_each(rays_and_intersecs,
        [&](auto&& item) -> void {
            auto&& [ray, intersection] {item};
            /*
            std::for_each(xmlscene.spheres.begin(), xmlscene.spheres.end(), [&](const Sphere& sphere) -> void {
                sphere.intersect(intersection, ray);

            });
            std::ranges::for_each(xmlscene.meshes, [&](const Mesh& mesh) -> void {
                mesh.intersect(intersection, ray);
            });
            */
            xmlscene.intersectables_bvh.determine_intersections(intersection, ray);
    });

    static constexpr auto intersect_to_rgb {[](const Intersection& intersection) -> std::array<u8, 3> {
        if (!intersection.has_intersected()) {
            return {0, 0, 0};
        }

        auto&& [nx, ny, nz] {intersection.normal};
        return Colour {.r = nx, .g = ny, .b = nz};
    }};

    std::vector<u8> rgbvec {
        intersections
        | std::views::transform(intersect_to_rgb)
        | std::views::join
        | utils::view_to_vec
    };

    return std::make_pair(rgbvec, xmlscene.camera.resolution_horizontal);
}

