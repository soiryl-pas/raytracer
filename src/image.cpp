#include "image.hpp"

#include <spng.h>
#include <cstdio>
#include <iostream>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "logging.hpp"
#include "utils.hpp"

namespace {

static constexpr
auto check_error = [] (std::string_view name, int retval) {
    if (retval != 0) {
        std::cout << "SPNG Error for " << name << ": " << ::spng_strerror(retval);
    }
};

}

// Based on the example.c from SPNG
void png::dbg::debugimage() {
    ::spng_ctx* ctx {::spng_ctx_new(SPNG_CTX_ENCODER)};

    FILE *const outputfile {std::fopen("debugimage.png", "wb")};
    ::spng_set_png_file(ctx, outputfile);

    constexpr int width {480};
    constexpr int height {480};

    ::spng_ihdr ihdr {
        .width = width,
        .height = height,
        .bit_depth = 8,
        .color_type = SPNG_COLOR_TYPE_TRUECOLOR,
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0,
    };
    ::spng_set_ihdr(ctx, &ihdr);

    std::vector<u8> img (width * height * 3, 0x80);
    int ret = ::spng_encode_image(ctx, img.data(), img.size(),
        SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE
    );
    check_error("spng_encode_image()", ret);

    std::fclose(outputfile);
    ::spng_ctx_free(ctx);
}

void png::encode_image(
    std::filesystem::path outputpath,
    std::vector<u8>&& rgbvec,
    usize imagewidth
) {
    ::spng_ctx *ctx {::spng_ctx_new(SPNG_CTX_ENCODER)};

    std::FILE *const outputfile {std::fopen(outputpath.c_str(), "wb")};
    ::spng_set_png_file(ctx, outputfile);

    const usize rgb_bytesize {rgbvec.size()};
    const usize img_pixelnum {rgb_bytesize / 3};
    const usize imageheight {img_pixelnum / imagewidth};

    ::spng_ihdr ihdr {
        .width = static_cast<u32>(imagewidth),
        .height = static_cast<u32>(imageheight),
        .bit_depth = 8,
        .color_type = SPNG_COLOR_TYPE_TRUECOLOR,
        .compression_method = 0,
        .filter_method = 0,
        .interlace_method = 0,
    };
    ::spng_set_ihdr(ctx, &ihdr);

    // Image is assumed to be upside down
    std::vector<u8> imgvec {
        rgbvec
            | std::views::chunk(imagewidth * 3)
            | std::views::reverse
            | std::views::join
            | utils::view_to_vec
    };

    int ret {::spng_encode_image(ctx, imgvec.data(), rgb_bytesize,
        SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE
    )};
    check_error("spng_encode_image()", ret);

    LOG("Finished encoding the image.");

    std::fclose(outputfile);
    ::spng_ctx_free(ctx);
}

auto png::decode_image(const std::filesystem::path& inputpath) -> png::Image {
    ::spng_ctx *ctx {::spng_ctx_new(0)};

    std::FILE *const inputfile {std::fopen(inputpath.c_str(), "rb")};
    if (inputfile == nullptr) {
        throw std::runtime_error("File " + inputpath.string() + " could not be found.");
    }

    ::spng_set_png_file(ctx, inputfile);

    ::spng_ihdr ihdr {};
    ::spng_get_ihdr(ctx, &ihdr);

    if (ihdr.width == 0 && ihdr.height == 0) {
        throw std::runtime_error("File " + inputpath.string() + " seems not to be a valid PNG file.");
    }

    /*
    if (ihdr.color_type == SPNG_COLOR_TYPE_INDEXED) {
        WARN("The texture ", inputpath.c_str(), " is given as indexed file, and will probably not be correctly decoded.");
    }
    */

    usize image_size {};
    ::spng_decoded_image_size(ctx, SPNG_FMT_RGB8, &image_size);
    usize image_width {image_size / ihdr.height / 3};

    std::vector<u8> rawimage(image_size, u8 {});
    int ret {::spng_decode_image(
        ctx, rawimage.data(), rawimage.size(),
        SPNG_FMT_RGB8, 0
    )};
    check_error("spng_decode_image()", ret);

    std::fclose(inputfile);
    ::spng_ctx_free(ctx);

    return std::make_pair(rawimage, image_width);
}

