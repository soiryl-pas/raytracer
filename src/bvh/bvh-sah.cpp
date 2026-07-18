#include "bvh/bvh.hpp"

#include <algorithm>
#include <cassert>
#include <ranges>
#include <stdexcept>

#include "geometry/aabb.hpp"
#include "geometry/sphere.hpp"
#include "geometry/mesh.hpp"
#include "logging.hpp"
#include "math/coor.hpp"

namespace {

template<bvh::Indexable... Is>
struct Primitive {
    AABB bounds {};
    bvh::BvhNode<Is...> bvhindex {};
};

template<bvh::Indexable I, bvh::Indexable... Is>
static
void push_primitives(std::vector<Primitive<Is...>>& out, const std::vector<I>& intersectables) {
    for (auto&& [index, intersectable] : intersectables | std::views::enumerate) {
        out.emplace_back(
            intersectable.bounds(),
            typename I::BvhIndex { index }
        );
    }
}

template<bvh::Indexable... Is>
static
void push_primitives(std::vector<Primitive<Is...>>& out, const std::vector<Mesh>& meshes) {
    for (auto&& [meshindex, mesh] : meshes | std::views::enumerate) {
        for (auto&& [triangleindex, triangle] : mesh.triangles | std::views::enumerate) {
            out.emplace_back(
                triangle.bounds(),
                Mesh::BvhIndex { meshindex, triangleindex }
            );
        }
    }
}

template<bvh::Indexable... Is>
struct BuildNode {
    Primitive<Is...> primitive {};
    BuildNode *left_child {};
    BuildNode *right_child {};
};

template<bvh::Indexable... Is>
struct BuildNodeArena {
private:
    static constexpr usize DEFAULT_NODE_CAPACITY {0x10000};
    std::vector<BuildNode<Is...>> nodes {};

public:
    BuildNodeArena() {
        this->nodes.reserve(DEFAULT_NODE_CAPACITY);
    }

    constexpr
    auto push_node(
        Primitive<Is...> primitive,
        BuildNode<Is...> *left_child,
        BuildNode<Is...> *right_child
    ) -> BuildNode<Is...>& {
        if (this->nodes.size() >= this->nodes.capacity()) {
            throw std::runtime_error("The arena for building nodes is not big enough for this scene. Too many primitives are present.");
        }

        return this->nodes.emplace_back(primitive, left_child, right_child);
    }

    constexpr
    auto push_primitive(Primitive<Is...> primitive) -> BuildNode<Is...>& {
        return this->push_node(primitive, nullptr, nullptr);
    }

    [[nodiscard]] constexpr
    auto empty() const -> bool {
        return this->nodes.empty();
    }

    [[nodiscard]] constexpr
    auto root() const -> const BuildNode<Is...>& {
        assert(!this->empty());
        return this->nodes.back();
    }
};

template<bvh::Indexable... Is>
static
auto build_leaf_primitive(
    BuildNodeArena<Is...>& build_node_arena,
    const Primitive<Is...>& primitive_to_push
) -> BuildNode<Is...>& {
    auto& primitive_node {build_node_arena.push_primitive(primitive_to_push)};
    return primitive_node;
}

/*
 * Returns a leaf group node, where all primitives of the group
 * are pushed afterwards.
 */
template<bvh::Indexable... Is>
static
auto build_leaf_group(
    BuildNodeArena<Is...>& build_node_arena,
    const std::span<Primitive<Is...>> primitives_to_push,
    AABB primitives_bounds_whole
) -> BuildNode<Is...>& {
    auto& leafnode {build_node_arena.push_primitive({
        .bounds = primitives_bounds_whole,
        .bvhindex = bvh::LeafGroupIndex { primitives_to_push.size() },
    })};

    for (auto&& primitive : primitives_to_push) {
        build_node_arena.push_primitive(primitive);
    }

    return leafnode;
}


/* Based on https://www.pbr-book.org/3ed-2018/Primitives_and_Intersection_Acceleration/Bounding_Volume_Hierarchies#TheSurfaceAreaHeuristic
*/
template<bvh::Indexable... Is>
static constexpr
auto sah_split_and_shuffle_primitives(
    std::span<Primitive<Is...>> curprimitives,
    AABB bound_centroids,
    AABB bound_whole,
    Axis split_axis
) -> usize {

    if (curprimitives.size() <= 4) {
        usize mid_index {curprimitives.size() / 2};

        std::nth_element(
            curprimitives.begin(),
            curprimitives.begin() + static_cast<isize>(mid_index),
            curprimitives.end(),
            [split_axis](auto&& lop, auto&& rop) -> bool {
                return lop.bounds.centroid().axis_coor(split_axis) < rop.bounds.centroid().axis_coor(split_axis);
        });

        return mid_index;
    }

    constexpr usize NUM_BUCKETS {12};
    struct Bucket {
        u32 count {};
        AABB bounds {};
    };
    std::array<Bucket, NUM_BUCKETS> buckets {{}};

    for (const auto& primitive : curprimitives) {
        Coor primitive_centroid {primitive.bounds.centroid()};
        f64 primitive_offset {bound_centroids.offset_float(primitive_centroid, split_axis)};
        usize bucket_idx {std::min(NUM_BUCKETS - 1, static_cast<usize>(primitive_offset * NUM_BUCKETS))};

        Bucket& curbucket {buckets[bucket_idx]};
        curbucket.bounds = (curbucket.count == 0)
            ? AABB { .max = primitive_centroid, .min = primitive_centroid }
            : curbucket.bounds.merge(primitive_centroid);
        ++curbucket.count;
    }

    std::array<f64, NUM_BUCKETS - 1> partition_cost {{}};
    for (usize partition_idx {}; partition_idx < NUM_BUCKETS - 1; ++partition_idx) {
        AABB bound_left {buckets.front().bounds};
        AABB bound_right {buckets.back().bounds};
        usize count_left {buckets.front().count};
        usize count_right {buckets.back().count};

        for (usize left_idx {1}; left_idx <= partition_idx; ++left_idx) {
            bound_left = bound_left.merge(buckets[left_idx].bounds);
            count_left += buckets[left_idx].count;
        }
        for (usize right_idx {partition_idx+1}; right_idx < NUM_BUCKETS - 1; ++right_idx) {
            bound_right = bound_right.merge(buckets[right_idx].bounds);
            count_right += buckets[right_idx].count;
        }

        partition_cost[partition_idx] =
            (static_cast<f64>(count_left) * bound_left.surface() + static_cast<f64>(count_right) * bound_right.surface()) / bound_whole.surface();
    }

    auto&& [min_cost_idx, min_cost] {std::ranges::min(
        partition_cost | std::views::enumerate, {},
        [](auto&& item) -> f64 { auto&& [idx, cost] {item}; return cost; }
    )};

    const auto is_primitive_left_of_partition {[=](const auto& primitive) -> bool {
        Coor primitive_centroid {primitive.bounds.centroid()};
        f64 primitive_offset {bound_centroids.offset_float(primitive_centroid, split_axis)};
        usize bucket_idx {std::min(NUM_BUCKETS - 1, static_cast<usize>(primitive_offset * NUM_BUCKETS))};
        return bucket_idx <= static_cast<usize>(min_cost_idx);
    }};

    auto mid_primitive {std::partition(curprimitives.begin(), curprimitives.end(), is_primitive_left_of_partition)};
    assert(mid_primitive != curprimitives.begin());
    assert(mid_primitive != curprimitives.end());
    return static_cast<usize>(mid_primitive - curprimitives.begin());
}

template<bvh::Indexable... Is>
static
auto construct_bvh_rec(
    BuildNodeArena<Is...>& build_node_arena,
    std::span<Primitive<Is...>> curprimitives
) -> BuildNode<Is...>& {
    assert (!curprimitives.empty());

    // Only one primitive in this node, push it
    if (curprimitives.size() == 1u) {
        return build_leaf_primitive(build_node_arena, curprimitives.front());
    }

    AABB first_primitive_bound {curprimitives.front().bounds};
    AABB bound_whole {std::ranges::fold_left(curprimitives, first_primitive_bound, [](const AABB& acc, const Primitive<Is...>& primitive) -> AABB {
        return acc.merge(primitive.bounds);
    })};

    AABB bound_centroids {AABB::from_centroids(curprimitives | std::views::transform(
        [](auto&& primitive) -> Coor { return primitive.bounds.centroid(); }
    ))};
    Axis split_axis {bound_centroids.largest_axis()};

    // Multiple centroids merge into the same place on the axis, push them into one group
    if (bound_centroids.min.axis_coor(split_axis) == bound_centroids.max.axis_coor(split_axis)) {
        return build_leaf_group(build_node_arena, curprimitives, bound_whole);
    }

    // No leafs are now possible anymore, construct interior node

    usize mid_index {sah_split_and_shuffle_primitives(curprimitives, bound_centroids, bound_whole, split_axis)};
    BuildNode<Is...>& leftnode {construct_bvh_rec(
        build_node_arena, curprimitives.first(mid_index)
    )};
    BuildNode<Is...>& rightnode {construct_bvh_rec(
        build_node_arena, curprimitives.last(curprimitives.size() - mid_index)
    )};

    BuildNode<Is...>& curnode {build_node_arena.push_node(
        Primitive<Is...> {
            .bounds = bound_whole, 
            .bvhindex = bvh::InteriorIndex { .offset_to_right_child = 0, .bounds_index = 0, .split_axis = split_axis },
        },
        &leftnode, &rightnode
    )};
    return curnode;
}

template<bvh::Indexable... Is>
static constexpr
auto flatten_node_arena(
    BuildNodeArena<Is...>&& build_node_arena
) -> std::pair<std::vector<bvh::BvhNode<Is...>>, std::vector<AABB>> {
    std::vector<bvh::BvhNode<Is...>> result_flat_bvh {};
    std::vector<AABB> result_bounds {};

    struct PendingRightChild {
        usize parent_idx {};
        BuildNode<Is...> *node {};
    };
    std::vector<PendingRightChild> pending_right_children {};

    const auto push_primitive_into_bvh {[&](auto&& primitive_idx) -> void {
        assert(typeid(primitive_idx) != typeid(bvh::InteriorIndex));
        assert(typeid(primitive_idx) != typeid(bvh::LeafGroupIndex));

        result_flat_bvh.emplace_back(primitive_idx);
    }};

    const auto handle_build_node_and_get_next {[&](const BuildNode<Is...>& build_node)
        -> const BuildNode<Is...>* {
        Primitive<Is...> primitive {build_node.primitive};

        /*
        const utils::LambdaFunctor handle_index {
            [&](typename Is::BvhIndex&& idx) -> std::optional<BuildNode<Is...>> {
                result_flat_bvh.emplace_back(idx);
            }...,
            [&](bvh::InteriorIndex&& idx) -> std::optional<BuildNode<Is...>> {
            }
        };
        */

        // Interior node
        if (std::holds_alternative<bvh::InteriorIndex>(primitive.bvhindex)) {
            assert(build_node.right_child != nullptr);
            assert(build_node.left_child != nullptr);

            result_bounds.emplace_back(primitive.bounds);
            result_flat_bvh.emplace_back(bvh::InteriorIndex {
                .offset_to_right_child = 0,
                .bounds_index = utils::asserting_narrow<u32>(result_bounds.size() - 1),
                .split_axis = std::get<bvh::InteriorIndex>(primitive.bvhindex).split_axis,
            });

            pending_right_children.emplace_back(result_flat_bvh.size() - 1, build_node.right_child);

            return build_node.left_child;
        }

        // Leaf node
        if (std::holds_alternative<bvh::LeafGroupIndex>(primitive.bvhindex)) {
            bvh::LeafGroupIndex leaf_idx {std::get<bvh::LeafGroupIndex>(primitive.bvhindex)};
            result_flat_bvh.emplace_back(leaf_idx);

            for (usize child_idx {0}; child_idx < leaf_idx.num_children_in_group; ++child_idx) {
                const BuildNode<Is...>& child {(&build_node)[child_idx + 1]};
                result_flat_bvh.emplace_back(child.primitive.bvhindex);
            }
        }
        else {
            // Other node
            primitive.bvhindex | push_primitive_into_bvh;
        }

        // Return right child
        if (pending_right_children.empty()) {
            return nullptr;
        }

        PendingRightChild right_child {pending_right_children.back()};
        pending_right_children.pop_back();

        bvh::BvhNode<Is...>& parent {result_flat_bvh[right_child.parent_idx]};
        bvh::InteriorIndex& parent_interior_idx {std::get<bvh::InteriorIndex>(parent)};
        parent_interior_idx.offset_to_right_child = utils::asserting_narrow<u32>(result_flat_bvh.size() - right_child.parent_idx);

        return right_child.node;
    }};

    // Start with root
    const BuildNode<Is...> *iterator {&build_node_arena.root()};
    while (iterator != nullptr) {
        iterator = handle_build_node_and_get_next(*iterator);
    }

    return std::make_pair(result_flat_bvh, result_bounds);
}

}

template<bvh::Indexable... Is>
auto bvh::BVH<Is...>::construct_bvh_with_sah(std::vector<Is>&&... intersectables) -> BVH {
    LOG("Constructing the BVH.");

    std::vector<Primitive<Is...>> primitives {};
    (push_primitives(primitives, intersectables), ...);


    BuildNodeArena<Is...> build_node_arena {};
    construct_bvh_rec(build_node_arena, std::span {primitives});

    LOG("Flattening the BVH.");

    auto&& [bvh, bounds] {flatten_node_arena(std::move(build_node_arena))};
    return {
        .intersectables = std::make_tuple(std::move(intersectables)...),
        .bvh = std::move(bvh),
        .bounds = std::move(bounds),
        .use_bvh = true,
    };
}

template struct bvh::BVH<Sphere, Mesh>;

