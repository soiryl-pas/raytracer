#ifndef SPHERE_HPP
#define SPHERE_HPP

#include <cassert>

#include "geometry/aabb.hpp"
#include "geometry/intersection.hpp"
#include "geometry/ray.hpp"
#include "illum/material.hpp"
#include "math/coor.hpp"
#include "math/matrix.hpp"
#include "types.hpp"
#include "utils.hpp"

struct Sphere {
    Coor position {};
    f64 radius {};
    AABB bounds_world {};
    lum::Material material {};
    Mat4 ray_transformation {};

    constexpr
    void set_material(lum::Material&& material) {
        this->material = material;
    }

    constexpr
    void set_ray_transformation(const Mat4& ray_transformation) {
        this->ray_transformation = ray_transformation;
    }

    /**
     * Updates `out` with the correct data if there was a (nearer) intersection,
     * else leaves it.
     */
    void intersect(Intersection& out, const Ray& ray) const;

    [[nodiscard]] constexpr
    auto bounds() const -> AABB {
        return this->bounds_world;
    }

    struct BvhIndex {
        u32 sphere_index {};
        BvhIndex(long sphere_index) : sphere_index {utils::asserting_narrow<u32>(sphere_index)} {}
    };
};

#endif

