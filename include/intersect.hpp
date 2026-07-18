#ifndef INTERSECT_HPP
#define INTERSECT_HPP

#include <algorithm>
#include <concepts>
#include <vector>

#include "geometry/intersection.hpp"
#include "geometry/ray.hpp"

namespace isec {

/**
 * The intersect method is expected to update `intersection` if an intersection
 * with the ray happened, but only if either the intersection is set
 * to no_intersection before, or t is smaller.
 * If no intersection happens, `intersection` should be left alone.
 */
template<typename T>
concept Intersectable = requires(T body, Intersection& intersection, const Ray& ray) {
    { body.intersect(intersection, ray) } -> std::same_as<void>;
};

// TODO lab2b: Shouldn't be here, but rather called by a BVH consuming function

/**
 * If intersections are found, `intersections` is updated, otherwise the
 * prior state is left alone
 */
template<Intersectable I>
extern
void determine_intersections(
    const std::vector<I>& intersectables,
    const PixelRay& ray,
    PixelIntersection& intersection
) {
    const auto intersect_and_transfer_pixel {[&] (auto&& body) -> void {
        body.intersect(intersection, ray);
        intersection.pixel = &ray.pixel;
        intersection.colour_multiplier = ray.colour_multiplier;
    }};
    std::ranges::for_each(intersectables, intersect_and_transfer_pixel);
}

template<Intersectable I>
static inline constexpr
void determine_intersections(
    const std::vector<I>& intersectables,
    const Ray& ray,
    Intersection& intersection
) {
    for (auto&& body : intersectables) {
        body.intersect(intersection, ray);
    }
}

}

#endif

