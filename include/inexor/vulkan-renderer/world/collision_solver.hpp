#pragma once

#include "inexor/vulkan-renderer/world/collision_query.hpp"

#include <memory>
#include <optional>
#include <vector>

// Forward declaration
namespace inexor::vulkan_renderer::world {
class Cube;
} // namespace inexor::vulkan_renderer::world

namespace inexor::vulkan_renderer::world {

class OctreeCollisionSolver {
private:
    std::vector<std::pair<std::shared_ptr<world::Cube>, float>> m_collision_candidates{};

public:

    /// @brief Default constructor.
    /// @param octree_count The number of octrees
    OctreeCollisionSolver(std::size_t octree_count);

    /// @brief Find a collision between a ray and an octree which is closest to the camera.
    /// @param worlds The octrees to check for collision
    /// @param position The start position of the ray
    /// @param direction The direction vector of the ray
    /// @return The collision data
    [[nodiscard]] std::optional<RayCubeCollision<Cube>>
    find_ray_octree_collision(std::vector<std::shared_ptr<world::Cube>> &worlds, glm::vec3 position,
                              glm::vec3 direction);

    // TODO: Implement collision with all octrees.
};

} // namespace inexor::vulkan_renderer::world
