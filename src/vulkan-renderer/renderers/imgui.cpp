#include "inexor/vulkan-renderer/renderers/imgui.hpp"

#include "inexor/vulkan-renderer/render-graph/graphics_pass_builder.hpp"
#include "inexor/vulkan-renderer/render-graph/render_graph.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/shader.hpp"
#include "inexor/vulkan-renderer/wrapper/swapchain.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace inexor::vulkan_renderer::renderers {

ImGuiRenderer::ImGuiRenderer(const Device &device,
                             const Swapchain &swapchain,
                             render_graph::RenderGraph &render_graph,
                             std::weak_ptr<render_graph::Texture> color_attachment,
                             std::function<void()> on_update_user_data)
    : m_device(device), m_on_update_user_data(std::move(on_update_user_data)),
      m_color_attachment(std::move(color_attachment)) {

    spdlog::trace("Creating ImGui context");
    ImGui::CreateContext();

    spdlog::trace("Loading ImGui font texture");
    load_font_data_from_file();

    spdlog::trace("Setting ImGui style");
    set_imgui_style();

    using render_graph::BufferType;
    m_vertex_buffer = render_graph.add_buffer("ImGui", BufferType::VERTEX_BUFFER, [&]() {
        m_on_update_user_data();
        const ImDrawData *draw_data = ImGui::GetDrawData();
        if (draw_data == nullptr || draw_data->TotalIdxCount == 0 || draw_data->TotalVtxCount == 0) {
            return;
        }
        m_index_data.clear();
        m_vertex_data.clear();
        // We need to collect the vertices and indices generated by ImGui
        // because it does not store them in one array, but rather in chunks (command lists)
        for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[i]; // NOLINT
            for (std::size_t j = 0; j < cmd_list->IdxBuffer.Size; j++) {
                m_index_data.push_back(cmd_list->IdxBuffer.Data[j]); // NOLINT
            }
            for (std::size_t j = 0; j < cmd_list->VtxBuffer.Size; j++) {
                m_vertex_data.push_back(cmd_list->VtxBuffer.Data[j]); // NOLINT
            }
        }
        // Request rendergraph to do an update of the vertex buffer
        m_vertex_buffer.lock()->request_update(m_vertex_data);
    });

    m_index_buffer = render_graph.add_buffer("ImGui", BufferType::INDEX_BUFFER, [&]() {
        // Request rendergraph to do an update of the index buffer
        m_index_buffer.lock()->request_update(m_index_data);
    });

    m_vertex_shader =
        std::make_shared<wrapper::Shader>(m_device, "ImGui", VK_SHADER_STAGE_VERTEX_BIT, "shaders/ui.vert.spv");
    m_fragment_shader =
        std::make_shared<wrapper::Shader>(m_device, "ImGui", VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/ui.frag.spv");

    using render_graph::TextureUsage;
    m_imgui_texture = render_graph.add_texture("ImGui-Font", TextureUsage::NORMAL, VK_FORMAT_R8G8B8A8_UNORM,
                                               m_font_texture_width, m_font_texture_width, [&]() {
                                                   // Initialize the ImGui font texture
                                                   m_imgui_texture.lock()->request_update(m_font_texture_data,
                                                                                          m_font_texture_data_size);
                                               });

    using wrapper::descriptors::DescriptorSetAllocator;
    using wrapper::descriptors::DescriptorSetLayoutBuilder;
    using wrapper::descriptors::DescriptorSetUpdateBuilder;
    render_graph.add_resource_descriptor(
        [&](DescriptorSetLayoutBuilder &builder) {
            m_descriptor_set_layout = builder.add_combined_image_sampler(VK_SHADER_STAGE_FRAGMENT_BIT).build("ImGui");
        },
        [&](DescriptorSetAllocator &allocator) {
            m_descriptor_set = allocator.allocate("ImGui", m_descriptor_set_layout);
        },
        [&](DescriptorSetUpdateBuilder &builder) {
            builder.add_combined_image_sampler_update(m_descriptor_set, m_imgui_texture).update();
        });

    using wrapper::pipelines::GraphicsPipelineBuilder;
    render_graph.add_graphics_pipeline([&](GraphicsPipelineBuilder &builder) {
        m_imgui_pipeline = builder
                               .set_vertex_input_bindings({
                                   {
                                       .binding = 0,
                                       .stride = sizeof(ImDrawVert),
                                       .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
                                   },
                               })
                               .set_vertex_input_attributes({
                                   {
                                       .location = 0,
                                       .format = VK_FORMAT_R32G32_SFLOAT,
                                       .offset = offsetof(ImDrawVert, pos),
                                   },
                                   {
                                       .location = 1,
                                       .format = VK_FORMAT_R32G32_SFLOAT,
                                       .offset = offsetof(ImDrawVert, uv),
                                   },
                                   {
                                       .location = 2,
                                       .format = VK_FORMAT_R8G8B8A8_UNORM,
                                       .offset = offsetof(ImDrawVert, col),
                                   },
                               })
                               .add_default_color_blend_attachment()
                               .add_color_attachment(swapchain.image_format())
                               .set_depth_attachment_format(VK_FORMAT_D32_SFLOAT_S8_UINT)
                               .set_viewport(swapchain.extent())
                               .set_scissor(swapchain.extent())
                               .uses_shader(m_vertex_shader)
                               .uses_shader(m_fragment_shader)
                               .set_descriptor_set_layout(m_descriptor_set_layout)
                               .add_push_constant_range(VK_SHADER_STAGE_VERTEX_BIT, sizeof(m_push_const_block))
                               .build("ImGui");
    });

    using wrapper::commands::CommandBuffer;
    auto on_record_cmd_buffer = [&](const CommandBuffer &cmd_buf) {
        const ImGuiIO &io = ImGui::GetIO();
        m_push_const_block.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);

        cmd_buf.bind_pipeline(m_imgui_pipeline)
            .bind_vertex_buffer(m_vertex_buffer)
            .bind_index_buffer(m_index_buffer)
            .bind_descriptor_set(m_descriptor_set, m_imgui_pipeline)
            .push_constant(m_imgui_pipeline, m_push_const_block, VK_SHADER_STAGE_VERTEX_BIT);

        ImDrawData *draw_data = ImGui::GetDrawData();
        if (draw_data == nullptr) {
            return;
        }
        std::uint32_t index_offset = 0;
        std::int32_t vertex_offset = 0;
        for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
            const ImDrawList *cmd_list = draw_data->CmdLists[i];
            for (std::int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                const ImDrawCmd &draw_cmd = cmd_list->CmdBuffer[j];
                cmd_buf.draw_indexed(draw_cmd.ElemCount, 1, index_offset, vertex_offset);
                index_offset += draw_cmd.ElemCount;
            }
            vertex_offset += cmd_list->VtxBuffer.Size;
        }
    };

    using render_graph::GraphicsPassBuilder;
    render_graph.add_graphics_pass([&](GraphicsPassBuilder &builder) {
        return builder.add_color_attachment(m_color_attachment)
            .set_on_record(std::move(on_record_cmd_buffer))
            .build("ImGui");
    });
}

ImGuiRenderer::~ImGuiRenderer() {
    ImGui::DestroyContext();
}

void ImGuiRenderer::load_font_data_from_file() {
    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;

    // This is here because it doesn't need to be member data
    constexpr const char *FONT_FILE_PATH = "assets/fonts/NotoSans-Bold.ttf";
    constexpr float FONT_SIZE = 18.0f;

    spdlog::trace("Loading front {} with size {}", FONT_FILE_PATH, FONT_SIZE);
    ImFont *font = io.Fonts->AddFontFromFileTTF(FONT_FILE_PATH, FONT_SIZE);
    io.Fonts->GetTexDataAsRGBA32(&m_font_texture_data, &m_font_texture_width, &m_font_texture_height);

    constexpr int FONT_TEXTURE_CHANNELS = 4;
    m_font_texture_data_size = m_font_texture_width * m_font_texture_height * FONT_TEXTURE_CHANNELS;
}

void ImGuiRenderer::set_imgui_style() {
    ImGuiStyle &style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
}

} // namespace inexor::vulkan_renderer::renderers
