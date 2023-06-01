#include "inexor/vulkan-renderer/imgui.hpp"

#include "inexor/vulkan-renderer/wrapper/cpu_texture.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptor_builder.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"

#include <cassert>
#include <stdexcept>
#include <utility>

namespace inexor::vulkan_renderer {

ImGUIOverlay::ImGUIOverlay(const wrapper::Device &device, RenderGraph *render_graph, TextureResource *back_buffer,
                           std::function<void()> on_update_user_data)
    : m_device(device), m_vertex_shader(m_device, VK_SHADER_STAGE_VERTEX_BIT, "ImGUI", "shaders/ui.vert.spv"),
      m_fragment_shader(m_device, VK_SHADER_STAGE_FRAGMENT_BIT, "ImGUI", "shaders/ui.frag.spv"),
      m_on_update_user_data(std::move(on_update_user_data)) {

    spdlog::trace("Creating ImGUI context");
    ImGui::CreateContext();

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

    ImGuiIO &io = ImGui::GetIO();
    io.FontGlobalScale = 1.0f;

    // Load font texture

    // TODO: Move this data into a container class; have container class also support bold and italic.
    constexpr const char *FONT_FILE_PATH = "assets/fonts/NotoSans-Bold.ttf";
    constexpr float FONT_SIZE = 18.0f;

    spdlog::trace("Loading front {}", FONT_FILE_PATH);

    ImFont *font = io.Fonts->AddFontFromFileTTF(FONT_FILE_PATH, FONT_SIZE);

    unsigned char *font_texture_data{};
    int font_texture_width{0};
    int font_texture_height{0};
    io.Fonts->GetTexDataAsRGBA32(&font_texture_data, &font_texture_width, &font_texture_height);

    if (font == nullptr || font_texture_data == nullptr) {
        spdlog::error("Unable to load font {}.  Falling back to error texture", FONT_FILE_PATH);
        m_imgui_texture = std::make_unique<wrapper::GpuTexture>(m_device, wrapper::CpuTexture());
    } else {
        spdlog::trace("Creating ImGUI font texture");

        // Our font textures always have 4 channels and a single mip level by definition.
        constexpr int FONT_TEXTURE_CHANNELS{4};
        constexpr int FONT_MIP_LEVELS{1};

        VkDeviceSize upload_size = static_cast<VkDeviceSize>(font_texture_width) *
                                   static_cast<VkDeviceSize>(font_texture_height) *
                                   static_cast<VkDeviceSize>(FONT_TEXTURE_CHANNELS);

        m_imgui_texture = std::make_unique<wrapper::GpuTexture>(
            m_device, font_texture_data, upload_size, font_texture_width, font_texture_height, FONT_TEXTURE_CHANNELS,
            FONT_MIP_LEVELS, "ImGUI font texture");
    }

    // Create an instance of the resource descriptor builder.
    // This allows us to make resource descriptors with the help of a builder pattern.
    wrapper::DescriptorBuilder descriptor_builder(m_device);

    // Make use of the builder to create a resource descriptor for the combined image sampler.
    m_descriptor = std::make_unique<wrapper::ResourceDescriptor>(
        descriptor_builder.add_combined_image_sampler(m_imgui_texture->sampler(), m_imgui_texture->image_view(), 0)
            .build("ImGUI"));

    m_index_buffer = render_graph->add<BufferResource>("ImGui", BufferUsage::INDEX_BUFFER);
    m_vertex_buffer = render_graph->add<BufferResource>("ImGui", BufferUsage::VERTEX_BUFFER);

    m_stage = render_graph->add<GraphicsStage>("ImGui");
    m_stage->add_shader(m_vertex_shader)
        ->add_shader(m_fragment_shader)
        ->add_color_blend_attachment({
            .blendEnable = VK_TRUE,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .colorBlendOp = VK_BLEND_OP_ADD,
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
            .alphaBlendOp = VK_BLEND_OP_ADD,
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                              VK_COLOR_COMPONENT_A_BIT,
        })
        ->set_vertex_input_attribute_descriptions({
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ImDrawVert, pos),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ImDrawVert, uv),
            },
            {
                .location = 2,
                .binding = 0,
                .format = VK_FORMAT_R8G8B8A8_UNORM,
                .offset = offsetof(ImDrawVert, col),
            },
        })
        ->set_vertex_input_binding_descriptions<ImDrawVert>()
        ->writes_to(back_buffer)
        ->reads_from(m_index_buffer)
        ->reads_from(m_vertex_buffer)
        ->set_on_record([&](const PhysicalStage &physical, const wrapper::CommandBuffer &cmd_buf) {
            ImDrawData *draw_data = ImGui::GetDrawData();
            if (draw_data == nullptr) {
                return;
            }
            const ImGuiIO &io = ImGui::GetIO();
            m_push_const_block.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);

            cmd_buf.bind_descriptor_sets(m_descriptor->descriptor_sets(), physical.pipeline_layout());
            cmd_buf.push_constant(physical.pipeline_layout(), m_push_const_block, VK_SHADER_STAGE_VERTEX_BIT);

            std::uint32_t index_offset = 0;
            std::int32_t vertex_offset = 0;
            for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                const ImDrawList *cmd_list = draw_data->CmdLists[i]; // NOLINT
                for (std::int32_t j = 0; j < cmd_list->CmdBuffer.Size; j++) {
                    const ImDrawCmd &draw_cmd = cmd_list->CmdBuffer[j];
                    cmd_buf.draw_indexed(draw_cmd.ElemCount, 1, index_offset, vertex_offset);
                    index_offset += draw_cmd.ElemCount;
                }
                vertex_offset += cmd_list->VtxBuffer.Size;
            }
        })
        ->set_on_update([&]() {
            // TODO: How to account for updates which are bound to a buffer, but not a stage?
            // TOOD: How to account for async updates (which do not require sync/coherency?)
            m_on_update_user_data();
            ImDrawData *draw_data = ImGui::GetDrawData();
            if (draw_data == nullptr || draw_data->TotalIdxCount == 0 || draw_data->TotalVtxCount == 0) {
                return;
            }
            if (m_index_data.size() != draw_data->TotalIdxCount) {
                m_index_data.clear();
                for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                    const ImDrawList *cmd_list = draw_data->CmdLists[i]; // NOLINT
                    for (std::size_t j = 0; j < cmd_list->IdxBuffer.Size; j++) {
                        m_index_data.push_back(cmd_list->IdxBuffer.Data[j]); // NOLINT
                    }
                }
                m_index_buffer->upload_data(m_index_data);
            }
            if (m_vertex_data.size() != draw_data->TotalVtxCount) {
                m_vertex_data.clear();
                for (std::size_t i = 0; i < draw_data->CmdListsCount; i++) {
                    const ImDrawList *cmd_list = draw_data->CmdLists[i]; // NOLINT
                    for (std::size_t j = 0; j < cmd_list->VtxBuffer.Size; j++) {
                        m_vertex_data.push_back(cmd_list->VtxBuffer.Data[j]); // NOLINT
                    }
                }
                m_vertex_buffer->upload_data(m_vertex_data);
            }
        })
        ->add_descriptor_layout(m_descriptor->descriptor_set_layout())
        ->add_push_constant_range<PushConstBlock>();
}

ImGUIOverlay::~ImGUIOverlay() {
    ImGui::DestroyContext();
}

} // namespace inexor::vulkan_renderer
