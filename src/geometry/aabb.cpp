#include "geometry/aabb.hpp"

#include <algorithm>
#include <ranges>

#include "logging.hpp"
#include "types.hpp"
#include "utils.hpp"

/*
 * Idea from the pbrt book: https://www.pbr-book.org/3ed-2018/Shapes/Basic_Shape_Interface#RayndashBoundsIntersections
 * Take maximum of all t1s of the axes and the minimum of all t2s of the axes,
 * if t1 is smaller than t2, the interval is valid and inside the bounds,
 * otherwise it's outside.
 */
auto AABB::intersect(const Ray& ray) const -> bool {
    const auto ts_from_axis {[&](Axis axis) -> std::pair<f64, f64> {
        f64 t1 {(this->min.axis_coor(axis) - ray.origin.axis_coor(axis)) / ray.direction.axis_coor(axis)};
        f64 t2 {(this->max.axis_coor(axis) - ray.origin.axis_coor(axis)) / ray.direction.axis_coor(axis)};
        if (t1 > t2) { std::swap(t1, t2); }

        return std::make_pair(t1, t2);
    }};

    auto tpairs {AXES | std::views::transform(ts_from_axis)};
    f64 global_t1 {std::max(0.0, std::ranges::max(tpairs | std::views::elements<0>))};
    f64 global_t2 {std::ranges::min(tpairs | std::views::elements<1>)};

    /*
    if (global_t1 >= global_t2) {
        LOG("global_t1: ", global_t1);
        LOG("global_t2: ", global_t2);
    }
    */
    return (global_t2 - global_t1) >= 0.0_eps;
}

