﻿#pragma once

#include "inexor/vulkan-renderer/camera.hpp"
#include "inexor/vulkan-renderer/fps_counter.hpp"
#include "inexor/vulkan-renderer/input/keyboard_mouse_data.hpp"
#include "inexor/vulkan-renderer/octree_gpu_vertex.hpp"
#include "inexor/vulkan-renderer/render-graph/render_graph.hpp"
#include "inexor/vulkan-renderer/renderers/imgui.hpp"
#include "inexor/vulkan-renderer/time_step.hpp"
#include "inexor/vulkan-renderer/world/collision_query.hpp"
#include "inexor/vulkan-renderer/world/cube.hpp"
#include "inexor/vulkan-renderer/wrapper/instance.hpp"
#include "inexor/vulkan-renderer/wrapper/swapchain.hpp"
#include "inexor/vulkan-renderer/wrapper/window.hpp"
#include "inexor/vulkan-renderer/wrapper/window_surface.hpp"

// Forward declarations
namespace inexor::vulkan_renderer::input {
class KeyboardMouseInputData;
} // namespace inexor::vulkan_renderer::input

namespace inexor::vulkan_renderer {

class Application {
private:
    TimeStep m_stopwatch;
    FPSCounter m_fps_counter;
    bool m_vsync_enabled{false};

    PFN_vkDebugUtilsMessengerCallbackEXT m_debug_callbacks{VK_NULL_HANDLE};

    bool m_debug_report_callback_initialised{false};

    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<wrapper::Window> m_window;
    std::unique_ptr<wrapper::Instance> m_instance;
    std::unique_ptr<wrapper::Device> m_device;
    std::unique_ptr<wrapper::WindowSurface> m_surface;
    std::unique_ptr<wrapper::Swapchain> m_swapchain;
    std::unique_ptr<renderers::ImGuiRenderer> m_imgui_overlay;

    std::vector<OctreeGpuVertex> m_octree_vertices;
    std::vector<std::uint32_t> m_octree_indices;

    std::unique_ptr<render_graph::RenderGraph> m_render_graph;
    std::weak_ptr<render_graph::Texture> m_color_attachment;
    std::weak_ptr<render_graph::Texture> m_depth_attachment;
    std::weak_ptr<render_graph::Buffer> m_index_buffer;
    std::weak_ptr<render_graph::Buffer> m_vertex_buffer;
    std::weak_ptr<render_graph::Buffer> m_uniform_buffer;
    std::shared_ptr<wrapper::Shader> m_octree_vert;
    std::shared_ptr<wrapper::Shader> m_octree_frag;

    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};

    std::shared_ptr<wrapper::pipelines::GraphicsPipeline> m_octree_pipeline;
    std::shared_ptr<render_graph::GraphicsPass> m_octree_pass;

    struct ModelViewPerspectiveMatrices {
        glm::mat4 model{1.0f};
        glm::mat4 view{1.0f};
        glm::mat4 proj{1.0f};
    } m_mvp_matrices;

    void setup_render_graph();
    void generate_octree_indices();
    void recreate_swapchain();
    void render_frame();

    float m_time_passed{0.0f};

    std::uint32_t m_wnd_width{0};
    std::uint32_t m_wnd_height{0};
    std::string m_wnd_title;
    bool m_wnd_resized{false};
    wrapper::Window::Mode m_wnd_mode{wrapper::Window::Mode::WINDOWED};

    std::vector<std::string> m_gltf_model_files;
    std::unique_ptr<input::KeyboardMouseInputData> m_input_data;

    bool m_enable_validation_layers{true};
    std::vector<std::shared_ptr<world::Cube>> m_worlds;

    // If the user specified command line argument "--stop-on-validation-message", the program will call
    // std::abort(); after reporting a validation layer (error) message.
    bool m_stop_on_validation_message{false};

    /// @brief Load the configuration of the renderer from a TOML configuration file.
    /// @brief file_name The TOML configuration file.
    /// @note It was collectively decided not to use JSON for configuration files.
    void load_toml_configuration_file(const std::string &file_name);

    void check_octree_collisions();
    void initialize_spdlog();

    /// @param initialize Initialize worlds with a fixed seed, which is useful for benchmarking and testing
    void load_octree_geometry(bool initialize);

    void process_keyboard_input();
    void process_mouse_input();
    void setup_window_and_input_callbacks();
    void update_imgui_overlay();

public:
    Application(int argc, char **argv);
    Application(const Application &) = delete;
    Application(Application &&) = delete;
    ~Application();

    Application &operator=(const Application &) = delete;
    Application &operator=(Application &&) = delete;

    /// @brief Call glfwSetCursorPosCallback.
    /// @param window The window that received the event.
    /// @param x_pos The new x-coordinate, in screen coordinates, of the cursor.
    /// @param y_pos The new y-coordinate, in screen coordinates, of the cursor.
    void cursor_position_callback(GLFWwindow *window, double x_pos, double y_pos);

    /// @brief Call glfwSetKeyCallback.
    /// @param window The window that received the event.
    /// @param key The keyboard key that was pressed or released.
    /// @param scancode The system-specific scancode of the key.
    /// @param action GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
    /// @param mods Bit field describing which modifier keys were held down.
    void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

    /// @brief Call glfwSetMouseButtonCallback.
    /// @param window The window that received the event.
    /// @param button The mouse button that was pressed or released.
    /// @param action One of GLFW_PRESS or GLFW_RELEASE.
    /// @param mods Bit field describing which modifier keys were held down.
    void mouse_button_callback(GLFWwindow *window, int button, int action, int mods);

    /// @brief Call camera's process_mouse_scroll method.
    /// @param window The window that received the event.
    /// @param x_offset The change of x-offset of the mouse wheel.
    /// @param y_offset The change of y-offset of the mouse wheel.
    void mouse_scroll_callback(GLFWwindow *window, double x_offset, double y_offset);

    void run();
};

} // namespace inexor::vulkan_renderer
