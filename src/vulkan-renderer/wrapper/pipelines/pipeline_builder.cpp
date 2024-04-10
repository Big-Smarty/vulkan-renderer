#include "inexor/vulkan-renderer/wrapper/pipelines/pipeline_builder.hpp"

#include "inexor/vulkan-renderer/wrapper/device.hpp"

#include <utility>

namespace inexor::vulkan_renderer::wrapper::pipelines {

GraphicsPipelineBuilder::GraphicsPipelineBuilder(const Device &device) : m_device(device) {
    reset();
}

GraphicsPipelineBuilder::GraphicsPipelineBuilder(GraphicsPipelineBuilder &&other) noexcept : m_device(other.m_device) {
    m_depth_attachment_format = other.m_depth_attachment_format;
    m_stencil_attachment_format = other.m_stencil_attachment_format;
    m_swapchain_img_format = other.m_swapchain_img_format;
    m_pipeline_rendering_ci = std::move(other.m_pipeline_rendering_ci);
    m_vertex_input_sci = std::move(other.m_vertex_input_sci);
    m_input_assembly_sci = std::move(other.m_input_assembly_sci);
    m_tesselation_sci = std::move(other.m_tesselation_sci);
    m_viewport_sci = std::move(other.m_viewport_sci);
    m_rasterization_sci = std::move(m_rasterization_sci);
    m_multisample_sci = std::move(other.m_multisample_sci);
    m_depth_stencil_sci = std::move(other.m_depth_stencil_sci);
    m_color_blend_sci = std::move(other.m_color_blend_sci);
    m_dynamic_states_sci = std::move(other.m_dynamic_states_sci);
    m_pipeline_layout = std::exchange(other.m_pipeline_layout, VK_NULL_HANDLE);
    m_dynamic_states = std::move(other.m_dynamic_states);
    m_viewports = std::move(other.m_viewports);
    m_scissors = std::move(other.m_scissors);
    m_shader_stages = std::move(other.m_shader_stages);
    m_vertex_input_binding_descriptions = std::move(other.m_vertex_input_binding_descriptions);
    m_vertex_input_attribute_descriptions = std::move(other.m_vertex_input_attribute_descriptions);
    m_color_blend_attachment_states = std::move(other.m_color_blend_attachment_states);
}

std::unique_ptr<GraphicsPipeline> GraphicsPipelineBuilder::build(std::string name) {
    assert(!name.empty());
    assert(!m_vertex_input_binding_descriptions.empty());
    assert(!m_vertex_input_attribute_descriptions.empty());

    // We don't really need all the make_infos here, as we initialized it all in reset() already,
    // but it makes the code look cleaner and more consistent
    m_vertex_input_sci = make_info<VkPipelineVertexInputStateCreateInfo>({
        .vertexBindingDescriptionCount = static_cast<std::uint32_t>(m_vertex_input_binding_descriptions.size()),
        .pVertexBindingDescriptions = m_vertex_input_binding_descriptions.data(),
        .vertexAttributeDescriptionCount = static_cast<std::uint32_t>(m_vertex_input_attribute_descriptions.size()),
        .pVertexAttributeDescriptions = m_vertex_input_attribute_descriptions.data(),

    });

    assert(!m_viewports.empty());
    assert(!m_scissors.empty());

    m_viewport_sci = make_info<VkPipelineViewportStateCreateInfo>({
        .viewportCount = static_cast<uint32_t>(m_viewports.size()),
        .pViewports = m_viewports.data(),
        .scissorCount = static_cast<uint32_t>(m_scissors.size()),
        .pScissors = m_scissors.data(),
    });

    if (!m_dynamic_states.empty()) {
        m_dynamic_states_sci = make_info<VkPipelineDynamicStateCreateInfo>({
            .dynamicStateCount = static_cast<std::uint32_t>(m_dynamic_states.size()),
            .pDynamicStates = m_dynamic_states.data(),
        });
    }

    m_pipeline_rendering_ci = make_info<VkPipelineRenderingCreateInfo>({
        // The pNext chain ends here!
        .pNext = nullptr,
        // TODO: Implement more than one color attachment in the future if required
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &m_swapchain_img_format,
        .depthAttachmentFormat = m_depth_attachment_format,
        .stencilAttachmentFormat = m_stencil_attachment_format,
    });

    assert(m_pipeline_layout);

    auto new_graphics_pipeline =
        std::make_unique<GraphicsPipeline>(m_device,
                                           make_info<VkGraphicsPipelineCreateInfo>({
                                               // This is one of those rare cases where pNext is actually not nullptr!
                                               .pNext = &m_pipeline_rendering_ci, // We use dynamic rendering
                                               .stageCount = static_cast<std::uint32_t>(m_shader_stages.size()),
                                               .pStages = m_shader_stages.data(),
                                               .pVertexInputState = &m_vertex_input_sci,
                                               .pInputAssemblyState = &m_input_assembly_sci,
                                               .pTessellationState = &m_tesselation_sci,
                                               .pViewportState = &m_viewport_sci,
                                               .pRasterizationState = &m_rasterization_sci,
                                               .pMultisampleState = &m_multisample_sci,
                                               .pDepthStencilState = &m_depth_stencil_sci,
                                               .pColorBlendState = &m_color_blend_sci,
                                               .pDynamicState = &m_dynamic_states_sci,
                                               .layout = m_pipeline_layout,
                                               .renderPass = VK_NULL_HANDLE, // We use dynamic rendering
                                           }),
                                           std::move(name));

    // Reset the builder's data after creating the graphics pipeline
    reset();

    // We must std::move the return value because it is a std::unique_ptr
    return std::move(new_graphics_pipeline);
}

void GraphicsPipelineBuilder::reset() {
    m_swapchain_img_format = VK_FORMAT_UNDEFINED;
    m_depth_attachment_format = VK_FORMAT_UNDEFINED;
    m_stencil_attachment_format = VK_FORMAT_UNDEFINED;
    m_pipeline_layout = VK_NULL_HANDLE;

    m_vertex_input_binding_descriptions.clear();
    m_vertex_input_attribute_descriptions.clear();
    m_vertex_input_sci = {
        make_info<VkPipelineVertexInputStateCreateInfo>(),
    };

    m_input_assembly_sci = {
        make_info<VkPipelineInputAssemblyStateCreateInfo>({
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
            .primitiveRestartEnable = VK_FALSE,
        }),
    };

    m_tesselation_sci = {
        make_info<VkPipelineTessellationStateCreateInfo>(),
    };

    m_viewports.clear();
    m_scissors.clear();
    m_viewport_sci = {
        make_info<VkPipelineViewportStateCreateInfo>(),
    };

    m_rasterization_sci = {
        make_info<VkPipelineRasterizationStateCreateInfo>({
            .polygonMode = VK_POLYGON_MODE_FILL,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_CLOCKWISE,
            .lineWidth = 1.0f,
        }),
    };

    m_multisample_sci = {
        make_info<VkPipelineMultisampleStateCreateInfo>({
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 1.0f,
        }),
    };

    m_depth_stencil_sci = {
        make_info<VkPipelineDepthStencilStateCreateInfo>(),
    };

    m_color_blend_sci = {
        make_info<VkPipelineColorBlendStateCreateInfo>(),
    };

    m_dynamic_states.clear();
    m_dynamic_states_sci = {
        make_info<VkPipelineDynamicStateCreateInfo>(),
    };
}

} // namespace inexor::vulkan_renderer::wrapper::pipelines
