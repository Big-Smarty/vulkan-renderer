#include "inexor/vulkan-renderer/wrapper/pipeline.hpp"

#include "inexor/vulkan-renderer/exception.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"

#include <utility>

namespace inexor::vulkan_renderer::wrapper {

GraphicsPipeline::GraphicsPipeline(const Device &device, const VkGraphicsPipelineCreateInfo &pipeline_ci,
                                   std::string name)
    : m_device(device), m_name(std::move(name)) {
    if (const auto result =
            vkCreateGraphicsPipelines(m_device.device(), nullptr, 1, &pipeline_ci, nullptr, &m_pipeline);
        result != VK_SUCCESS) {
        throw VulkanException("Error: vkCreateGraphicsPipelines failed for pipeline " + m_name + " !", result);
    }
    m_device.set_debug_marker_name(&m_pipeline, VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT, m_name);
}

GraphicsPipeline::GraphicsPipeline(GraphicsPipeline &&other) noexcept : m_device(other.m_device) {
    m_pipeline = std::exchange(other.m_pipeline, nullptr);
    m_name = std::move(other.m_name);
}

GraphicsPipeline::~GraphicsPipeline() {
    vkDestroyPipeline(m_device.device(), m_pipeline, nullptr);
}

} // namespace inexor::vulkan_renderer::wrapper
