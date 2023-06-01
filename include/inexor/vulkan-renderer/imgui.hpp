﻿#pragma once

#include "inexor/vulkan-renderer/render_graph.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptor.hpp"
#include "inexor/vulkan-renderer/wrapper/gpu_texture.hpp"
#include "inexor/vulkan-renderer/wrapper/shader.hpp"

#include <glm/vec2.hpp>
#include <imgui.h>
#include <volk.h>

#include <memory>
#include <vector>

// Forward declarations
namespace inexor::vulkan_renderer::wrapper {
class Device;
} // namespace inexor::vulkan_renderer::wrapper

namespace inexor::vulkan_renderer {

class ImGUIOverlay {
    const wrapper::Device &m_device;

    BufferResource *m_index_buffer{nullptr};
    BufferResource *m_vertex_buffer{nullptr};
    GraphicsStage *m_stage{nullptr};

    std::unique_ptr<wrapper::GpuTexture> m_imgui_texture;
    wrapper::Shader m_vertex_shader;
    wrapper::Shader m_fragment_shader;
    std::unique_ptr<wrapper::ResourceDescriptor> m_descriptor;
    std::vector<std::uint32_t> m_index_data;
    std::vector<ImDrawVert> m_vertex_data;

    // Neither scale nor translation change
    struct PushConstBlock {
        glm::vec2 scale{-1.0f};
        glm::vec2 translate{-1.0f};
    } m_push_const_block;

    /// This function will be called at the beginning of set_on_update
    /// The user's ImGui data will be updated in this function
    std::function<void()> m_on_update_user_data{[]() {}};

public:
    /// Default constructor
    /// @param device A reference to the device wrapper
    /// @param render_graph A pointer to the render graph
    /// @param back_buffer A pointer to the target of the ImGUI rendering
    /// @param on_update_user_data The function in which the user's ImGui data is updated
    ImGUIOverlay(
        const wrapper::Device &device, RenderGraph *render_graph, TextureResource *back_buffer,
        std::function<void()> on_update_user_data = []() {});
    ImGUIOverlay(const ImGUIOverlay &) = delete;
    ImGUIOverlay(ImGUIOverlay &&) = delete;
    ~ImGUIOverlay();

    ImGUIOverlay &operator=(const ImGUIOverlay &) = delete;
    ImGUIOverlay &operator=(ImGUIOverlay &&) = delete;
};

} // namespace inexor::vulkan_renderer
