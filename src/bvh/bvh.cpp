#include "bvh/bvh.hpp"

#include <algorithm>
#include <cassert>

#include "geometry/sphere.hpp"
#include "geometry/mesh.hpp"
#include "utils.hpp"


namespace {

/*
 * Based on the pbrt book: https://www.pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies#Traversal
 * But written recursively for ease of writing.
 */
template<bvh::Indexable... Is>
void determine_intersections_rec(
    Intersection& out,
    const Ray& ray,
    const bvh::BVH<Is...>& bvh,
    usize curindex
) {
    const utils::LambdaFunctor intersect_or_recurse {
        [&](bvh::InteriorIndex idx) -> void {
            const AABB& curbounds {bvh.bounds[idx.bounds_index]};
            bool does_ray_intersect {curbounds.intersect(ray)};
            if (!does_ray_intersect) { return; }

            // Recurse into the nearer child first, depending on the ray direction
            Axis split_axis {idx.split_axis};
            f64 ray_axis_coor {ray.direction.axis_coor(split_axis)};
            if (ray_axis_coor < 0.0) {
                determine_intersections_rec(out, ray, bvh, curindex + idx.offset_to_right_child);
                determine_intersections_rec(out, ray, bvh, curindex + 1);
            }
            else {
                determine_intersections_rec(out, ray, bvh, curindex + 1);
                determine_intersections_rec(out, ray, bvh, curindex + idx.offset_to_right_child);
            }
        },
        [&](bvh::LeafGroupIndex idx) -> void {
            for (usize group_idx {}; group_idx < idx.num_children_in_group; ++group_idx) {
                determine_intersections_rec(out, ray, bvh, curindex + group_idx + 1);
            }
        },
        [&](Sphere::BvhIndex idx) -> void {
            const auto& spherevec {bvh::get_intersectables_vector<Sphere>(bvh)};
            assert(idx.sphere_index < spherevec.size());

            const Sphere& sphere {spherevec[idx.sphere_index]};
            sphere.intersect(out, ray);
        },
        [&](Mesh::BvhIndex idx) -> void {
            const auto& meshvec {bvh::get_intersectables_vector<Mesh>(bvh)};
            assert(idx.mesh_index < meshvec.size());

            const Mesh& mesh {meshvec[idx.mesh_index]};
            assert(idx.triangle_index < mesh.triangles.size());

            const Mesh::Triangle& triangle {mesh.triangles[idx.triangle_index]};
            triangle.intersect(out, ray);
        },
    };

    const bvh::BvhNode<Is...>& bvhindex {bvh.bvh[curindex]};
    bvhindex | intersect_or_recurse;
}

template<isec::Intersectable I>
void determine_intersections_all_flat(
    Intersection& out,
    const Ray& ray,
    const std::vector<I>& intersectables
) {
    std::ranges::for_each(intersectables, [&](auto&& intersectable) -> void {
        intersectable.intersect(out, ray);
    });
}

}

template<bvh::Indexable... Is>
void bvh::BVH<Is...>::determine_intersections(Intersection& out, const Ray& ray) const {
    if (this->use_bvh) {
        determine_intersections_rec(out, ray, *this, 0);
    }
    else {
        std::apply([&](auto&&... vecs) -> void {
            (determine_intersections_all_flat(out, ray, vecs), ...);
        }, this->intersectables);
    }
}

template<bvh::Indexable... Is>
void bvh::BVH<Is...>::determine_intersections(PixelIntersection& out, const PixelRay& ray) const {
    this->determine_intersections(static_cast<Intersection&>(out), static_cast<const Ray&>(ray));
    out.pixel = &ray.pixel;
    out.colour_multiplier = ray.colour_multiplier;
}

template<bvh::Indexable... Is>
auto bvh::BVH<Is...>::construct_simple_traversal_without_bvh(std::vector<Is>&&... intersectables) -> BVH {
    /*
    std::vector<bvh::BvhNode<Is...>> bvhnodes {};

    const auto put_intersectables_indices_into_bvh {[&](auto&& intersectable_vec) -> void {
        for (auto&& [index, intersectable] : intersectable_vec | std::views::enumerate) {
            intersectable.template push_bvhindices_into_vec<Is...>(bvhnodes, static_cast<usize>(index));
        }
    }};

    (put_intersectables_indices_into_bvh(intersectables), ...);

    const usize num_primitives {bvhnodes.size()};
    bvhnodes.emplace(bvhnodes.begin(), bvh::LeafGroupIndex {
        .num_children_in_group = utils::asserting_narrow<u32>(num_primitives),
    });
    */

    return {
        .intersectables = std::make_tuple(std::move(intersectables)...),
        .bvh = {},
        .use_bvh = false,
    };
}

template struct bvh::BVH<Sphere, Mesh>;

