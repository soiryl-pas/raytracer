#ifndef XMLPARSE_HPP
#define XMLPARSE_HPP

#include <tinyxml2.h>
#include <filesystem>
#include <vector>

#include "bvh/bvh.hpp"
#include "geometry/mesh.hpp"
#include "geometry/sphere.hpp"
#include "illum/lights.hpp"
#include "math/coor.hpp"
#include "math/vec.hpp"
#include "math/matrix.hpp"
#include "types.hpp"

namespace xml {

struct Camera {
    Coor position {};
    Coor lookat {};
    Vec  up {};
    Mat3 ray_rotation {};
    u32  resolution_horizontal {};
    u32  resolution_vertical {};
    f64  tan_fovh {};
    f64  tan_fovv {};
    f64  focal_distance {};
    f64  weakness {};
    u32  max_bounces {};
    u32  render_times {1u};

    /**
     * Computes and saves tan_fovh and tan_fovv.
     * It is assumed that the resolutions have already been set,
     * otherwise nothing is done if they're 0.
     */
    void set_fovs_from_angle_deg(f64 fovh_deg);

    [[nodiscard]] constexpr
    auto num_pixel() const -> usize {
        return static_cast<usize>(this->resolution_horizontal) * this->resolution_vertical;
    }
};

struct Scene {
    xml::Camera camera {};

    Colour background_colour {};
    std::filesystem::path outputfile {};

    std::vector<lum::Light> lights {};

    bvh::BVH<Sphere, Mesh> intersectables_bvh {};
};

extern
auto parse_no_bvh(const std::filesystem::path&) -> xml::Scene;

extern
auto parse_with_bvh(const std::filesystem::path&) -> xml::Scene;

}

#endif

