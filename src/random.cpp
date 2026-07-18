#include "random.hpp"

#include <random>

namespace {

static std::random_device randdev {};
static std::default_random_engine randeng {randdev()};
static std::uniform_real_distribution<f64> uniform_dist {0.0, 1.0};

}

auto utils::get_uniform_float_0_to_1() -> f64 {
    return uniform_dist(randeng);
}
