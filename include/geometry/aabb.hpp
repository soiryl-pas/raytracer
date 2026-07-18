#ifndef AABB_HPP
#define AABB_HPP

#include <algorithm>
#include <cassert>

#include "geometry/ray.hpp"
#include "math/coor.hpp"
#include "types.hpp"

struct AABB {
    Coor max {};
    Coor min {};

    [[nodiscard]] constexpr
    auto centroid() const -> Coor {
        return min.add(Coor::to_vec(min, max).scale(0.5));
    }

    [[nodiscard]] constexpr
    auto merge(AABB rop) const -> AABB {
        return {
            .max = {
                .x = std::max(this->max.x, rop.max.x),
                .y = std::max(this->max.y, rop.max.y),
                .z = std::max(this->max.z, rop.max.z),
            },
            .min = {
                .x = std::min(this->min.x, rop.min.x),
                .y = std::min(this->min.y, rop.min.y),
                .z = std::min(this->min.z, rop.min.z),
            },
        };
    }

    [[nodiscard]] constexpr
    auto merge(Coor rop) const -> AABB {
        return this->merge(AABB { .max = rop, .min = rop });
    }

    template<std::ranges::range R>
    [[nodiscard]] static constexpr
    auto from_centroids(R&& centroids) -> AABB {
        assert(centroids.size() > 0);

        AABB first_centroid_bound {
            .max = centroids.front(),
            .min = centroids.front(),
        };
        return std::ranges::fold_left(centroids, first_centroid_bound, [](const AABB& acc, const Coor& coor) -> AABB {
            return acc.merge(coor);
        });
    }

    [[nodiscard]] constexpr
    auto largest_axis() const -> Axis {
        Vec diagonal {Coor::to_vec(this->min, this->max)};
        if (diagonal.x > diagonal.y && diagonal.x > diagonal.z) {
            return Axis::X;
        }
        if (diagonal.y > diagonal.z) {
            return Axis::Y;
        }
        return Axis::Z;
    }

    /*
     * True if either the ray hits the aabb and goes through it (t1 and t2 positive)
     * or if the ray starts in the aabb and goes out of it (t1 negative, t2 positive)
     */
    [[nodiscard]]
    auto intersect(const Ray& ray) const -> bool;

    [[nodiscard]] constexpr
    auto offset_float(Coor coor, Axis axis) const -> f64 {
        f64 axis_min {this->min.axis_coor(axis)};
        f64 axis_max {this->max.axis_coor(axis)};
        f64 range {axis_max - axis_min};
        assert(range > 0.0);

        f64 axis_coor {coor.axis_coor(axis)};
        return (axis_coor - axis_min) / range;
    }

    [[nodiscard]] constexpr
    auto surface() const -> f64 {
        Vec diagonal {Coor::to_vec(this->min, this->max)};
        return 2 * (diagonal.x * diagonal.y + diagonal.x * diagonal.z + diagonal.y * diagonal.z);
    }
};

#endif

