#include "inexor/vulkan-renderer/application.hpp"

#include "inexor/vulkan-renderer/exception.hpp"
#include "inexor/vulkan-renderer/meta.hpp"
#include "inexor/vulkan-renderer/octree_gpu_vertex.hpp"
#include "inexor/vulkan-renderer/render-graph/graphics_pass_builder.hpp"
#include "inexor/vulkan-renderer/standard_ubo.hpp"
#include "inexor/vulkan-renderer/tools/cla_parser.hpp"
#include "inexor/vulkan-renderer/vk_tools/enumerate.hpp"
#include "inexor/vulkan-renderer/world/collision.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptors/descriptor_set_update_frequency.hpp"
#include "inexor/vulkan-renderer/wrapper/instance.hpp"
#include "inexor/vulkan-renderer/wrapper/pipelines/pipeline_layout.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <toml.hpp>

#include <random>
#include <thread>

namespace inexor::vulkan_renderer {

VKAPI_ATTR VkBool32 VKAPI_CALL debug_messenger_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                                        VkDebugUtilsMessageTypeFlagsEXT type,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *data,
                                                        void *user_data) {
    // Validation layers have their own logger
    std::shared_ptr<spdlog::logger> m_validation_log{spdlog::default_logger()->clone("validation-layer")};

    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        m_validation_log->trace("{}", data->pMessage);
    }
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        m_validation_log->info("{}", data->pMessage);
    }
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        m_validation_log->warn("{}", data->pMessage);
    }
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        m_validation_log->critical("{}", data->pMessage);
    }
    return false;
}

Application::Application(int argc, char **argv) {
    spdlog::trace("Initialising vulkan-renderer");

    tools::CommandLineArgumentParser cla_parser;
    cla_parser.parse_args(argc, argv);

    spdlog::trace("Application version: {}.{}.{}", APP_VERSION[0], APP_VERSION[1], APP_VERSION[2]);
    spdlog::trace("Engine version: {}.{}.{}", ENGINE_VERSION[0], ENGINE_VERSION[1], ENGINE_VERSION[2]);

    // Load the configuration from the TOML file.
    load_toml_configuration_file("configuration/renderer.toml");

    // If the user specified command line argument "--no-validation", the Khronos validation instance layer will be
    // disabled. For debug builds, this is not advisable! Always use validation layers during development!
    const auto disable_validation = cla_parser.arg<bool>("--no-validation");
    if (disable_validation.value_or(false)) {
        spdlog::warn("--no-validation specified, disabling validation layers");
        m_enable_validation_layers = false;
    }

    m_window =
        std::make_unique<wrapper::Window>(m_window_title, m_window_width, m_window_height, true, true, m_window_mode);

    spdlog::trace("Creating Vulkan instance");

    m_instance = std::make_unique<wrapper::Instance>(
        APP_NAME, ENGINE_NAME, VK_MAKE_API_VERSION(0, APP_VERSION[0], APP_VERSION[1], APP_VERSION[2]),
        VK_MAKE_API_VERSION(0, ENGINE_VERSION[0], ENGINE_VERSION[1], ENGINE_VERSION[2]), m_enable_validation_layers,
        debug_messenger_callback);

    vk_tools::print_driver_vulkan_version();

    m_input_data = std::make_unique<input::KeyboardMouseInputData>();

    m_surface = std::make_unique<wrapper::WindowSurface>(m_instance->instance(), m_window->get());

    setup_window_and_input_callbacks();

#ifndef NDEBUG
    if (cla_parser.arg<bool>("--stop-on-validation-message").value_or(false)) {
        spdlog::warn("--stop-on-validation-message specified. Application will call a breakpoint after reporting a "
                     "validation layer message");
        m_stop_on_validation_message = true;
    }
#endif

    spdlog::trace("Creating window surface");

    // The user can specify with "--gpu <number>" which graphics card to prefer.
    auto preferred_graphics_card = cla_parser.arg<std::uint32_t>("--gpu");
    if (preferred_graphics_card) {
        spdlog::trace("Preferential graphics card index {} specified", *preferred_graphics_card);
    }

    bool display_graphics_card_info = true;

    // If the user specified command line argument "--nostats", no information will be
    // displayed about all the graphics cards which are available on the system.
    const auto hide_gpu_stats = cla_parser.arg<bool>("--no-stats");
    if (hide_gpu_stats.value_or(false)) {
        spdlog::trace("--no-stats specified, no extended information about graphics cards will be shown");
        display_graphics_card_info = false;
    }

    // If the user specified command line argument "--vsync", the presentation engine waits
    // for the next vertical blanking period to update the current image.
    const auto enable_vertical_synchronisation = cla_parser.arg<bool>("--vsync");
    if (enable_vertical_synchronisation.value_or(false)) {
        spdlog::trace("V-sync enabled!");
        m_vsync_enabled = true;
    } else {
        spdlog::trace("V-sync disabled!");
        m_vsync_enabled = false;
    }

    if (display_graphics_card_info) {
        vk_tools::print_all_physical_devices(m_instance->instance(), m_surface->get());
    }

    bool use_distinct_data_transfer_queue = true;

    // Ignore distinct data transfer queue
    const auto forbid_distinct_data_transfer_queue = cla_parser.arg<bool>("--no-separate-data-queue");
    if (forbid_distinct_data_transfer_queue.value_or(false)) {
        spdlog::warn("Command line argument --no-separate-data-queue specified");
        spdlog::warn("This will force the application to avoid using a distinct queue for data transfer to GPU");
        spdlog::warn("Performance loss might be a result of this!");
        use_distinct_data_transfer_queue = false;
    }

    const auto physical_devices = vk_tools::get_physical_devices(m_instance->instance());
    if (preferred_graphics_card && *preferred_graphics_card >= physical_devices.size()) {
        spdlog::critical("GPU index {} out of range!", *preferred_graphics_card);
        throw std::runtime_error("Invalid GPU index");
    }

    const VkPhysicalDeviceFeatures required_features{
        // Add required physical device features here
        .sampleRateShading = VK_TRUE,
        .samplerAnisotropy = VK_TRUE,
    };

    const VkPhysicalDeviceFeatures optional_features{
        // Add optional physical device features here
        // TODO: Add callback on_device_feature_not_available and remove optional features
        // Then, return true or false from the callback, indicating if you can run the app
        // without this physical device feature being present.
    };

    // TODO: Also implement a callback for required extensions
    std::vector<const char *> required_extensions{
        // Since we want to draw on a window, we need the swapchain extension
        VK_KHR_SWAPCHAIN_EXTENSION_NAME, // VK_KHR_swapchain
        // We are using dynamic rendering
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME, // VK_KHR_dynamic_rendering
        // The following is required by VK_KHR_dynamic_rendering
        VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME, // VK_KHR_depth_stencil_resolve
        // The following is required by VK_KHR_depth_stencil_resolve
        VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME, // VK_KHR_create_renderpass2
    };

    const VkPhysicalDevice physical_device =
        preferred_graphics_card ? physical_devices[*preferred_graphics_card]
                                : wrapper::Device::pick_best_physical_device(*m_instance, m_surface->get(),
                                                                             required_features, required_extensions);

    m_device =
        std::make_unique<wrapper::Device>(*m_instance, m_surface->get(), use_distinct_data_transfer_queue,
                                          physical_device, required_extensions, required_features, optional_features);

    m_swapchain = std::make_unique<wrapper::Swapchain>(*m_device, m_surface->get(), m_window->width(),
                                                       m_window->height(), m_vsync_enabled);

    load_octree_geometry(true);
    generate_octree_indices();

    m_vertex_shader = std::make_unique<wrapper::Shader>(*m_device, VK_SHADER_STAGE_VERTEX_BIT, "Shader Octree",
                                                        "shaders/main.vert.spv");
    m_fragment_shader = std::make_unique<wrapper::Shader>(*m_device, VK_SHADER_STAGE_FRAGMENT_BIT, "Shader Octree",
                                                          "shaders/main.frag.spv");

    m_window->show();
    recreate_swapchain();
}

Application::~Application() {
    spdlog::trace("Shutting down vulkan renderer");
}

void Application::check_octree_collisions() {
    // Check for collision between camera ray and every octree
    for (const auto &world : m_worlds) {
        const auto collision = ray_cube_collision_check(*world, m_camera->position(), m_camera->front());

        if (collision) {
            const auto intersection = collision.value().intersection();
            const auto face_normal = collision.value().face();
            const auto corner = collision.value().corner();
            const auto edge = collision.value().edge();

            spdlog::trace("pos {} {} {} | face {} {} {} | corner {} {} {} | edge {} {} {}", intersection.x,
                          intersection.y, intersection.z, face_normal.x, face_normal.y, face_normal.z, corner.x,
                          corner.y, corner.z, edge.x, edge.y, edge.z);

            // Break after one collision.
            break;
        }
    }
}

void Application::cursor_position_callback(GLFWwindow * /*window*/, double x_pos, double y_pos) {
    m_input_data->set_cursor_pos(x_pos, y_pos);
}

void Application::generate_octree_indices() {
    auto old_vertices = std::move(m_octree_vertices);
    m_octree_indices.clear();
    m_octree_vertices.clear();
    std::unordered_map<OctreeGpuVertex, std::uint32_t> vertex_map;
    for (auto &vertex : old_vertices) {
        // TODO: Use std::unordered_map::contains() when we switch to C++ 20.
        if (vertex_map.count(vertex) == 0) {
            assert(vertex_map.size() < std::numeric_limits<std::uint32_t>::max() && "Octree too big!");
            vertex_map.emplace(vertex, static_cast<std::uint32_t>(vertex_map.size()));
            m_octree_vertices.push_back(vertex);
        }
        m_octree_indices.push_back(vertex_map.at(vertex));
    }
    spdlog::trace("Reduced octree by {} vertices (from {} to {})", old_vertices.size() - m_octree_vertices.size(),
                  old_vertices.size(), m_octree_vertices.size());
    spdlog::trace("Total indices {} ", m_octree_indices.size());
}

void Application::key_callback(GLFWwindow * /*window*/, int key, int, int action, int /*mods*/) {
    if (key < 0 || key > GLFW_KEY_LAST) {
        return;
    }

    switch (action) {
    case GLFW_PRESS:
        m_input_data->press_key(key);
        break;
    case GLFW_RELEASE:
        m_input_data->release_key(key);
        break;
    default:
        break;
    }
}

void Application::load_toml_configuration_file(const std::string &file_name) {
    spdlog::trace("Loading TOML configuration file: {}", file_name);

    std::ifstream toml_file(file_name, std::ios::in);
    if (!toml_file) {
        // If you are using CLion, go to "Edit Configurations" and select "Working Directory".
        throw std::runtime_error("Could not find configuration file: " + file_name +
                                 "! You must set the working directory properly in your IDE");
    }

    toml_file.close();

    // Load the TOML file using toml11.
    auto renderer_configuration = toml::parse(file_name);

    // Search for the title of the configuration file and print it to debug output.
    const auto &configuration_title = toml::find<std::string>(renderer_configuration, "title");
    spdlog::trace("Title: {}", configuration_title);

    using WindowMode = ::inexor::vulkan_renderer::wrapper::Window::Mode;
    const auto &wmodestr = toml::find<std::string>(renderer_configuration, "application", "window", "mode");
    if (wmodestr == "windowed") {
        m_window_mode = WindowMode::WINDOWED;
    } else if (wmodestr == "windowed_fullscreen") {
        m_window_mode = WindowMode::WINDOWED_FULLSCREEN;
    } else if (wmodestr == "fullscreen") {
        m_window_mode = WindowMode::FULLSCREEN;
    } else {
        spdlog::warn("Invalid application window mode: {}", wmodestr);
        m_window_mode = WindowMode::WINDOWED;
    }

    m_window_width = toml::find<int>(renderer_configuration, "application", "window", "width");
    m_window_height = toml::find<int>(renderer_configuration, "application", "window", "height");
    m_window_title = toml::find<std::string>(renderer_configuration, "application", "window", "name");
    spdlog::trace("Window: {}, {} x {}", m_window_title, m_window_width, m_window_height);

    m_gltf_model_files = toml::find<std::vector<std::string>>(renderer_configuration, "glTFmodels", "files");

    spdlog::trace("glTF 2.0 models:");

    for (const auto &gltf_model_file : m_gltf_model_files) {
        spdlog::trace("   - {}", gltf_model_file);
    }
}

void Application::load_octree_geometry(bool initialize) {
    spdlog::trace("Creating octree geometry");

    // 4: 23 012 | 5: 184352 | 6: 1474162 | 7: 11792978 cubes, DO NOT USE 7!
    m_worlds.clear();
    m_worlds.push_back(
        world::create_random_world(2, {0.0f, 0.0f, 0.0f}, initialize ? std::optional(42) : std::nullopt));
    m_worlds.push_back(
        world::create_random_world(2, {10.0f, 0.0f, 0.0f}, initialize ? std::optional(60) : std::nullopt));

    m_octree_vertices.clear();
    for (const auto &world : m_worlds) {
        for (const auto &polygons : world->polygons(true)) {
            for (const auto &triangle : *polygons) {
                for (const auto &vertex : triangle) {
                    glm::vec3 color = {
                        static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                        static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                        static_cast<float>(rand()) / static_cast<float>(RAND_MAX),
                    };
                    m_octree_vertices.emplace_back(vertex, color);
                }
            }
        }
    }
}

void Application::mouse_button_callback(GLFWwindow * /*window*/, int button, int action, int /*mods*/) {
    if (button < 0 || button > GLFW_MOUSE_BUTTON_LAST) {
        return;
    }

    switch (action) {
    case GLFW_PRESS:
        m_input_data->press_mouse_button(button);
        break;
    case GLFW_RELEASE:
        m_input_data->release_mouse_button(button);
        break;
    default:
        break;
    }
}

void Application::mouse_scroll_callback(GLFWwindow * /*window*/, double /*x_offset*/, double y_offset) {
    m_camera->change_zoom(static_cast<float>(y_offset));
}

void Application::process_keyboard_input() {}

void Application::process_mouse_input() {
    const auto cursor_pos_delta = m_input_data->calculate_cursor_position_delta();

    if (m_camera->type() == CameraType::LOOK_AT && m_input_data->is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT)) {
        m_camera->rotate(static_cast<float>(cursor_pos_delta[0]), -static_cast<float>(cursor_pos_delta[1]));
    }

    m_camera->set_movement_state(CameraMovement::FORWARD, m_input_data->is_key_pressed(GLFW_KEY_W));
    m_camera->set_movement_state(CameraMovement::LEFT, m_input_data->is_key_pressed(GLFW_KEY_A));
    m_camera->set_movement_state(CameraMovement::BACKWARD, m_input_data->is_key_pressed(GLFW_KEY_S));
    m_camera->set_movement_state(CameraMovement::RIGHT, m_input_data->is_key_pressed(GLFW_KEY_D));
}

void Application::recreate_swapchain() {
    m_window->wait_for_focus();
    m_device->wait_idle();

    // Query the framebuffer size here again although the window width is set during framebuffer resize callback
    // The reason for this is that the framebuffer size could already be different again because we missed a poll
    // This seems to be an issue on Linux only though
    int window_width = 0;
    int window_height = 0;
    glfwGetFramebufferSize(m_window->get(), &window_width, &window_height);

    m_swapchain->setup_swapchain(window_width, window_height, m_vsync_enabled);

    m_render_graph = std::make_unique<render_graph::RenderGraph>(*m_device, *m_swapchain);
    setup_render_graph();

    // TODO: Do we really have to recreate the camera every time we recreate swapchain?
    m_camera = std::make_unique<Camera>(glm::vec3(6.0f, 10.0f, 2.0f), 180.0f, 0.0f,
                                        static_cast<float>(m_window->width()), static_cast<float>(m_window->height()));

    m_camera->set_movement_speed(5.0f);
    m_camera->set_rotation_speed(0.5f);

    m_imgui_overlay = std::make_unique<ImGuiOverlay>(*m_device, *m_render_graph.get(), m_back_buffer, m_msaa_color,
                                                     [&]() { update_imgui_overlay(); });

    m_render_graph->compile();
}

void Application::render_frame() {
    if (m_window_resized) {
        m_window_resized = false;
        recreate_swapchain();
        return;
    }

    m_render_graph->render();

    if (auto fps_value = m_fps_counter.update()) {
        m_window->set_title("Inexor Vulkan API renderer demo - " + std::to_string(*fps_value) + " FPS");
        spdlog::trace("FPS: {}, window size: {} x {}", *fps_value, m_window->width(), m_window->height());
    }
}

void Application::run() {
    spdlog::trace("Running Application");

    while (!m_window->should_close()) {
        m_window->poll();
        process_keyboard_input();
        process_mouse_input();
        m_camera->update(m_time_passed);
        m_time_passed = m_stopwatch.time_step();
        check_octree_collisions();
        render_frame();
    }
}

void Application::setup_render_graph() {
    using render_graph::TextureUsage;

    m_back_buffer = m_render_graph->add_texture("Color",                   //
                                                TextureUsage::BACK_BUFFER, //
                                                m_swapchain->image_format());

    m_msaa_color = m_render_graph->add_texture("MSAA color",                   //
                                               TextureUsage::MSAA_BACK_BUFFER, //
                                               m_swapchain->image_format());

    m_depth_buffer = m_render_graph->add_texture("Depth",                            //
                                                 TextureUsage::DEPTH_STENCIL_BUFFER, //
                                                 VK_FORMAT_D32_SFLOAT_S8_UINT);

    m_msaa_depth = m_render_graph->add_texture("MSAA depth",                            //
                                               TextureUsage::MSAA_DEPTH_STENCIL_BUFFER, //
                                               VK_FORMAT_D32_SFLOAT_S8_UINT);

    using render_graph::BufferType;
    using wrapper::descriptors::DescriptorSetUpdateFrequency;

    m_vertex_buffer = m_render_graph->add_buffer("Octree", BufferType::VERTEX_BUFFER, [&]() {
        // If the key N was pressed once, we generate a new octree
        if (m_input_data->was_key_pressed_once(GLFW_KEY_N)) {
            load_octree_geometry(false);
            generate_octree_indices();
            // We update the vertex buffer together with the index buffer
            // to keep data consistent across frames
            m_vertex_buffer.lock()->request_update(m_octree_vertices);
            m_index_buffer.lock()->request_update(m_octree_indices);
        }
    });

    // Note that the index buffer is updated together with the vertex buffer to keep data consistent
    // TODO: FIX ME!
    m_index_buffer = m_render_graph->add_buffer("Octree", BufferType::INDEX_BUFFER, [] {});

    // Update the vertex buffer and index buffer at initialization
    // Note that we update the vertex buffer together with the index buffer to keep data consistent
    m_vertex_buffer.lock()->request_update(m_octree_vertices);
    m_index_buffer.lock()->request_update(m_octree_indices);

    m_uniform_buffer = m_render_graph->add_buffer("Matrices", BufferType::UNIFORM_BUFFER, [&]() {
        // The m_mvp_matrices.model matrix doesn't need to be updated
        m_mvp_matrices.view = m_camera->view_matrix();
        m_mvp_matrices.proj = m_camera->perspective_matrix();
        m_mvp_matrices.proj[1][1] *= -1;
        m_uniform_buffer.lock()->request_update(m_mvp_matrices);
    });

    // TODO: How to associate pipeline layouts with pipelines?
    m_render_graph->add_graphics_pipeline(
        [&](wrapper::pipelines::GraphicsPipelineBuilder &builder, const VkPipelineLayout pipeline_layout) {
            m_octree_pipeline = builder
                                    .add_color_blend_attachment({
                                        .blendEnable = VK_FALSE,
                                        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
                                    })
                                    .set_vertex_input_bindings({
                                        {
                                            .binding = 0,
                                            .stride = sizeof(OctreeGpuVertex),
                                            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                                        },
                                    })
                                    .set_vertex_input_attributes({
                                        {
                                            .location = 0,
                                            .binding = 0,
                                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                                            .offset = offsetof(OctreeGpuVertex, position),
                                        },
                                        {
                                            .location = 1,
                                            .binding = 0,
                                            .format = VK_FORMAT_R32G32B32_SFLOAT,
                                            .offset = offsetof(OctreeGpuVertex, color),
                                        },
                                    })
                                    .set_viewport(m_swapchain->extent())
                                    .set_scissor(m_swapchain->extent())
                                    .set_pipeline_layout(pipeline_layout)
                                    .add_shader(*m_vertex_shader)
                                    .add_shader(*m_fragment_shader)
                                    .build("Octree");
            return m_octree_pipeline;
        });

    m_render_graph->add_graphics_pass([&](render_graph::GraphicsPassBuilder &builder) {
        m_octree_pass = builder
                            .set_clear_value({
                                .color = {1.0f, 0.0f, 0.0f},
                            })
                            .set_depth_test(true)
                            .set_on_record([&](const wrapper::CommandBuffer &cmd_buf) {
                                // Render octree
                                cmd_buf.bind_pipeline(*m_octree_pipeline)
                                    .bind_vertex_buffer(m_vertex_buffer)
                                    .bind_index_buffer(m_index_buffer)
                                    .draw_indexed(static_cast<std::uint32_t>(m_octree_indices.size()));
                            })
                            .reads_from_buffer(m_index_buffer)
                            .reads_from_buffer(m_vertex_buffer)
                            .reads_from_buffer(m_uniform_buffer, VK_SHADER_STAGE_VERTEX_BIT)
                            .writes_to_texture(m_back_buffer)
                            .writes_to_texture(m_depth_buffer)
                            .build("Octree");
        return m_octree_pass;
    });
}

void Application::setup_window_and_input_callbacks() {
    m_window->set_user_ptr(this);

    spdlog::trace("Setting up window callback:");

    auto lambda_frame_buffer_resize_callback = [](GLFWwindow *window, int width, int height) {
        auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        spdlog::trace("Frame buffer resize callback called. window width: {}, height: {}", width, height);
        app->m_window_resized = true;
    };

    m_window->set_resize_callback(lambda_frame_buffer_resize_callback);

    spdlog::trace("   - keyboard button callback");

    auto lambda_key_callback = [](GLFWwindow *window, int key, int scancode, int action, int mods) {
        auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        app->key_callback(window, key, scancode, action, mods);
    };

    m_window->set_keyboard_button_callback(lambda_key_callback);

    spdlog::trace("   - cursor position callback");

    auto lambda_cursor_position_callback = [](GLFWwindow *window, double xpos, double ypos) {
        auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        app->cursor_position_callback(window, xpos, ypos);
    };

    m_window->set_cursor_position_callback(lambda_cursor_position_callback);

    spdlog::trace("   - mouse button callback");

    auto lambda_mouse_button_callback = [](GLFWwindow *window, int button, int action, int mods) {
        auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        app->mouse_button_callback(window, button, action, mods);
    };

    m_window->set_mouse_button_callback(lambda_mouse_button_callback);

    spdlog::trace("   - mouse wheel scroll callback");

    auto lambda_mouse_scroll_callback = [](GLFWwindow *window, double xoffset, double yoffset) {
        auto *app = static_cast<Application *>(glfwGetWindowUserPointer(window));
        app->mouse_scroll_callback(window, xoffset, yoffset);
    };

    m_window->set_mouse_scroll_callback(lambda_mouse_scroll_callback);
}

void Application::update_imgui_overlay() {
    ImGuiIO &io = ImGui::GetIO();
    io.DeltaTime = m_time_passed + 0.00001f;
    auto cursor_pos = m_input_data->get_cursor_pos();
    io.MousePos = ImVec2(static_cast<float>(cursor_pos[0]), static_cast<float>(cursor_pos[1]));
    io.MouseDown[0] = m_input_data->is_mouse_button_pressed(GLFW_MOUSE_BUTTON_LEFT);
    io.MouseDown[1] = m_input_data->is_mouse_button_pressed(GLFW_MOUSE_BUTTON_RIGHT);
    io.DisplaySize =
        ImVec2(static_cast<float>(m_swapchain->extent().width), static_cast<float>(m_swapchain->extent().height));

    ImGui::NewFrame();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::SetNextWindowPos(ImVec2(10, 10));
    ImGui::SetNextWindowSize(ImVec2(330, 0));
    ImGui::Begin("Inexor Vulkan-renderer", nullptr,
                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    ImGui::Text("%s", m_device->gpu_name().c_str());
    ImGui::Text("Engine version %d.%d.%d (Git sha %s)", ENGINE_VERSION[0], ENGINE_VERSION[1], ENGINE_VERSION[2],
                BUILD_GIT);
    ImGui::Text("Vulkan API %d.%d.%d", VK_API_VERSION_MAJOR(wrapper::Instance::REQUIRED_VK_API_VERSION),
                VK_API_VERSION_MINOR(wrapper::Instance::REQUIRED_VK_API_VERSION),
                VK_API_VERSION_PATCH(wrapper::Instance::REQUIRED_VK_API_VERSION));
    const auto cam_pos = m_camera->position();
    ImGui::Text("Camera position (%.2f, %.2f, %.2f)", cam_pos.x, cam_pos.y, cam_pos.z);
    const auto cam_rot = m_camera->rotation();
    ImGui::Text("Camera rotation: (%.2f, %.2f, %.2f)", cam_rot.x, cam_rot.y, cam_rot.z);
    const auto cam_front = m_camera->front();
    ImGui::Text("Camera vector front: (%.2f, %.2f, %.2f)", cam_front.x, cam_front.y, cam_front.z);
    const auto cam_right = m_camera->right();
    ImGui::Text("Camera vector right: (%.2f, %.2f, %.2f)", cam_right.x, cam_right.y, cam_right.z);
    const auto cam_up = m_camera->up();
    ImGui::Text("Camera vector up (%.2f, %.2f, %.2f)", cam_up.x, cam_up.y, cam_up.z);
    ImGui::Text("Yaw: %.2f pitch: %.2f roll: %.2f", m_camera->yaw(), m_camera->pitch(), m_camera->roll());
    const auto cam_fov = m_camera->fov();
    ImGui::Text("Field of view: %d", static_cast<std::uint32_t>(cam_fov));
    ImGui::PushItemWidth(150.0f);
    ImGui::PopItemWidth();
    ImGui::End();
    ImGui::PopStyleVar();
    ImGui::Render();
}

} // namespace inexor::vulkan_renderer
