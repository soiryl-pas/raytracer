#ifndef INTERSECTION_HPP
#define INTERSECTION_HPP

#include "illum/material.hpp"
#include "math/coor.hpp"
#include "math/vec.hpp"
#include "types.hpp"

struct Intersection {
    Coor point {};
    Vec normal {};
    Vec incident_ray_direction {};
    f64 t {};
    f64 u {};
    f64 v {};

    // Must be nullptr such that it is default intialised to a no-intersection state
    const lum::Material *material {nullptr};

    constexpr
    auto set_no_intersection() -> Intersection& {
        this->material = nullptr;
        return *this;
    }

    [[nodiscard]] constexpr
    auto has_intersected() const -> bool {
        return this->material != nullptr;
    }
};

struct PixelIntersection : public Intersection {
    Colour *pixel {};
    f64 colour_multiplier {};
};

#endif

