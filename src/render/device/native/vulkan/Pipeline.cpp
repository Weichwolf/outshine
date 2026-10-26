#ifdef __linux__
#include "Pipeline.h"
#include <span>
#include <expected>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

PipelineState::~PipelineState() {
  if (Handle != VK_NULL_HANDLE) { vkDestroyPipeline(Owner->Logical, Handle, nullptr); }
  if (Layout != VK_NULL_HANDLE) { vkDestroyPipelineLayout(Owner->Logical, Layout, nullptr); }
  for (const auto &set : Sets) {
    if (set != VK_NULL_HANDLE) { vkDestroyDescriptorSetLayout(Owner->Logical, set, nullptr); }
  }
}

VkDescriptorType DescriptorType(BindingKind kind) noexcept {
  switch (kind) {
    case BindingKind::Uniform: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case BindingKind::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case BindingKind::SampledTexture: return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    case BindingKind::StorageTexture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  }
  return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

Result<void> CheckBindings(const Context &context,
                           std::span<const ShaderBinding> bindings,
                           std::span<const ImageRequirement> imageRequirements,
                           bool perStage) {
  for (const auto &image : imageRequirements) {
    if (!context.StorageImageExtendedFormats && (image.StorageFormat == VK_FORMAT_R8_UNORM ||
                                                 image.StorageFormat == VK_FORMAT_R16G16_SFLOAT)) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "extended storage image formats unavailable"});
    }
  }
  uint32_t uniforms = 0;
  uint32_t storage = 0;
  uint32_t sampled = 0;
  uint32_t images = 0;
  for (const auto &binding : bindings) {
    if (binding.Set >= context.Properties.limits.maxBoundDescriptorSets) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "compute descriptor set exceeds device limit"});
    }
    uniforms += binding.Kind == BindingKind::Uniform ? 1u : 0u;
    storage += binding.Kind == BindingKind::StorageBuffer ? 1u : 0u;
    sampled += binding.Kind == BindingKind::SampledTexture ? 1u : 0u;
    images += binding.Kind == BindingKind::StorageTexture ? 1u : 0u;
  }
  const auto &limits = context.Properties.limits;
  const auto stageLimit = [perStage](uint32_t value) {
    return perStage ? value : std::numeric_limits<uint32_t>::max();
  };
  if (uniforms > std::min(stageLimit(limits.maxPerStageDescriptorUniformBuffers),
                          limits.maxDescriptorSetUniformBuffers) ||
      storage > std::min(stageLimit(limits.maxPerStageDescriptorStorageBuffers),
                         limits.maxDescriptorSetStorageBuffers) ||
      sampled > std::min({stageLimit(limits.maxPerStageDescriptorSampledImages),
                          limits.maxDescriptorSetSampledImages,
                          stageLimit(limits.maxPerStageDescriptorSamplers),
                          limits.maxDescriptorSetSamplers}) ||
      images > std::min(stageLimit(limits.maxPerStageDescriptorStorageImages),
                        limits.maxDescriptorSetStorageImages) ||
      bindings.size() > stageLimit(limits.maxPerStageResources)) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "compute descriptor count exceeds device limit"});
  }
  return {};
}

Result<void> CreateLayouts(PipelineState &state,
                           std::span<const ShaderBinding> resources,
                           std::span<const VkShaderStageFlags> stages) {
  uint32_t setCount = 0;
  for (const auto &binding : resources) { setCount = std::max(setCount, binding.Set + 1); }
  state.Sets.resize(setCount, VK_NULL_HANDLE);
  for (uint32_t at = 0; at < setCount; ++at) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (std::size_t index = 0; index < resources.size(); ++index) {
      const auto &binding = resources[index];
      if (binding.Set != at) { continue; }
      VkDescriptorSetLayoutBinding into{};
      into.binding = binding.Slot;
      into.descriptorType = DescriptorType(binding.Kind);
      into.descriptorCount = 1;
      into.stageFlags = stages.empty() ? VK_SHADER_STAGE_COMPUTE_BIT : stages[index];
      bindings.push_back(into);
    }
    VkDescriptorSetLayoutCreateInfo create{};
    create.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    create.bindingCount = static_cast<uint32_t>(bindings.size());
    create.pBindings = bindings.data();
    const auto result =
        vkCreateDescriptorSetLayout(state.Owner->Logical, &create, nullptr, &state.Sets[at]);
    if (result != VK_SUCCESS) {
      return std::unexpected(Failure("vkCreateDescriptorSetLayout", result));
    }
  }
  VkPipelineLayoutCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  create.setLayoutCount = setCount;
  create.pSetLayouts = state.Sets.data();
  const auto result = vkCreatePipelineLayout(state.Owner->Logical, &create, nullptr, &state.Layout);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreatePipelineLayout", result)); }
  return {};
}

Result<std::unique_ptr<ComputePipeline>> CreatePipeline(std::shared_ptr<Context> context,
                                                        std::span<const uint32_t> spirv,
                                                        std::string_view entry) {
  auto reflected = ReflectCompute(spirv, entry);
  if (!reflected) { return std::unexpected(reflected.error()); }
  const auto &layout = reflected->Layout;
  uint64_t lanes = 1;
  for (std::size_t at = 0; at < layout.LocalSize.size(); ++at) {
    if (layout.LocalSize[at] > context->Limits.MaxWorkgroupSize[at]) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "compute local size exceeds device limit"});
    }
    lanes *= layout.LocalSize[at];
  }
  if (lanes > context->Limits.MaxWorkgroupInvocations) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "compute local invocations exceed device limit"});
  }
  if (auto checked = CheckBindings(*context, reflected->Layout.Bindings, reflected->Images);
      !checked) {
    return std::unexpected(checked.error());
  }
  auto state = std::make_shared<PipelineState>();
  state->Owner = std::move(context);
  state->Images = std::move(reflected->Images);
  if (auto result = CreateLayouts(*state, reflected->Layout.Bindings); !result) {
    return std::unexpected(result.error());
  }
  VkShaderModuleCreateInfo module{};
  module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  module.codeSize = spirv.size_bytes();
  module.pCode = spirv.data();
  VkShaderModule shader = VK_NULL_HANDLE;
  auto result = vkCreateShaderModule(state->Owner->Logical, &module, nullptr, &shader);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateShaderModule", result)); }
  const std::string name(entry);
  const VkComputePipelineCreateInfo create{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = shader,
                .pName = name.c_str(),
                .pSpecializationInfo = nullptr},
      .layout = state->Layout,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = 0};
  result = vkCreateComputePipelines(
      state->Owner->Logical, VK_NULL_HANDLE, 1, &create, nullptr, &state->Handle);
  vkDestroyShaderModule(state->Owner->Logical, shader, nullptr);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateComputePipelines", result)); }
  return std::make_unique<VulkanPipeline>(std::move(state), std::move(reflected->Layout));
}

}
#endif
