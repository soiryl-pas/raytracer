#ifndef OBJPARSE_HPP
#define OBJPARSE_HPP

#include <filesystem>

#include "geometry/mesh.hpp"

namespace obj {

extern
void parse(Mesh& out, const std::filesystem::path&);

}

#endif

