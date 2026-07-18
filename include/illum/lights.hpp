#ifndef LIGHTS_HPP
#define LIGHTS_HPP

#include <variant>
#include <vector>

#include "math/coor.hpp"
#include "math/vec.hpp"
#include "math/matrix.hpp"
#include "types.hpp"

namespace lum {

struct BaseLight {
    Colour colour {};
};

struct AmbientLight : lum::BaseLight {};

struct ParallelLight : lum::BaseLight {
    Vec direction {};
};

struct PointLight : lum::BaseLight {
    Coor position {};
};

struct SpotLight : lum::BaseLight {
    Coor position {};
    Vec direction {};
    f64 alpha_falloff_begin {};
    f64 alpha_falloff_end {};
};

struct AreaLight {
    usize mesh_idx {};

    inline static constexpr usize NUM_SAMPLES_PER_TRIANGLE {25u};
    std::vector<std::array<std::pair<Coor, Vec>, NUM_SAMPLES_PER_TRIANGLE>> sample_vertices_normals {};
};

using Light = std::variant<
    lum::AmbientLight,
    lum::ParallelLight,
    lum::PointLight,
    lum::SpotLight,
    lum::AreaLight
>;

}

#endif

