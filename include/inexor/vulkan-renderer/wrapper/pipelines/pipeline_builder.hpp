#pragma once

#include <vulkan/vulkan_core.h>

#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/pipelines/pipeline.hpp"
#include "inexor/vulkan-renderer/wrapper/shader.hpp"

#include <spdlog/spdlog.h>

#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace inexor::vulkan_renderer::wrapper {
// Forward declarations
class Device;
class Shader;
} // namespace inexor::vulkan_renderer::wrapper

namespace inexor::vulkan_renderer::render_graph {
// Forward declaration
class RenderGraph;
} // namespace inexor::vulkan_renderer::render_graph

namespace inexor::vulkan_renderer::wrapper::pipelines {

// Forward declaration
class render_graph::RenderGraph;
class wrapper::Shader;

// TODO: ComputePipelineBuilder

/// Builder class for VkPipelineCreateInfo for graphics pipelines which use dynamic rendering
/// @note This builder pattern does not perform any checks which are already covered by validation layers.
/// This means if you forget to specify viewport for example, creation of the graphics pipeline will fail.
/// It is the reponsibility of the programmer to use validation layers to check for problems.
class GraphicsPipelineBuilder {
    friend class RenderGraph;

private:
    /// The device wrapper reference
    const Device &m_device;

    // We are not using member initializers here:
    // Note that all members are initialized in the reset() method
    // This method is also called after the graphics pipeline has been created,
    // allowing one instance of GraphicsPipelineBuilder to be reused

    // With the builder we can either call add_shader or set_shaders
    std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages{};

    std::vector<VkVertexInputBindingDescription> m_vertex_input_binding_descriptions{};
    std::vector<VkVertexInputAttributeDescription> m_vertex_input_attribute_descriptions{};
    // With the builder we can fill vertex binding descriptions and vertex attribute descriptions in here
    VkPipelineVertexInputStateCreateInfo m_vertex_input_sci{};

    // With the builder we can set topology in here
    VkPipelineInputAssemblyStateCreateInfo m_input_assembly_sci{};

    // With the builder we can set the patch control point count in here
    VkPipelineTessellationStateCreateInfo m_tesselation_sci{};

    std::vector<VkViewport> m_viewports{};
    std::vector<VkRect2D> m_scissors{};
    // With the builder we can set viewport(s) and scissor(s) in here
    VkPipelineViewportStateCreateInfo m_viewport_sci{};

    // With the builder we can set polygon mode, cull mode, front face, and line width
    // TODO: Implement methods to enable depth bias and for setting the depth bias parameters
    VkPipelineRasterizationStateCreateInfo m_rasterization_sci{};

    // With the builder we can't set individial fields of this struct,
    // since it's easier to specify an entire VkPipelineDepthStencilStateCreateInfo struct to the builder instead
    VkPipelineDepthStencilStateCreateInfo m_depth_stencil_sci{};

    /// This is used for dynamic rendering
    VkFormat m_depth_attachment_format{};
    VkFormat m_stencil_attachment_format{};
    std::vector<VkFormat> m_color_attachments{};

    VkPipelineRenderingCreateInfo m_pipeline_rendering_ci{};

    // With the builder we can set rasterization samples and min sample shading
    // TODO: Expose more multisampling parameters if desired
    VkPipelineMultisampleStateCreateInfo m_multisample_sci{};

    // With the builder we can't set individial fields of this struct,
    // since it's easier to specify an entire VkPipelineColorBlendStateCreateInfo struct to the builder instead
    VkPipelineColorBlendStateCreateInfo m_color_blend_sci{};

    std::vector<VkDynamicState> m_dynamic_states{};
    // This will be filled in the build() method
    VkPipelineDynamicStateCreateInfo m_dynamic_states_sci{};

    /// The layout of the graphics pipeline
    VkPipelineLayout m_pipeline_layout{VK_NULL_HANDLE};

    // With the builder we can either call add_color_blend_attachment or set_color_blend_attachments
    std::vector<VkPipelineColorBlendAttachmentState> m_color_blend_attachment_states{};

    /// The push constant ranges of the graphics pass
    std::vector<VkPushConstantRange> m_push_constant_ranges{};

    VkDescriptorSetLayout m_descriptor_set_layout{VK_NULL_HANDLE};

    /// Reset all data in this class so the builder can be re-used
    /// @note This is called by the constructor
    void reset();

    /// Default constructor is private, so only rendergraph can access it
    /// @param device The device wrapper
    explicit GraphicsPipelineBuilder(const Device &device);

public:
    GraphicsPipelineBuilder(const GraphicsPipelineBuilder &) = delete;
    GraphicsPipelineBuilder(GraphicsPipelineBuilder &&other) noexcept;
    ~GraphicsPipelineBuilder() = default;

    GraphicsPipelineBuilder &operator=(const GraphicsPipelineBuilder &) = delete;
    GraphicsPipelineBuilder &operator=(GraphicsPipelineBuilder &&) = delete;

    /// Adds a color attachment
    /// @param format The format of the color attachment
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &add_color_attachment_format(VkFormat format);

    /// Add a color blend attachment
    /// @param attachment The color blend attachment
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    add_color_blend_attachment(const VkPipelineColorBlendAttachmentState &attachment);

    /// Add the default color blend attachment
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &add_default_color_blend_attachment();

    /// Add a push constant range to the graphics pass
    /// @param shader_stage The shader stage for the push constant range
    /// @param size The size of the push constant
    /// @param offset The offset in the push constant range
    /// @return A const reference to the this pointer (allowing method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    add_push_constant_range(VkShaderStageFlags shader_stage, std::uint32_t size, std::uint32_t offset = 0);

    /// Add a shader to the graphics pipeline
    /// @param shader The shader
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &add_shader(std::weak_ptr<Shader> shader);

    /// Build the graphics pipeline with specified pipeline create flags
    /// @param name The debug name of the graphics pipeline
    /// @return The unique pointer instance of ``GraphicsPipeline`` that was created
    [[nodiscard]] std::shared_ptr<GraphicsPipeline> build(std::string name);

    /// Set the color blend manually
    /// @param color_blend The color blend
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_color_blend(const VkPipelineColorBlendStateCreateInfo &color_blend);

    /// Set all color blend attachments manually
    /// @note You should prefer to use ``add_color_blend_attachment`` instead
    /// @param attachments The color blend attachments
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_color_blend_attachments(const std::vector<VkPipelineColorBlendAttachmentState> &attachments);

    /// Enable or disable culling
    /// @warning Disabling culling will have a significant performance impact
    /// @param culling_enabled ``true`` if culling is enabled
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_culling_mode(VkBool32 culling_enabled);

    /// Set the deptch attachment format
    /// @param format The format of the depth attachment
    /// @return A const reference to the this pointer (allowing method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_depth_attachment_format(VkFormat format);

    /// Set the descriptor set layout
    /// @param descriptor_set_layout The descriptor set layout
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_descriptor_set_layout(VkDescriptorSetLayout descriptor_set_layout);

    /// Set the depth stencil
    /// @warning Disabling culling can have performance impacts!
    /// @param depth_stencil The depth stencil
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_depth_stencil(const VkPipelineDepthStencilStateCreateInfo &depth_stencil);

    /// Set the dynamic states
    /// @param dynamic_states The dynamic states
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_dynamic_states(const std::vector<VkDynamicState> &dynamic_states);

    /// Set the stencil attachment format
    /// @param format The format of the stencil attachment
    /// @return A const reference to the this pointer (allowing method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_stencil_attachment_format(VkFormat format);

    /// Set the input assembly state create info
    /// @note If you just want to set the triangle topology, call ``set_triangle_topology`` instead, because this is the
    /// most powerful method of this method in case you really need to overwrite it
    /// @param input_assembly The pipeline input state create info
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_input_assembly(const VkPipelineInputAssemblyStateCreateInfo &input_assembly);

    /// Set the line width of rasterization
    /// @param line_width The line width used in rasterization
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_line_width(float width);

    /// Set the most important MSAA settings
    /// @param sample_count The number of samples used in rasterization
    /// @param min_sample_shading A minimum fraction of sample shading
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_multisampling(VkSampleCountFlagBits sample_count,
                                                             std::optional<float> min_sample_shading);

    /// Store the pipeline layout
    /// @param layout The pipeline layout
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_pipeline_layout(VkPipelineLayout layout);

    /// Set the triangle topology
    /// @param topology the primitive topology
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_primitive_topology(VkPrimitiveTopology topology);

    /// Set the rasterization state of the graphics pipeline manually
    /// @param rasterization The rasterization state
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_rasterization(const VkPipelineRasterizationStateCreateInfo &rasterization);

    /// Set the scissor data in VkPipelineViewportStateCreateInfo
    /// There is another method called set_scissors in case multiple scissors will be used
    /// @param scissors The scissors in in VkPipelineViewportStateCreateInfo
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_scissor(const VkRect2D &scissor);

    /// Set the scissor data in VkPipelineViewportStateCreateInfo (convert VkExtent2D to VkRect2D)
    /// @param extent The extent of the scissor
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_scissor(const VkExtent2D &extent);

    /// Set the tesselation control point count
    /// @note This is not used in the code so far, because we are not using tesselation
    /// @param control_point_count The tesselation control point count
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_tesselation_control_point_count(std::uint32_t control_point_count);

    /// Set the vertex input attribute descriptions manually
    /// @note As of C++23, there is no mechanism to do so called reflection in C++, meaning we can't get any information
    /// about the members of a struct, which would allow us to determine vertex input attributes automatically.
    /// @param descriptions The vertex input attribute descriptions
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_vertex_input_attributes(const std::vector<VkVertexInputAttributeDescription> &descriptions);

    /// Set the vertex input binding descriptions manually
    /// @param descriptions The vertex input binding descriptions
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &
    set_vertex_input_bindings(const std::vector<VkVertexInputBindingDescription> &descriptions);

    /// Set the viewport in VkPipelineViewportStateCreateInfo
    /// There is another method called set_viewports in case multiple viewports will be used
    /// @param viewport The viewport in VkPipelineViewportStateCreateInfo
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_viewport(const VkViewport &viewport);

    /// Set the viewport in VkPipelineViewportStateCreateInfo (convert VkExtent2D to VkViewport)
    /// @param extent The extent of the viewport
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_viewport(const VkExtent2D &extent);

    /// Set the wireframe mode
    /// @param wireframe ``true`` if wireframe is enabled
    /// @return A reference to the dereferenced this pointer (allows method calls to be chained)
    [[nodiscard]] GraphicsPipelineBuilder &set_wireframe(VkBool32 wireframe);
};

} // namespace inexor::vulkan_renderer::wrapper::pipelines
