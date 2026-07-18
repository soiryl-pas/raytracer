#ifndef TYPES_HPP
#define TYPES_HPP

#include <array>
#include <cassert>
#include <cstdint>

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using usize = std::size_t;
using isize = std::ptrdiff_t;
using f64 = double;

#define TODO [[maybe_unused]]

enum struct Axis : u8 { X, Y, Z };
inline std::array<Axis, 3> AXES { Axis::X, Axis::Y, Axis::Z };

struct Colour {
    f64 r {};
    f64 g {};
    f64 b {};

    constexpr
    operator std::array<u8, 3>() const {
        return {
            static_cast<u8>(this->r * 0xff),
            static_cast<u8>(this->g * 0xff),
            static_cast<u8>(this->b * 0xff),
        };
    }

    static const Colour BLACK;

    [[nodiscard]] constexpr
    auto scale(f64 scalar) const -> Colour {
        assert (scalar >= 0.0);
        assert (scalar <= 1.0);

        return {
            .r = this->r * scalar,
            .g = this->g * scalar,
            .b = this->b * scalar,
        };
    }

    static constexpr
    auto saturating_add(Colour lcolour, Colour rcolour) -> Colour {
        assert(lcolour.r >= 0.0);
        assert(lcolour.g >= 0.0);
        assert(lcolour.b >= 0.0);
        assert(rcolour.r >= 0.0);
        assert(rcolour.g >= 0.0);
        assert(rcolour.b >= 0.0);

        return {
            .r = std::min(lcolour.r + rcolour.r, 1.0),
            .g = std::min(lcolour.g + rcolour.g, 1.0),
            .b = std::min(lcolour.b + rcolour.b, 1.0),
        };
    }

    static constexpr
    auto blend(Colour lcolour, Colour rcolour) -> Colour {
        return {
            .r = lcolour.r * rcolour.r,
            .g = lcolour.g * rcolour.g,
            .b = lcolour.b * rcolour.b,
        };
    }
};

inline constexpr Colour Colour::BLACK { .r = 0, .g = 0, .b = 0};

#endif

