#include "parse/objparse.hpp"

#include <cassert>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "geometry/aabb.hpp"
#include "geometry/mesh.hpp"
#include "utils.hpp"

using std::operator""sv;
using std::operator""s;

namespace {

static constexpr
auto parse_face(std::string_view facetoken) -> Mesh::Face {
    usize first_slash {facetoken.find('/')};
    if (first_slash == std::string_view::npos) {
        throw std::runtime_error("f cmd was malformed, with too few slashes");
    }
    //LOG("First slash: ", first_slash);
    usize second_slash {facetoken.find('/', first_slash + 1)};
    if (second_slash == std::string_view::npos) {
        throw std::runtime_error("f cmd was malformed, with too few slashes");
    }
    //LOG("Second slash: ", second_slash);

    Mesh::Face resultface {
        .pos_idx = 1,
        .uv_idx = 1,
        .normal_idx = 1,
    };
    std::from_chars(facetoken.begin(), facetoken.begin() + first_slash, resultface.pos_idx);
    std::from_chars(facetoken.begin() + first_slash + 1, facetoken.begin() + second_slash, resultface.uv_idx);
    std::from_chars(facetoken.begin() + second_slash + 1, facetoken.end(), resultface.normal_idx);

    resultface.pos_idx--;
    resultface.uv_idx--;
    resultface.normal_idx--;

    assert (resultface.normal_idx != 0xffffffff);

    return resultface;
}

}

void obj::parse(Mesh& out, const std::filesystem::path& meshfilepath) {
    std::ifstream meshfile {meshfilepath};
    if (meshfile.fail()) {
        throw std::runtime_error("The .obj file "s + meshfilepath.native() + " could not be found.");
    }

    for (std::string line {}; std::getline(meshfile, line);) {
        const std::vector<std::string> tokens {
            std::views::split(line, " "sv)
            | std::views::transform([](auto&& token) -> std::string {return std::string {std::string_view {token}};})
            | utils::view_to_vec
        };

        if (tokens.size() <= 0) { continue; }

        if (tokens[0] == "v"sv) {
            if (tokens.size() != 4) {
                throw std::runtime_error("OBJ: v command must be followed by 3 operands");
            }

            out.positions.emplace_back(
                std::stod(tokens[1]),
                std::stod(tokens[2]),
                std::stod(tokens[3])
            );
            continue;
        }

        if (tokens[0] == "vt"sv) {
            if (tokens.size() != 3) {
                throw std::runtime_error("OBJ: vt command must be followed by 2 operands");
            }

            out.uvs.emplace_back(
                std::stod(tokens[1]),
                std::stod(tokens[2])
            );
            continue;
        }

        if (tokens[0] == "vn"sv) {
            if (tokens.size() != 4) {
                throw std::runtime_error("OBJ: vn command must be followed by 3 operands");
            }

            out.normals.emplace_back(
                std::stod(tokens[1]),
                std::stod(tokens[2]),
                std::stod(tokens[3])
            );
            continue;
        }

        if (tokens[0] == "f"sv) {
            if (tokens.size() != 4) {
                throw std::runtime_error("OBJ: f command must be followed by 3 operands");
            }

            out.triangles.emplace_back(
                parse_face(tokens[1]),
                parse_face(tokens[2]),
                parse_face(tokens[3]),
                &out,
                AABB {}
            );
            continue;
        }
    }
}
