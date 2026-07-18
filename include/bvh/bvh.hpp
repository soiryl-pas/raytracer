#ifndef BVH_HPP
#define BVH_HPP

#include <variant>
#include <vector>

#include "geometry/aabb.hpp"
#include "geometry/ray.hpp"
#include "geometry/intersection.hpp"
#include "intersect.hpp"
#include "utils.hpp"

namespace bvh {

template<typename Body>
concept Indexable = isec::Intersectable<Body>
    && requires { typename Body::BvhIndex; };

/*
 * Designates an interior node, that has an offset to the right child from itself.
 * The left child is following this node immediately.
 */
struct InteriorIndex {
    u32 offset_to_right_child {};
    u32 bounds_index {};
    Axis split_axis {};
};
/*
 * Designates a leaf node that holds multiple children, that should all be traversed.
 */
struct LeafGroupIndex {
    u32 num_children_in_group {};
    LeafGroupIndex(usize num_children_in_group)
        : num_children_in_group {utils::asserting_narrow<u32>(num_children_in_group)} {}
};

template<bvh::Indexable... Is>
using BvhNode = std::variant<InteriorIndex, LeafGroupIndex, typename Is::BvhIndex...>;

template<bvh::Indexable... Is>
struct BVH {
    std::tuple<std::vector<Is>...> intersectables {};
    std::vector<bvh::BvhNode<Is...>> bvh {};
    std::vector<AABB> bounds {};
    bool use_bvh {false};

    [[nodiscard]] static
    auto construct_simple_traversal_without_bvh(std::vector<Is>&&... intersectables) -> BVH;

    [[nodiscard]] static
    auto construct_bvh_with_sah(std::vector<Is>&&... intersectables) -> BVH;

    void determine_intersections(Intersection& out, const Ray& ray) const;
    void determine_intersections(PixelIntersection& out, const PixelRay& ray) const;
};

template<bvh::Indexable I, bvh::Indexable... Is>
inline constexpr
auto get_intersectables_vector(const bvh::BVH<Is...>& bvh) -> const std::vector<I>& {
    return std::get<std::vector<I>>(bvh.intersectables);
}

};

#endif

