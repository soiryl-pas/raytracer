#ifndef UTILS_HPP
#define UTILS_HPP

#include <algorithm>
#include <iterator>
#include <ranges>
#include <variant>
#include <vector>

#include "types.hpp"

/**
 * std::ranges::to was only introduced in gcc14, so I rolled out my own
 * impl to transform views into a vector, based on
 * https://stackoverflow.com/questions/58808030/range-view-to-stdvector#comment140828464_79612294
 */

/*
struct ViewToVec {};
inline constexpr ViewToVec view_to_vec {};

template <std::ranges::range R>
inline constexpr
auto operator|(R&& r, ViewToVec) {
    using value_type = std::iter_value_t<decltype(r.begin())>;
    auto&& common_r {r | std::views::common};
    return std::vector<value_type>{std::ranges::begin(common_r), std::ranges::end(common_r)};
}
*/

namespace utils {

struct ViewToVec : std::ranges::range_adaptor_closure<ViewToVec> {
    template<std::ranges::range R>
    constexpr 
    auto operator()(R&& r) const -> std::vector<std::ranges::range_value_t<R>> {
        auto&& common_r {r | std::views::common};
        return {std::ranges::begin(common_r), std::ranges::end(common_r)};
    }

    template<std::ranges::sized_range R>
    constexpr
    auto operator()(R&& r) const -> std::vector<std::ranges::range_value_t<R>> {
        using ItemT = std::ranges::range_value_t<R>;

        std::vector<ItemT> resultvec {};
        resultvec.reserve(std::ranges::size(r));

        std::ranges::copy(r, std::back_inserter(resultvec));

        return resultvec;
    }
};

inline constexpr ViewToVec view_to_vec {};

// Overloaded comparisons against shadow acne and other floating poing inaccuracies,
// with a nice user defined literal
// https://en.cppreference.com/cpp/language/user_literal

struct PermissiveFloat {
    long double value {};
    static constexpr long double EPSILON {0.0001};

    PermissiveFloat(long double value) : value {value} {}
};

static inline constexpr
auto operator==(PermissiveFloat lop, PermissiveFloat rop) -> bool {
    long double comp {lop.value - rop.value};
    return comp < PermissiveFloat::EPSILON && comp > -PermissiveFloat::EPSILON;
}

static inline constexpr
auto operator<(PermissiveFloat lop, PermissiveFloat rop) -> bool {
    return lop.value < (rop.value - PermissiveFloat::EPSILON);
}

static inline constexpr
auto operator>(PermissiveFloat lop, PermissiveFloat rop) -> bool {
    return lop.value > (rop.value + PermissiveFloat::EPSILON);
}

static inline constexpr
auto operator<=(PermissiveFloat lop, PermissiveFloat rop) -> bool {
    return !(lop > rop);
}

static inline constexpr
auto operator>=(PermissiveFloat lop, PermissiveFloat rop) -> bool {
    return !(lop < rop);
}

template<typename To>
static inline constexpr
auto asserting_narrow(auto&& value) -> To {
    assert (value <= std::numeric_limits<To>::max());
    return static_cast<To>(value);
}

template<typename... Lambdas>
struct LambdaFunctor : Lambdas... {
    using Lambdas::operator()...;
};

}

// Some nicer syntax for variants and std::visit

template<typename Callable, typename Variant>
requires requires (Variant variant, Callable callable) {
    std::visit(callable, variant);
}
static inline constexpr
auto operator|(Variant&& variant, Callable&& callable) {
    return std::visit(std::forward<Callable>(callable), std::forward<Variant>(variant));
}


static inline constexpr
auto operator""_eps(long double value) -> utils::PermissiveFloat {
    return value;
}

#endif

