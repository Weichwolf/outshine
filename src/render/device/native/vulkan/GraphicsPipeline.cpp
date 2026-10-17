#ifdef __linux__
#include "VulkanGraphics.h"
#include "VulkanImage.h"
#include <array>
#include <expected>
#include <string>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {
namespace {

VkBlendFactor Blend(BlendFactor factor) {
  switch (factor) {
    case BlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
    case BlendFactor::One: return VK_BLEND_FACTOR_ONE;
    case BlendFactor::SourceAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
    case BlendFactor::OneMinusSourceAlpha: return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  }
  return VK_BLEND_FACTOR_ZERO;
}

struct ShaderModule {
  VkDevice Device = VK_NULL_HANDLE;
  VkShaderModule Handle = VK_NULL_HANDLE;
  ShaderModule() = default;

  ~ShaderModule() {
    if (Handle != VK_NULL_HANDLE) { vkDestroyShaderModule(Device, Handle, nullptr); }
  }

  ShaderModule(const ShaderModule &) = delete;
  ShaderModule &operator=(const ShaderModule &) = delete;
};

Result<void> Module(ShaderModule &module, VkDevice device, ShaderCode source) {
  module.Device = device;
  VkShaderModuleCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  create.codeSize = source.Spirv.size_bytes();
  create.pCode = source.Spirv.data();
  const auto result = vkCreateShaderModule(device, &create, nullptr, &module.Handle);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateShaderModule", result)); }
  return {};
}

struct GraphicsSetup {
  std::vector<VkVertexInputBindingDescription> Streams;
  std::vector<VkVertexInputAttributeDescription> Attributes;
  std::vector<VkFormat> Formats;
  std::vector<VkPipelineColorBlendAttachmentState> Blends;
};

GraphicsSetup Setup(const GraphicsState &state) {
  GraphicsSetup setup;
  for (const auto &stream : state.Streams) {
    setup.Streams.push_back({.binding = stream.Slot,
                             .stride = stream.Stride,
                             .inputRate = stream.Rate == VertexRate::Vertex
                                              ? VK_VERTEX_INPUT_RATE_VERTEX
                                              : VK_VERTEX_INPUT_RATE_INSTANCE});
  }
  for (const auto &attribute : state.Attributes) {
    setup.Attributes.push_back({.location = attribute.Location,
                                .binding = attribute.Stream,
                                .format = AttributeFormat(attribute.Format),
                                .offset = attribute.Offset});
  }
  for (const auto &colour : state.Colours) {
    setup.Formats.push_back(TexelFormat(colour.Texels));
    setup.Blends.push_back({.blendEnable = colour.Blend ? VK_TRUE : VK_FALSE,
                            .srcColorBlendFactor = Blend(colour.Rgb.Source),
                            .dstColorBlendFactor = Blend(colour.Rgb.Destination),
                            .colorBlendOp = VK_BLEND_OP_ADD,
                            .srcAlphaBlendFactor = Blend(colour.Alpha.Source),
                            .dstAlphaBlendFactor = Blend(colour.Alpha.Destination),
                            .alphaBlendOp = VK_BLEND_OP_ADD,
                            .colorWriteMask = static_cast<uint32_t>(colour.Write)});
  }
  return setup;
}

}

Result<void> BuildGraphics(GraphicsState &state, const GraphicsDescription &description) {
  auto *const device = state.Pipeline->Owner->Logical;
  ShaderModule vertex;
  ShaderModule fragment;
  if (auto created = Module(vertex, device, description.Vertex); !created) { return created; }
  if (description.Fragment) {
    if (auto created = Module(fragment, device, *description.Fragment); !created) {
      return created;
    }
  }
  const std::string vertexEntry(description.Vertex.Entry);
  const std::string fragmentEntry(description.Fragment ? description.Fragment->Entry : "");
  const std::array<VkPipelineShaderStageCreateInfo, 2> stages{
      {{.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex.Handle,
        .pName = vertexEntry.c_str()},
       {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment.Handle,
        .pName = fragmentEntry.c_str()}}};
  const auto setup = Setup(state);
  VkPipelineVertexInputStateCreateInfo inputs{};
  inputs.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  inputs.vertexBindingDescriptionCount = static_cast<uint32_t>(setup.Streams.size());
  inputs.pVertexBindingDescriptions = setup.Streams.data();
  inputs.vertexAttributeDescriptionCount = static_cast<uint32_t>(setup.Attributes.size());
  inputs.pVertexAttributeDescriptions = setup.Attributes.data();
  VkPipelineInputAssemblyStateCreateInfo assembly{};
  assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo raster{};
  raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  raster.polygonMode = VK_POLYGON_MODE_FILL;
  raster.depthClampEnable = description.ClampDepth ? VK_TRUE : VK_FALSE;
  raster.cullMode = VK_CULL_MODE_NONE;
  if (description.Culling == Cull::Back) { raster.cullMode = VK_CULL_MODE_BACK_BIT; }
  if (description.Culling == Cull::Front) { raster.cullMode = VK_CULL_MODE_FRONT_BIT; }
  raster.frontFace = description.Front == Winding::CounterClockwise
                         ? VK_FRONT_FACE_COUNTER_CLOCKWISE
                         : VK_FRONT_FACE_CLOCKWISE;
  raster.lineWidth = 1;
  const VkPipelineMultisampleStateCreateInfo samples{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT};
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth.depthTestEnable =
      state.Depth && (state.Depth->Test || state.Depth->Write) ? VK_TRUE : VK_FALSE;
  depth.depthWriteEnable = state.Depth && state.Depth->Write ? VK_TRUE : VK_FALSE;
  depth.depthCompareOp = VK_COMPARE_OP_ALWAYS;
  if (state.Depth && state.Depth->Test) {
    depth.depthCompareOp = state.Depth->Compare == DepthCompare::GreaterEqual
                               ? VK_COMPARE_OP_GREATER_OR_EQUAL
                               : VK_COMPARE_OP_GREATER;
  }
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = static_cast<uint32_t>(setup.Blends.size());
  blend.pAttachments = setup.Blends.data();
  constexpr std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
  dynamic.pDynamicStates = dynamicStates.data();
  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = static_cast<uint32_t>(setup.Formats.size());
  rendering.pColorAttachmentFormats = setup.Formats.data();
  rendering.depthAttachmentFormat =
      state.Depth ? TexelFormat(state.Depth->Texels) : VK_FORMAT_UNDEFINED;
  VkGraphicsPipelineCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  create.pNext = &rendering;
  create.stageCount = description.Fragment ? 2 : 1;
  create.pStages = stages.data();
  create.pVertexInputState = &inputs;
  create.pInputAssemblyState = &assembly;
  create.pViewportState = &viewport;
  create.pRasterizationState = &raster;
  create.pMultisampleState = &samples;
  create.pDepthStencilState = &depth;
  create.pColorBlendState = &blend;
  create.pDynamicState = &dynamic;
  create.layout = state.Pipeline->Layout;
  const auto result = vkCreateGraphicsPipelines(
      device, VK_NULL_HANDLE, 1, &create, nullptr, &state.Pipeline->Handle);
  if (result != VK_SUCCESS) {
    return std::unexpected(Failure("vkCreateGraphicsPipelines", result));
  }
  return {};
}

Result<std::unique_ptr<GraphicsPipeline>> CreateGraphics(std::shared_ptr<Context> context,
                                                         const GraphicsDescription &description) {
  if (auto valid = CheckGraphics(*context, description); !valid) {
    return std::unexpected(valid.error());
  }
  auto reflected = ReflectGraphics(*context, description);
  if (!reflected) { return std::unexpected(reflected.error()); }
  auto state = std::make_shared<GraphicsState>();
  state->Pipeline = std::make_shared<PipelineState>();
  state->Pipeline->Owner = std::move(context);
  state->Pipeline->Images = std::move(reflected->Images);
  std::vector<VkShaderStageFlags> stages;
  for (const auto &binding : reflected->Layout.Bindings) {
    state->Bindings.push_back(binding.Resource);
    const auto flags = static_cast<uint32_t>(binding.Stages);
    stages.push_back(
        ((flags & static_cast<uint32_t>(ShaderStage::Vertex)) != 0 ? VK_SHADER_STAGE_VERTEX_BIT
                                                                   : 0u) |
        ((flags & static_cast<uint32_t>(ShaderStage::Fragment)) != 0 ? VK_SHADER_STAGE_FRAGMENT_BIT
                                                                     : 0u));
  }
  state->Streams.assign(description.Streams.begin(), description.Streams.end());
  state->Attributes.assign(description.Attributes.begin(), description.Attributes.end());
  state->Colours.assign(description.Colours.begin(), description.Colours.end());
  state->Depth = description.Depth;
  if (auto layouts = CreateLayouts(*state->Pipeline, state->Bindings, stages); !layouts) {
    return std::unexpected(layouts.error());
  }
  if (auto pipeline = BuildGraphics(*state, description); !pipeline) {
    return std::unexpected(pipeline.error());
  }
  return std::make_unique<VulkanGraphicsPipeline>(std::move(state), std::move(reflected->Layout));
}

}
#endif
