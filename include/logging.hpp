#ifndef LOGGING_H
#define LOGGING_H

#ifdef NDEBUG

template<typename... Ts>
static inline constexpr
void LOG([[maybe_unused]] Ts... messages) {}

template<typename... Ts>
static inline constexpr
void WARN([[maybe_unused]] Ts... messages) {}

#else

#include <iostream>

template<typename... Ts>
static inline constexpr
void LOG(Ts... messages) {
    std::clog << std::fixed << "[LOG]\t";
    ((std::clog << messages), ...);
    std::clog << "\n";
}

template<typename... Ts>
static inline constexpr
void WARN(Ts... messages) {
    std::clog << "\033[0;31m[WARN]\t";
    ((std::clog << messages), ...);
    std::clog << "\033[0m\n";
}

#include "types.hpp"

inline constexpr
auto operator<<(std::ostream& os, Colour colour) -> std::ostream& {
    auto&& [r, g, b] {colour};
    return os << "(" << r << ", " << g << ", " << b << ")";
}

#include "math/coor.hpp"

inline constexpr
auto operator<<(std::ostream& os, Coor coor) -> std::ostream& {
    auto&& [x, y, z] {coor};
    return os << "(" << x << ", " << y << ", " << z << ")";
}

#include "math/vec.hpp"

inline constexpr
auto operator<<(std::ostream& os, Vec vec) -> std::ostream& {
    auto&& [x, y, z] {vec};
    return os << "(" << x << ", " << y << ", " << z << ")";
}

#include "geometry/intersection.hpp"

inline constexpr
auto operator<<(std::ostream& os, Intersection intersect) -> std::ostream& {
    return os << "[Point " << intersect.point << ", Normal "
        << intersect.normal << "]";
}

#endif

#endif

