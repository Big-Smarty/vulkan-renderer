﻿#pragma once

#include "inexor/vulkan-renderer/render-graph/render_graph.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <volk.h>

#include <memory>
#include <vector>

namespace inexor::vulkan_renderer::wrapper {
// Forward declarations
class Device;
class Shader;
class Swapchain;
} // namespace inexor::vulkan_renderer::wrapper

namespace inexor::vulkan_renderer::pipelines {
// Forward declaration
class GraphicsPipeline;
} // namespace inexor::vulkan_renderer::pipelines

namespace inexor::vulkan_renderer::render_graph {
// Forward declarations
class Buffer;
class RenderGraph;
class GraphicsPass;
} // namespace inexor::vulkan_renderer::render_graph

namespace inexor::vulkan_renderer::renderers {

// Using declarations
using render_graph::Buffer;
using render_graph::GraphicsPass;
using render_graph::RenderGraph;
using render_graph::Texture;
using wrapper::Device;
using wrapper::Shader;
using wrapper::Swapchain;
using wrapper::pipelines::GraphicsPipeline;

/// A wrapper for an ImGui implementation
class ImGuiRenderer {
    std::weak_ptr<Buffer> m_index_buffer;
    std::weak_ptr<Buffer> m_vertex_buffer;
    std::weak_ptr<Texture> m_imgui_texture;
    std::shared_ptr<GraphicsPipeline> m_imgui_pipeline;

    // This is the color attachment we will write to
    std::weak_ptr<Swapchain> m_swapchain;
    std::weak_ptr<Texture> m_color_attachment;
    // This is the previous pass we read from
    std::weak_ptr<GraphicsPass> m_previous_pass;

    std::shared_ptr<Shader> m_vertex_shader;
    std::shared_ptr<Shader> m_fragment_shader;

    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptor_set{VK_NULL_HANDLE};

    // We need to collect the vertices and indices generated by ImGui
    // because it does not store them in one array, but rather in chunks
    std::vector<std::uint32_t> m_index_data;
    std::vector<ImDrawVert> m_vertex_data;

    unsigned char *m_font_texture_data{nullptr};
    int m_font_texture_width{0};
    int m_font_texture_height{0};
    int m_font_texture_data_size{0};

    // Neither scale nor translation change
    struct PushConstBlock {
        glm::vec2 scale{-1.0f};
        glm::vec2 translate{-1.0f};
    } m_push_const_block;

    /// The user's ImGui data will be updated in this function
    /// It will be called at the beginning of set_on_update
    std::function<void()> m_on_update_user_data{[]() {}};

    void load_font_data_from_file();

    /// Customize ImGui style like text color for example
    void set_imgui_style();

public:
    /// Default constructor
    /// @param device The device wrapper
    /// @param render_graph The rendergraph
    /// @param previous_pass The previous pass
    /// @param color_attachment The color attachment
    /// @param swapchain The swapchain to render to
    /// @param on_update_user_data The user-specified ImGui update function
    ImGuiRenderer(const Device &device,
                  std::weak_ptr<RenderGraph> render_graph,
                  std::weak_ptr<GraphicsPass> previous_pass,
                  std::weak_ptr<Swapchain> swapchain,
                  std::function<void()> on_update_user_data);

    ImGuiRenderer(const ImGuiRenderer &) = delete;
    ImGuiRenderer(ImGuiRenderer &&) noexcept;

    /// Call ImGui::DestroyContext
    ~ImGuiRenderer();

    ImGuiRenderer &operator=(const ImGuiRenderer &) = delete;
    ImGuiRenderer &operator=(ImGuiRenderer &&) = delete;
};

} // namespace inexor::vulkan_renderer::renderers
