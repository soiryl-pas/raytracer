#ifndef RAY_HPP
#define RAY_HPP

#include "math/coor.hpp"
#include "math/matrix.hpp"
#include "math/vec.hpp"
#include "types.hpp"

struct Ray {
    Coor origin {};
    Vec direction {};

    [[nodiscard]] constexpr
    auto eval_to_point(f64 t) const -> Coor {
        return this->origin.add(this->direction.scale(t));
    }

    [[nodiscard]] constexpr
    auto apply_transformation(const Mat4& ray_transformation) const -> Ray {
        return Ray {
            .origin = ray_transformation.mul(this->origin),
            /*
            .origin = Coor { 
                .x = this->origin.x + ray_transformation.m14,
                .y = this->origin.y + ray_transformation.m24,
                .z = this->origin.z + ray_transformation.m34,
            },
            */
            .direction = ray_transformation.mul(this->direction),
        };
    }
};

struct PixelRay : public Ray {
    Colour& pixel;
    f64 colour_multiplier {1.0};
};

#endif

