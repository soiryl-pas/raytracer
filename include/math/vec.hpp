#ifndef VEC_HPP
#define VEC_HPP

#include <cmath>

#include "types.hpp"

struct Vec {
    f64 x {};
    f64 y {};
    f64 z {};

    [[nodiscard]] constexpr
    auto magnitude() const -> f64 {
        return std::hypot(std::hypot(this->x, this->y), this->z);
    }

    [[nodiscard]] constexpr
    auto normalise() const -> Vec {
        const f64 magnitude {this->magnitude()};
        return Vec {
            .x = this->x / magnitude,
            .y = this->y / magnitude,
            .z = this->z / magnitude,
        };
    }

    [[nodiscard]] constexpr
    auto squared() const -> f64 {
        return this->x * this->x + this->y * this->y + this->z * this->z;
    }

    [[nodiscard]] constexpr
    auto scale(f64 scalar) const -> Vec {
        return Vec {
            .x = this->x * scalar,
            .y = this->y * scalar,
            .z = this->z * scalar,
        };
    }

    template<typename... Vecs>
    requires (std::same_as<Vecs, Vec> && ...)
    [[nodiscard]] static constexpr
    auto add(Vecs... vecs) -> Vec {
        return {
            .x = (vecs.x + ...),
            .y = (vecs.y + ...),
            .z = (vecs.z + ...),
        };
    }

    [[nodiscard]] static constexpr
    auto subtract(Vec lvec, Vec rvec) -> Vec {
        return {
            .x = lvec.x - rvec.x,
            .y = lvec.y - rvec.y,
            .z = lvec.z - rvec.z,
        };
    }

    [[nodiscard]] static constexpr
    auto dot(Vec lvec, Vec rvec) -> f64 {
        return lvec.x * rvec.x + lvec.y * rvec.y + lvec.z * rvec.z;
    }

    [[nodiscard]] static constexpr
    auto cross(Vec lvec, Vec rvec) -> Vec {
        return {
            .x = lvec.y * rvec.z - lvec.z * rvec.y,
            .y = lvec.z * rvec.x - lvec.x * rvec.z,
            .z = lvec.x * rvec.y - lvec.y * rvec.x,
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

