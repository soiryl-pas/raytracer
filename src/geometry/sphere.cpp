#include "geometry/sphere.hpp"

#include "geometry/intersection.hpp"
#include "geometry/ray.hpp"
#include "logging.hpp"
#include "utils.hpp"

void Sphere::intersect(Intersection& out, const Ray& ray) const {
    Ray transformed_ray {ray.apply_transformation(this->ray_transformation)};

    Vec center_to_origin {Coor::to_vec(this->position, transformed_ray.origin)};
    f64 d_center_to_origin {Vec::dot(transformed_ray.direction, center_to_origin)};
    f64 center_to_origin2 {center_to_origin.squared()};
    f64 d2 {transformed_ray.direction.squared()};
    f64 r2 {this->radius * this->radius};

    f64 discriminant {d_center_to_origin * d_center_to_origin - d2 * (center_to_origin2 - r2)};
    if (discriminant < 0) {
        return;
    }

    f64 t1 {(std::sqrt(discriminant) - d_center_to_origin) / d2};
    f64 t2 {(- std::sqrt(discriminant) - d_center_to_origin) / d2};

    // Use the nearer t as intersection,
    // except if it is behind the the origin,
    // and make no intersection if both are behind the origin.
    f64 t {std::min(t1, t2)};
    if (t <= 0.0_eps) { t = std::max(t1, t2); }
    if (t <= 0.0_eps) { 
        return;
    }

    if (out.has_intersected() && out.t < t) {
        return;
    }

    // uv is just set to 0, 0

    Coor intersection_point_objspace {transformed_ray.eval_to_point(t)};
    Vec normal_objspace {Coor::to_vec(this->position, intersection_point_objspace)};

    Coor intersection_point {ray.eval_to_point(t)};
    Mat3 normalmat {this->ray_transformation.transpose().to_mat3()};
    Vec normal {Mat3::mul(normalmat, normal_objspace).normalise()};

    out = Intersection {
        .point    = intersection_point,
        .normal   = normal,
        .incident_ray_direction = ray.direction,
        .t        = t,
        .u        = 0.0,
        .v        = 0.0,
        .material = &this->material,
    };
}

