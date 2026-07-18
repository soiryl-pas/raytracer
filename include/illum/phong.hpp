#ifndef PHONG_HPP
#define PHONG_HPP

#include "geometry/intersection.hpp"
#include "parse/xmlparse.hpp"
#include "types.hpp"

namespace lum {

extern
auto get_phong_colour(
    const xml::Scene& xmlscene,
    const lum::Material& material,
    const Intersection& intersection
) -> Colour;

}

#endif

