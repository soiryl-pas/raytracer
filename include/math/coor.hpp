#ifndef COOR_HPP
#define COOR_HPP

#include "math/vec.hpp"
#include "types.hpp"

struct Coor {
    f64 x {};
    f64 y {};
    f64 z {};

    static constexpr 
    auto to_vec(Coor from, Coor to) -> Vec {
        return Vec {
            .x = to.x - from.x,
            .y = to.y - from.y,
            .z = to.z - from.z,
        };
    }

    [[nodiscard]] constexpr
    auto add(Vec vec) const -> Coor {
        return Coor {
            .x = this->x + vec.x,
            .y = this->y + vec.y,
            .z = this->z + vec.z,
        };
    }

    [[nodiscard]] constexpr
    auto axis_coor(Axis axis) const -> f64 {
        switch (axis) {
            case Axis::X: return this->x;
            case Axis::Y: return this->y;
            case Axis::Z: return this->z;
        };
        return this->x;
    }
};

#endif

