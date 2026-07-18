#ifndef MATRIX_HPP
#define MATRIX_HPP

#include <cmath>

#include "math/coor.hpp"
#include "math/vec.hpp"
#include "types.hpp"

struct Mat3 {
    f64 m11 {1.0};
    f64 m12 {};
    f64 m13 {};

    f64 m21 {};
    f64 m22 {1.0};
    f64 m23 {};

    f64 m31 {};
    f64 m32 {};
    f64 m33 {1.0};

    static constexpr
    auto from_columns(Vec c1, Vec c2, Vec c3) -> Mat3 {
        return {
            .m11 = c1.x,
            .m12 = c2.x,
            .m13 = c3.x,

            .m21 = c1.y,
            .m22 = c2.y,
            .m23 = c3.y,

            .m31 = c1.z,
            .m32 = c2.z,
            .m33 = c3.z,
        };
    }

    static constexpr
    auto mul(const Mat3& mat, Vec vec) -> Vec {
        return {
            .x = mat.m11 * vec.x + mat.m12 * vec.y + mat.m13 * vec.z,
            .y = mat.m21 * vec.x + mat.m22 * vec.y + mat.m23 * vec.z,
            .z = mat.m31 * vec.x + mat.m32 * vec.y + mat.m33 * vec.z,
        };
    }
};

struct Mat4 {
    f64 m11 {1.0};
    f64 m12 {};
    f64 m13 {};
    f64 m14 {};

    f64 m21 {};
    f64 m22 {1.0};
    f64 m23 {};
    f64 m24 {};

    f64 m31 {};
    f64 m32 {};
    f64 m33 {1.0};
    f64 m34 {};

    f64 m41 {};
    f64 m42 {};
    f64 m43 {};
    f64 m44 {1.0};

    [[nodiscard]] constexpr
    auto rightmul(const Mat4& rmat) const -> Mat4 {
        return {
            .m11 = this->m11 * rmat.m11 + this->m12 * rmat.m21 + this->m13 * rmat.m31 + this->m14 * rmat.m41,
            .m12 = this->m11 * rmat.m12 + this->m12 * rmat.m22 + this->m13 * rmat.m32 + this->m14 * rmat.m42,
            .m13 = this->m11 * rmat.m13 + this->m12 * rmat.m23 + this->m13 * rmat.m33 + this->m14 * rmat.m43,
            .m14 = this->m11 * rmat.m14 + this->m12 * rmat.m24 + this->m13 * rmat.m34 + this->m14 * rmat.m44,

            .m21 = this->m21 * rmat.m11 + this->m22 * rmat.m21 + this->m23 * rmat.m31 + this->m24 * rmat.m41,
            .m22 = this->m21 * rmat.m12 + this->m22 * rmat.m22 + this->m23 * rmat.m32 + this->m24 * rmat.m42,
            .m23 = this->m21 * rmat.m13 + this->m22 * rmat.m23 + this->m23 * rmat.m33 + this->m24 * rmat.m43,
            .m24 = this->m21 * rmat.m14 + this->m22 * rmat.m24 + this->m23 * rmat.m34 + this->m24 * rmat.m44,

            .m31 = this->m31 * rmat.m11 + this->m32 * rmat.m21 + this->m33 * rmat.m31 + this->m34 * rmat.m41,
            .m32 = this->m31 * rmat.m12 + this->m32 * rmat.m22 + this->m33 * rmat.m32 + this->m34 * rmat.m42,
            .m33 = this->m31 * rmat.m13 + this->m32 * rmat.m23 + this->m33 * rmat.m33 + this->m34 * rmat.m43,
            .m34 = this->m31 * rmat.m14 + this->m32 * rmat.m24 + this->m33 * rmat.m34 + this->m34 * rmat.m44,

            .m41 = this->m41 * rmat.m11 + this->m42 * rmat.m21 + this->m43 * rmat.m31 + this->m44 * rmat.m41,
            .m42 = this->m41 * rmat.m12 + this->m42 * rmat.m22 + this->m43 * rmat.m32 + this->m44 * rmat.m42,
            .m43 = this->m41 * rmat.m13 + this->m42 * rmat.m23 + this->m43 * rmat.m33 + this->m44 * rmat.m43,
            .m44 = this->m41 * rmat.m14 + this->m42 * rmat.m24 + this->m43 * rmat.m34 + this->m44 * rmat.m44,
        };
    }

    [[nodiscard]] constexpr
    auto leftmul(const Mat4& lmat) const -> Mat4 {
        return lmat.rightmul(*this);
    }

    // All normal transformation functions multiply to the right,
    // so a called transformation is applied BEFORE the prior called transformations

    [[nodiscard]] constexpr
    auto translate(Vec displacement) const -> Mat4 {
        return this->rightmul({
            .m14 = displacement.x,
            .m24 = displacement.y,
            .m34 = displacement.z,
        });
    }

    [[nodiscard]] constexpr
    auto scale(Vec scalings) const -> Mat4 {
        return this->rightmul({
            .m11 = scalings.x,
            .m22 = scalings.y,
            .m33 = scalings.z,
        });
    }

    [[nodiscard]] constexpr
    auto rotx(f64 rad) const -> Mat4 {
        return this->rightmul({
            .m22 = std::cos(rad),
            .m23 = -std::sin(rad),
            .m32 = std::sin(rad),
            .m33 = std::cos(rad),
        });
    }

    [[nodiscard]] constexpr
    auto roty(f64 rad) const -> Mat4 {
        return this->rightmul({
            .m11 = std::cos(rad),
            .m13 = std::sin(rad),
            .m31 = -std::sin(rad),
            .m33 = std::cos(rad),
        });
    }

    [[nodiscard]] constexpr
    auto rotz(f64 rad) const -> Mat4 {
        return this->rightmul({
            .m11 = std::cos(rad),
            .m12 = -std::sin(rad),
            .m21 = std::sin(rad),
            .m22 = std::cos(rad),
        });
    }

    // All inverse transformation functions multiply to the left,
    // so a called inv transformation is applied AFTER the prior called transformations

    [[nodiscard]] constexpr
    auto translate_inv(Vec displacement) const -> Mat4 {
        return this->leftmul({
            .m14 = -displacement.x,
            .m24 = -displacement.y,
            .m34 = -displacement.z,
        });
    }

    [[nodiscard]] constexpr
    auto scale_inv(Vec scalings) const -> Mat4 {
        return this->leftmul({
            .m11 = 1.0 / scalings.x,
            .m22 = 1.0 / scalings.y,
            .m33 = 1.0 / scalings.z,
        });
    }

    [[nodiscard]] constexpr
    auto rotx_inv(f64 rad) const -> Mat4 {
        return this->leftmul({
            .m22 = std::cos(-rad),
            .m23 = -std::sin(-rad),
            .m32 = std::sin(-rad),
            .m33 = std::cos(-rad),
        });
    }

    [[nodiscard]] constexpr
    auto roty_inv(f64 rad) const -> Mat4 {
        return this->leftmul({
            .m11 = std::cos(-rad),
            .m13 = std::sin(-rad),
            .m31 = -std::sin(-rad),
            .m33 = std::cos(-rad),
        });
    }

    [[nodiscard]] constexpr
    auto rotz_inv(f64 rad) const -> Mat4 {
        return this->leftmul({
            .m11 = std::cos(-rad),
            .m12 = -std::sin(-rad),
            .m21 = std::sin(-rad),
            .m22 = std::cos(-rad),
        });
    }

    [[nodiscard]] constexpr
    auto mul(std::array<f64, 4> coors) const -> std::array<f64, 4> {
        auto&& [x, y, z, w] {coors};
        return {
            this->m11 * x + this->m12 * y + this->m13 * z + this->m14 * w,
            this->m21 * x + this->m22 * y + this->m23 * z + this->m24 * w,
            this->m31 * x + this->m32 * y + this->m33 * z + this->m34 * w,
            this->m41 * x + this->m42 * y + this->m43 * z + this->m44 * w,
        };
    }

    [[nodiscard]] constexpr
    auto mul(Coor coor) const -> Coor {
        auto&& [x, y, z, w] {this->mul({ coor.x, coor.y, coor.z , 1.0 })};
        return {
            .x = x,
            .y = y,
            .z = z,
        };
    }

    [[nodiscard]] constexpr
    auto mul(Vec vec) const -> Vec {
        auto&& [x, y, z, w] {this->mul({vec.x, vec.y, vec.z, 0.0})};
        return {
            .x = x,
            .y = y,
            .z = z,
        };
    }

    [[nodiscard]] constexpr
    auto transpose() const -> Mat4 {
        return {
            .m11 = this->m11,
            .m12 = this->m21,
            .m13 = this->m31,
            .m14 = this->m41,
            .m21 = this->m12,
            .m22 = this->m22,
            .m23 = this->m32,
            .m24 = this->m42,
            .m31 = this->m13,
            .m32 = this->m23,
            .m33 = this->m33,
            .m34 = this->m43,
            .m41 = this->m14,
            .m42 = this->m24,
            .m43 = this->m34,
            .m44 = this->m44,
        };
    }

    [[nodiscard]] constexpr
    auto to_mat3() const -> Mat3 {
        return {
            .m11 = this->m11,
            .m12 = this->m12,
            .m13 = this->m13,
            .m21 = this->m21,
            .m22 = this->m22,
            .m23 = this->m23,
            .m31 = this->m31,
            .m32 = this->m32,
            .m33 = this->m33,
        };
    }
};

template<typename T>
concept HasTransform = requires(T t) {
    { t.set_ray_transformation(Mat4 {}) };
};

#endif

