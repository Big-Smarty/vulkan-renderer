#pragma once

#include "inexor/vulkan-renderer/world/collision_query.hpp"

#include <memory>
#include <optional>
#include <shared_mutex>
#include <vector>

// Forward declaration
namespace inexor::vulkan_renderer::world {
class Cube;
} // namespace inexor::vulkan_renderer::world

namespace inexor::vulkan_renderer::world {

class OctreeCollisionSolver {
private:
    const std::vector<std::shared_ptr<world::Cube>> m_worlds;
    std::vector<std::pair<std::shared_ptr<world::Cube>, float>> m_collision_candidates;
    std::shared_mutex m_collision_solver_mutex;

    /// @brief Find all collisions between the given ray and the given octrees, sorted by increasing distance between
    /// the octree and the camera.
    /// @param position The start position of the ray
    /// @param direction The direction vector of the ray
    /// @return A vector of the found collisions. If no collisions were found, the vector is empty.
    [[nodiscard]] std::vector<RayCubeCollision<Cube>>
    find_all_ray_octree_collisions(glm::vec3 position, glm::vec3 direction, bool find_only_one_collision = true);

public:
    /// @brief Default constructor
    /// @param worlds The octree worlds to test collision with
    explicit OctreeCollisionSolver(const std::vector<std::shared_ptr<world::Cube>> &worlds);

    // TODO: Implement min/max collision distance if required.
    // TODO: Start sorting octrees by distance to camera only after a certain number of worlds.
    // TODO: Implement a bounding volume hierarchy (BVH) tree, for example an octree for all the octrees.

    /// @brief Find a collision between a ray and an octree which is closest to the camera.
    /// @param position The start position of the ray
    /// @param direction The direction vector of the ray
    /// @return The collision data
    [[nodiscard]] std::optional<RayCubeCollision<Cube>> find_ray_octree_collision(glm::vec3 position,
                                                                                  glm::vec3 direction);

    /// @brief Find all collisions between the given ray and the given octrees, sorted by increasing distance between
    /// the octree and the camera.
    /// @note Expect this method to be more costly than find_ray_octree_collision!
    /// @param position The start position of the ray
    /// @param direction The direction vector of the ray
    /// @return A vector of the found collisions. If no collisions were found, the vector is empty.
    [[nodiscard]] std::vector<RayCubeCollision<Cube>> find_all_ray_octree_collisions(glm::vec3 position,
                                                                                     glm::vec3 direction);
};

} // namespace inexor::vulkan_renderer::world
