#include <iostream>
#include <stdexcept>

#include "image.hpp"
#include "logging.hpp"
#include "parse/xmlparse.hpp"
#include "traceray.hpp"

using std::operator""sv;

namespace {

static
void render_batch(const std::filesystem::path& directory, bool use_bvh) {
    if (!std::filesystem::is_directory(directory)) {
        throw std::runtime_error("The passed path " + directory.string() + " is not a directory.");
    }

    for (const auto& direntry : std::filesystem::directory_iterator(directory)) {
        const std::filesystem::path& filepath {direntry.path()};
        if (filepath.extension() != ".xml") {
            continue;
        }

        std::cout << "Tracing " << filepath << "...\n";
        try {
            xml::Scene xmlscene { use_bvh
                ? xml::parse_with_bvh(filepath)
                : xml::parse_no_bvh(filepath)
            };
            auto&& [rgbvec, rgbvec_width] {trace::trace_rays(xmlscene)};
            png::encode_image(xmlscene.outputfile, std::move(rgbvec), rgbvec_width);
            std::cout << "Done, written to " << xmlscene.outputfile << ".\n";
        } catch (std::runtime_error& error) {
            std::cout
                << "Skipping " << filepath << ", as an error occurred:\n"
                << error.what() << "\n";
        }
    }

    std::cout << "Done traversing through " << directory << ".\n";
}

}

auto main(int argc, char* argv[]) -> int {
    std::cout << "=== Raytracer by soiryl-pas ===\n\n";

    if (argc < 2) {
        std::cout
            << "Please pass in a scene XML file.\n"
            << "Paths to textures and meshes are searched relative to this file.\n"
            << "You can also specify --all <DIRECTORY>, which will render all scene files it can find in a directory you pass afterwards.\n"
            << "\nOther supported flags:\n"
            << "\t--nobvh\t\tRender without BVH\n";
        return 1;
    }

    std::span<char*> args {argv + 1,  argv + argc};

    auto nobvharg {std::ranges::find(args, "--nobvh"sv)};
    bool use_bvh {nobvharg == args.end()};

    if (auto allarg {std::ranges::find(args, "--all"sv)}; allarg != args.end()) {
        if (allarg == args.end() - 1) {
            std::cout << "Please pass a directory to traverse after --all.\n";
            return 2;
        }

        render_batch(*(allarg + 1), use_bvh);
        return 0;
    }

    auto positional_args {args | std::views::filter([](auto&& arg) -> bool {
        return arg[0] != '-' && arg[1] != '-';
    })};

    std::filesystem::path xmlfile {positional_args.front()};

    std::cout << "Tracing " << xmlfile << "...\n";

    xml::Scene xmlscene { use_bvh
        ? xml::parse_with_bvh(xmlfile)
        : xml::parse_no_bvh(xmlfile)
    };

    auto&& [rgbvec, rgbvec_width] {trace::trace_rays(xmlscene)};
    png::encode_image(xmlscene.outputfile, std::move(rgbvec), rgbvec_width);

    std::cout
        << "Scene " << xmlfile.filename()
        << " has been traced and put into " << xmlscene.outputfile.filename() << ".\n";

    return 0;
}



