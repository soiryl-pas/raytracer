#include "illum/material.hpp"
#include "logging.hpp"
#include "types.hpp"

auto lum::MaterialGetColour::operator()(const lum::MaterialTextured& material) -> Colour {
    f64 _ {};

    f64 u_frac {std::modf(this->u, &_)};
    if (u_frac < 0.0) { u_frac += 1.0; }
    f64 v_frac {std::modf(this->v, &_)};
    if (v_frac < 0.0) { v_frac += 1.0; }

    assert(u_frac <= 1.0);
    assert(u_frac >= 0.0);
    assert(v_frac <= 1.0);
    assert(v_frac >= 0.0);

    usize column {static_cast<usize>(std::trunc(
        u_frac * static_cast<f64>(material.imagewidth)
    ))};
    usize row {static_cast<usize>(std::trunc(
        v_frac * static_cast<f64>(material.get_imageheight())
    ))};

    usize baseidx {(column + row * material.imagewidth) * 3};
    assert (baseidx + 2 < material.rawimage.size());

    Colour result {
        .r = material.rawimage[baseidx + 0] / 255.0,
        .g = material.rawimage[baseidx + 1] / 255.0,
        .b = material.rawimage[baseidx + 2] / 255.0,
    };

    /*
    LOG("uv: (", this->u, ", ", this->v, ")");
    LOG("column: ", column, ", row: ", row);
    LOG(result);
    */

    return result;
}

