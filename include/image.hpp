#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>
#include <vector>

#include "types.hpp"

namespace png {

using Image = std::pair<std::vector<u8>, usize>;

namespace dbg {

extern
void debugimage();

}

extern
void encode_image(
    std::filesystem::path outputpath,
    std::vector<u8>&& rgbvec,
    usize imagewidth
);

extern
auto decode_image(const std::filesystem::path&) -> png::Image;

}

#endif

