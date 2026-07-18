#ifndef MESH_HPP
#define MESH_HPP

#include <algorithm>
#include <vector>

#include "geometry/aabb.hpp"
#include "geometry/intersection.hpp"
#include "geometry/ray.hpp"
#include "illum/material.hpp"
#include "math/coor.hpp"
#include "math/vec.hpp"
#include "math/matrix.hpp"
#include "types.hpp"
#include "utils.hpp"

struct Mesh {
    struct UV {
        f64 u {};
        f64 v {};
    };

    struct Face {
        u32 pos_idx {};
        u32 uv_idx {};
        u32 normal_idx {};

        [[nodiscard]] constexpr
        auto get_pos(const Mesh& parentmesh) const -> Coor {
            return parentmesh.positions[this->pos_idx];
        }

        [[nodiscard]] constexpr
        auto get_uv(const Mesh& parentmesh) const -> UV {
            return parentmesh.uvs[this->uv_idx];
        }

        [[nodiscard]] constexpr
        auto get_normal(const Mesh& parentmesh) const -> Vec {
            return parentmesh.normals[this->normal_idx];
        }
    };

    struct Triangle {
        Face v1 {};
        Face v2 {};
        Face v3 {};
        const Mesh *parentmesh {};
        AABB bounds_world {};

        [[nodiscard]] constexpr
        auto get_pos(Face face) const -> Coor {
            return face.get_pos(*this->parentmesh);
        }

        [[nodiscard]] constexpr
        auto get_uv(Face face) const -> UV {
            return face.get_uv(*this->parentmesh);
        }

        [[nodiscard]] constexpr
        auto get_normal(Face face) const -> Vec {
            return face.get_normal(*this->parentmesh);
        }

        /**
         * Updates `out` with the correct data if there was a (nearer) intersection,
         * else leaves it.
         */
        void intersect(Intersection& out, const Ray& ray) const;

        [[nodiscard]] constexpr
        auto bounds() const -> AABB {
            return this->bounds_world;
        }
    };

    std::vector<Coor> positions {};
    std::vector<Vec> normals {};
    std::vector<UV> uvs {};
    std::vector<Triangle> triangles {};
    lum::Material material {};
    Mat4 ray_transformation {};

    constexpr
    void set_material(lum::Material&& material) {
        this->material = material;
    }

    constexpr
    void set_ray_transformation(const Mat4& ray_transformation) {
        this->ray_transformation = ray_transformation;
    }

    constexpr
    void intersect(Intersection& out, const Ray& ray) const {
        std::ranges::for_each(this->triangles, [&](const Triangle& triangle) -> void {
            triangle.intersect(out, ray);
        });
    }

    struct BvhIndex {
        u32 triangle_index {};
        u16 mesh_index {};
        BvhIndex(long mesh_index, long triangle_index)
            : triangle_index {utils::asserting_narrow<u32>(triangle_index)}
            , mesh_index {utils::asserting_narrow<u16>(mesh_index)}
            {}
    };
};

#endif

