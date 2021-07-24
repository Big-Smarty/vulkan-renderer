#include "inexor/vulkan-renderer/world/collision_solver.hpp"

#include <glm/gtx/norm.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cassert>
#include <mutex>

namespace inexor::vulkan_renderer::world {

OctreeCollisionSolver::OctreeCollisionSolver(const std::size_t octree_count) {
    assert(octree_count > 0);
    m_collision_candidates.reserve(octree_count);
}

std::optional<RayCubeCollision<Cube>>
OctreeCollisionSolver::find_ray_octree_collision(const std::vector<std::shared_ptr<world::Cube>> &worlds,
                                                 const glm::vec3 position, const glm::vec3 direction) {

    if (worlds.empty()) {
        return std::nullopt;
    }

    // We need a critical section because we are modifying m_collision_candidates.
    std::scoped_lock lock(m_collision_solver_mutex);
    {
        for (const auto &world : worlds) {
            if (!is_bounding_box_and_bounding_sphere_hit(world, position, direction)) {
                continue;
            }

            // TODO: Implement min/max collision distance if required.
            m_collision_candidates.emplace_back(std::make_pair(world, glm::distance2(position, direction)));
        }

        // TODO: Start sorting only after a certain number of worlds.
        // Sort the octree collision candidates by increasing square of distance between cube center and camera.
        std::sort(m_collision_candidates.begin(), m_collision_candidates.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs.second < rhs.second; });

        // Check for collision between camera ray and every octree whose bounding box and bounding sphere is hit in
        // order of increasing squared distance between camera and octree's center.
        for (const auto &collision_candidate : m_collision_candidates) {

            // Do not make this const auto as we want to return it. This could prevent automatic move semantics.
            auto collision = ray_cube_collision_check(collision_candidate.first, position, direction);

            if (collision) {
                return collision;
            }
        }
    }
    // End of critical section.

    // No collision found with the bounding sphere or bounding box of any of the octrees.
    return std::nullopt;
}

std::vector<RayCubeCollision<Cube>>
OctreeCollisionSolver::find_all_ray_octree_collisions(const std::vector<std::shared_ptr<world::Cube>> &worlds,
                                                      const glm::vec3 position, const glm::vec3 direction) {
    if (worlds.empty()) {
        return {};
    }

    std::vector<RayCubeCollision<Cube>> found_collisions;

    found_collisions.reserve(worlds.size());

    // We need a critical section because we are modifying m_collision_candidates.
    std::scoped_lock lock(m_collision_solver_mutex);
    {
        for (const auto &world : worlds) {
            if (!is_bounding_box_and_bounding_sphere_hit(world, position, direction)) {
                continue;
            }

            // TODO: Implement min/max collision distance if required.
            m_collision_candidates.emplace_back(std::make_pair(world, glm::distance2(position, direction)));
        }

        // TODO: Start sorting only after a certain number of worlds.
        // Sort the octree collision candidates by increasing square of distance between cube center and camera.
        std::sort(m_collision_candidates.begin(), m_collision_candidates.end(),
                  [](const auto &lhs, const auto &rhs) { return lhs.second < rhs.second; });

        // Check for collision between camera ray and every octree whose bounding box and bounding sphere is hit in
        // order of increasing squared distance between camera and octree's center.
        for (const auto &collision_candidate : m_collision_candidates) {

            // Do not make this const auto as we want to return it. This could prevent automatic move semantics.
            auto collision = ray_cube_collision_check(collision_candidate.first, position, direction);

            if (collision) {
                found_collisions.emplace_back(collision.value());
            }
        }
    }
    // End of critical section.

    return std::move(found_collisions);
}

} // namespace inexor::vulkan_renderer::world
