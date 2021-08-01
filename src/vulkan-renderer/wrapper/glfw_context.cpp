#include "inexor/vulkan-renderer/wrapper/glfw_context.hpp"

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

namespace inexor::vulkan_renderer::wrapper {

GLFWContext::GLFWContext() {
    m_initialized = static_cast<bool>(glfwInit());
    if (!m_initialized) {
        throw std::runtime_error("Error: glfwInit failed!");
    }
}

GLFWContext::GLFWContext(GLFWContext &&other) noexcept {
    m_initialized = other.m_initialized;
}

GLFWContext::~GLFWContext() {
    glfwTerminate();
}

} // namespace inexor::vulkan_renderer::wrapper
