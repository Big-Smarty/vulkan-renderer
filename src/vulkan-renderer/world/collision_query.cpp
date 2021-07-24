#include "inexor/vulkan-renderer/world/collision_query.hpp"

#include <glm/gtx/intersect.hpp>
#include <inexor/vulkan-renderer/world/cube.hpp>

#include <algorithm>
#include <array>
#include <utility>

namespace inexor::vulkan_renderer::world {

bool ray_box_collision(const std::array<glm::vec3, 2> &box_bounds, const glm::vec3 &position,
                       const glm::vec3 &direction) {
    glm::vec3 inverse_dir{1 / direction.x, 1 / direction.y, 1 / direction.z};
    std::array<std::int32_t, 3> sign{static_cast<std::int32_t>(inverse_dir.x < 0),
                                     static_cast<std::int32_t>(inverse_dir.y < 0),
                                     static_cast<std::int32_t>(inverse_dir.z < 0)};

    float tmin{(box_bounds[sign[0]].x - position.x) * inverse_dir.x};
    float tmax{(box_bounds[1 - sign[0]].x - position.x) * inverse_dir.x};
    float tymin{(box_bounds[sign[1]].y - position.y) * inverse_dir.y};
    float tymax{(box_bounds[1 - sign[1]].y - position.y) * inverse_dir.y};

    if ((tmin > tymax) || (tymin > tmax)) {
        return false;
    }
    if (tymin > tmin) {
        tmin = tymin;
    }
    if (tymax < tmax) {
        tmax = tymax;
    }

    float tzmin{(box_bounds[sign[2]].z - position.z) * inverse_dir.z};
    float tzmax{(box_bounds[1 - sign[2]].z - position.z) * inverse_dir.z};

    return !((tmin > tzmax) || (tzmin > tmax));
}

std::optional<glm::vec3> ray_cube_vertex_intersection(const std::shared_ptr<Cube> cube, // NOLINT
                                                      const glm::vec3 pos, const glm::vec3 dir) {
    const auto cube_polygons = cube->polygons();

    // If the cube does not contain any vertex data, no collision with vertex data can take place inside of it.
    if (cube_polygons.empty()) {
        return std::nullopt;
    }

    const auto max_float = std::numeric_limits<float>::max();

    // Calculate the intersection points and select the one closest to the camera by squared distance.
    glm::vec3 vertex_intersection{max_float, max_float, max_float};

    float m_nearest_square_distance = std::numeric_limits<float>::max();

    for (const auto &polygon : cube_polygons) {
        for (const auto &triangle : *polygon) {

            glm::vec3 collision_output;

            const auto collision_found =
                glm::intersectLineTriangle(pos, dir, triangle[0], triangle[1], triangle[2], collision_output);

            if (collision_found) {
                const auto squared_distance = glm::distance2(collision_output, pos);

                // Always store the collision which is closest to the camera.
                if (squared_distance < m_nearest_square_distance) {
                    vertex_intersection = collision_output;
                    m_nearest_square_distance = squared_distance;
                }
            }
        }
    }

    return vertex_intersection;
}

[[nodiscard]] bool is_bounding_box_and_bounding_sphere_hit(const std::shared_ptr<Cube> cube, // NOLINT
                                                           const glm::vec3 pos, const glm::vec3 dir) {
    // We need to pass this into glm::intersectRaySphere by reference although we are not interested in it.
    auto intersection_distance{0.0f};
    const auto bounding_sphere_radius = static_cast<float>(glm::sqrt(3) * cube->size()) / 2.0f;
    const auto bounding_sphere_radius_squared = static_cast<float>(std::pow(bounding_sphere_radius, 2));

    // First, check if the ray collides with the cube's bounding sphere.
    // Ray-sphere collision is much easier to calculate than ray-box collision.
    if (!glm::intersectRaySphere(pos, dir, cube->center(), bounding_sphere_radius_squared, intersection_distance)) {
        return false;
    }

    // Second, check if ray collides with bounding box.
    // TODO: This is an axis aligned bounding box! Alignment must account for rotation of the octree in the future!
    if (!ray_box_collision(cube->bounding_box(), pos, dir)) {
        return false;
    }

    return true;
}

std::optional<RayCubeCollision<Cube>> ray_cube_collision_check(const std::shared_ptr<Cube> cube, // NOLINT
                                                               const glm::vec3 pos, const glm::vec3 dir,
                                                               const std::optional<std::uint32_t> grid_level_counter) {

    if (cube->type() == Cube::Type::EMPTY || !is_bounding_box_and_bounding_sphere_hit(cube, pos, dir)) {
        return std::nullopt;
    }

    switch (cube->type()) {
    case Cube::Type::SOLID: {

        // We found a leaf cube. The grid size does not matter anymore at this point. However, even if the cube is of
        // type solid, it could have arbitrary indentations. If any collision takes place and the cube is of type SOLID,
        // the collision must be inside of this cube. It's possible that the cube is indented in a way so there is more
        // than one collision inside of it. However there can only be one collision closest to the camera, which is the
        // final collision found. There are two possible outcomes for ray_cube_vertex_intersection:
        // 1) The bounding box of the cube is being intersected and the ray also collides with the vertex geometry
        // inside of the cube. In this case the collision data contains the intersection point between ray and the
        // vertex geometry of the cube, the bounding box intersection point, the selected face, the nearest corner,
        // and the nearest edge.
        // 2) The bounding box of the cube is being intersected, but the cube's geometry is not. In this case the
        // collision data contains only the bounding box intersection point, the selected face, the nearest corner, and
        // the nearest edge. ray_cube_vertex_intersection will return std::nullopt in this case.

        return std::make_optional<RayCubeCollision<Cube>>(cube, pos, dir, ray_cube_vertex_intersection(cube, pos, dir));
    }
    case Cube::Type::OCTANT: {

        if (grid_level_counter.value_or(1) == 0) {
            // We reached the smallest grid level and treat the current cube as if it was of type Cube::Type::SOLID.
            // TODO: How can we solve respecting octree indentation for this case?
            // TODO: We need to make sure that octree collision checks all vertices of all sub cubes in that case!
        }

        // We now must find the collision between ray and all subcubes of the octant which is closest to the camera.
        // In order to do this, we will not simply iterate recursively through all 8 subcubes and their subcubes and so
        // on and then sort the possible intersections by squared distance. Instead, we check for collision between ray
        // and the bounding sphere and bounding box of each subcube first and then sort the bounding collisions we found
        // by squared distance. We assume that this will be the actual ray-vertex collision is closest to the camera.

        const auto subcubes = cube->children();

        std::vector<std::pair<std::shared_ptr<Cube>, float>> collision_candidates;

        collision_candidates.reserve(8);

        for (const auto &subcube : subcubes) {
            if (!is_bounding_box_and_bounding_sphere_hit(subcube, pos, dir)) {
                continue;
            }

            // TODO: Implement min/max collision distance if required.

            collision_candidates.emplace_back(
                std::make_pair(subcube, glm::distance2(subcube->center(), cube->center())));
        }

        // Sort the subcube collision candidates by increasing square of distance between cube center and camera.
        std::sort(collision_candidates.begin(), collision_candidates.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs.second < rhs.second; });

        for (const auto &possible_collision : collision_candidates) {

            // Do not make this const auto as we want to return it. This could prevent automatic move semantics.
            auto collision_query = std::make_optional<RayCubeCollision<Cube>>(
                possible_collision.first, pos, dir, ray_cube_vertex_intersection(possible_collision.first, pos, dir));

            if (collision_query) {
                return collision_query;
            }
        }

        break;
    }
    default: {
        break;
    }
    }

    return std::nullopt;
}

} // namespace inexor::vulkan_renderer::world
