#include "geometry/mesh.hpp"
#include "utils.hpp"

void Mesh::Triangle::intersect(Intersection& out, const Ray& ray) const {
    Ray transformed_ray {ray.apply_transformation(this->parentmesh->ray_transformation)};

    Coor pos1 {this->get_pos(this->v1)};
    Vec e1 {Coor::to_vec(pos1, this->get_pos(this->v2))};
    Vec e2 {Coor::to_vec(pos1, this->get_pos(this->v3))};

    Vec P {Vec::cross(transformed_ray.direction, e2)};
    f64 det_A {Vec::dot(P, e1)};

    //LOG("det_A: ", det_A);
    // Backfacing, if determinant is negative
    // But open_room.obj at least does not correctly orient its
    // triangles for culling
    /*
    if (det_A < EPSILON) {
        return;
    }
    */

    /*
    if (std::abs(det_A) < EPSILON) {
        return;
    }
    */
    if (det_A == 0.0_eps) {
        return;
    }

    f64 det_A_inv {1.0 / det_A};
    Vec s {Coor::to_vec(pos1, transformed_ray.origin)};

    f64 a_factor {Vec::dot(P, s)};
    f64 a {det_A_inv * a_factor};

    //LOG("a: ", a);

    if (a < 0 || a > 1) {
        return;
    }

    Vec Q {Vec::cross(s, e1)};
    f64 b_factor {Vec::dot(Q, transformed_ray.direction)};
    f64 b {det_A_inv * b_factor};

    //LOG("b: ", b);

    if (b < 0 || a + b > 1) {
        return;
    }

    f64 t_factor {Vec::dot(Q, e2)};
    f64 t {det_A_inv * t_factor};

    //LOG("t: ", t);

    if (t <= 0.0_eps) {
        return;
    }

    if (out.has_intersected() && out.t < t) {
        return;
    }

    // Interpolate normal from vertex normals
    Vec n1 {this->get_normal(this->v1)};
    Vec n2 {this->get_normal(this->v2)};
    Vec n3 {this->get_normal(this->v3)};
    Vec normal_objspace {Vec::add(
        n1.scale(1 - a - b),
        n2.scale(a),
        n3.scale(b)
    )};

    Mat3 normal_mat {this->parentmesh->ray_transformation.transpose().to_mat3()};
    Vec normal_worldspace {Mat3::mul(normal_mat, normal_objspace).normalise()};

    auto [u1, v1] {this->get_uv(this->v1)};
    auto [u2, v2] {this->get_uv(this->v2)};
    auto [u3, v3] {this->get_uv(this->v3)};

    out = Intersection {
        .point    = ray.eval_to_point(t),
        .normal   = normal_worldspace,
        .incident_ray_direction = ray.direction,
        .t        = t,
        .u        = (1 - a - b) * u1 + a * u2 + b * u3,
        .v        = (1 - a - b) * v1 + a * v2 + b * v3,
        .material = &this->parentmesh->material,
    };
}

