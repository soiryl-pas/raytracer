#ifndef TRACERAY_HPP
#define TRACERAY_HPP

#include "image.hpp"
#include "parse/xmlparse.hpp"

namespace trace {

namespace dbg {

extern
auto colour_by_ray_dir() -> png::Image;

extern
auto colour_by_normal(const xml::Scene&) -> png::Image;

}

extern
auto trace_rays(const xml::Scene&) -> png::Image;

}

#endif

