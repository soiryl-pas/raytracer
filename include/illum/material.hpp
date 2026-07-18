#ifndef MATERIAL_HPP
#define MATERIAL_HPP

#include <cassert>
#include <cmath>
#include <concepts>
#include <variant>
#include <vector>

#include "types.hpp"

namespace lum {

struct BaseMaterial {
    f64 ka {};
    f64 kd {};
    f64 ks {};
    f64 reflectance {};
    f64 transmittance {};
    f64 refraction {};
    u32 exponent {};
};

struct MaterialSolid : lum::BaseMaterial {
    Colour colour {};
};

struct MaterialTextured : lum::BaseMaterial {
    std::vector<u8> rawimage {};
    usize imagewidth {};

    [[nodiscard]] constexpr
    auto get_num_pixel() const -> usize {
        return this->rawimage.size() / 3;
    }

    [[nodiscard]] constexpr
    auto get_imageheight() const -> usize {
        return this->get_num_pixel() / this->imagewidth;
    }
};

using Material = std::variant<lum::MaterialSolid, lum::MaterialTextured>;

#define BASE_MATERIAL_GETTER(member, type) \
    static inline constexpr auto get_##member { \
        [] (const auto& material) -> type { \
            return material.member; \
        } \
    };

BASE_MATERIAL_GETTER(ka, f64)
BASE_MATERIAL_GETTER(kd, f64)
BASE_MATERIAL_GETTER(ks, f64)
BASE_MATERIAL_GETTER(reflectance, f64)
BASE_MATERIAL_GETTER(transmittance, f64)
BASE_MATERIAL_GETTER(refraction, f64)
BASE_MATERIAL_GETTER(exponent, u32)

static inline constexpr auto get_phong_contribution {
    [] (auto&& material) -> f64 {
        return 1 - material.reflectance - material.transmittance;
    }
};

struct MaterialGetColour {
    f64 u {};
    f64 v {};

    constexpr
    auto operator()(const lum::MaterialSolid& material) -> Colour {
        return material.colour;
    }

    auto operator()(const lum::MaterialTextured&) -> Colour;
};

template<typename T>
concept HasMaterial = requires(T t) {
    { t.set_material(lum::MaterialSolid {}) } -> std::same_as<void>;
    { t.set_material(lum::MaterialTextured {}) } -> std::same_as<void>;
};

}

#endif

