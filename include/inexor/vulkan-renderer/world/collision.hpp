#pragma once

#include <glm/vec3.hpp>

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <tuple>

namespace inexor::vulkan_renderer::world {

/// @brief A wrapper for collision data which describes ray-octree collision.
/// This class is used for octree collision, but it can be used for every cube-like data structure
/// @tparam T A type which offers a size() and center() method.

template <typename T>
class RayCubeCollision {
    const std::shared_ptr<T> m_cube;

    // If we find a ray-cube collision, there will always be a collision with the bounding box.
    // However a collision with the cube's vertex geometry is no always present.
    std::optional<glm::vec3> m_vertex_intersection;

    glm::vec3 m_cube_intersection;
    glm::vec3 m_cube_face;
    glm::vec3 m_nearest_cube_corner;
    glm::vec3 m_nearest_cube_edge;

public:
    /// @brief Default constructor
    /// @param The cube to check collisions with
    /// @note We need to pass cube as a std::shared_ptr by copy in this case because we want to ensure the lifetime
    /// of the object in parallelized code. We are not accepting const references here, as they could become invalid.
    /// @param ray The start point of the ray
    /// @param dir The direction of the ray
    /// @param The intersection between ray and vertex geometry of the cube which was found
    RayCubeCollision(std::shared_ptr<T> cube, glm::vec3 ray, glm::vec3 dir, // NOLINT
                     std::optional<glm::vec3> vertex_intersection = std::nullopt);

    /// @note This method returns a copy of the cube, not a const reference.
    [[nodiscard]] std::shared_ptr<T> cube() const noexcept {
        return m_cube;
    }

    [[nodiscard]] auto vertex_intersection() const noexcept {
        return m_vertex_intersection;
    }

    [[nodiscard]] const glm::vec3 &cube_intersection() const noexcept {
        return m_cube_intersection;
    }

    [[nodiscard]] const glm::vec3 &cube_face() const noexcept {
        return m_cube_face;
    }

    [[nodiscard]] const glm::vec3 &nearest_cube_corner() const noexcept {
        return m_nearest_cube_corner;
    }

    [[nodiscard]] const glm::vec3 &nearest_cube_edge() const noexcept {
        return m_nearest_cube_edge;
    }
};

} // namespace inexor::vulkan_renderer::world
