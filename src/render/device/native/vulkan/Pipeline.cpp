#ifdef __linux__
#include "Pipeline.h"
#include <span>
#include <expected>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
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

namespace {

Result<void> CheckLayout(const Context &context, const ComputeReflection &reflection) {
  const auto &layout = reflection.Layout;
  for (const auto &image : reflection.Images) {
    if (!context.StorageImageExtendedFormats && (image.StorageFormat == VK_FORMAT_R8_UNORM ||
                                                 image.StorageFormat == VK_FORMAT_R16G16_SFLOAT)) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "extended storage image formats unavailable"});
    }
  }
  uint64_t lanes = 1;
  for (std::size_t at = 0; at < layout.LocalSize.size(); ++at) {
    if (layout.LocalSize[at] > context.Limits.MaxWorkgroupSize[at]) {
      return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                   .Message = "compute local size exceeds device limit"});
    }
    lanes *= layout.LocalSize[at];
  }
  if (lanes > context.Limits.MaxWorkgroupInvocations) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "compute local invocations exceed device limit"});
  }
  uint32_t uniforms = 0;
  uint32_t storage = 0;
  uint32_t sampled = 0;
  uint32_t images = 0;
  for (const auto &binding : layout.Bindings) {
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
  if (uniforms > std::min(limits.maxPerStageDescriptorUniformBuffers,
                          limits.maxDescriptorSetUniformBuffers) ||
      storage > std::min(limits.maxPerStageDescriptorStorageBuffers,
                         limits.maxDescriptorSetStorageBuffers) ||
      sampled > std::min({limits.maxPerStageDescriptorSampledImages,
                          limits.maxDescriptorSetSampledImages,
                          limits.maxPerStageDescriptorSamplers,
                          limits.maxDescriptorSetSamplers}) ||
      images > std::min(limits.maxPerStageDescriptorStorageImages,
                        limits.maxDescriptorSetStorageImages) ||
      layout.Bindings.size() > limits.maxPerStageResources) {
    return std::unexpected(Error{.Kind = ErrorKind::Unsupported,
                                 .Message = "compute descriptor count exceeds device limit"});
  }
  return {};
}

Result<void> CreateLayouts(PipelineState &state, const ComputeLayout &layout) {
  uint32_t setCount = 0;
  for (const auto &binding : layout.Bindings) { setCount = std::max(setCount, binding.Set + 1); }
  state.Sets.resize(setCount, VK_NULL_HANDLE);
  for (uint32_t at = 0; at < setCount; ++at) {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    for (const auto &binding : layout.Bindings) {
      if (binding.Set != at) { continue; }
      VkDescriptorSetLayoutBinding into{};
      into.binding = binding.Slot;
      into.descriptorType = DescriptorType(binding.Kind);
      into.descriptorCount = 1;
      into.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
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

}

Result<std::unique_ptr<ComputePipeline>> CreatePipeline(std::shared_ptr<Context> context,
                                                        std::span<const uint32_t> spirv,
                                                        std::string_view entry) {
  auto reflected = ReflectCompute(spirv, entry);
  if (!reflected) { return std::unexpected(reflected.error()); }
  if (auto checked = CheckLayout(*context, *reflected); !checked) {
    return std::unexpected(checked.error());
  }
  auto state = std::make_shared<PipelineState>();
  state->Owner = std::move(context);
  state->Images = std::move(reflected->Images);
  if (auto result = CreateLayouts(*state, reflected->Layout); !result) {
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
  VkComputePipelineCreateInfo create{};
  create.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  create.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  create.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  create.stage.module = shader;
  create.stage.pName = name.c_str();
  create.layout = state->Layout;
  result = vkCreateComputePipelines(
      state->Owner->Logical, VK_NULL_HANDLE, 1, &create, nullptr, &state->Handle);
  vkDestroyShaderModule(state->Owner->Logical, shader, nullptr);
  if (result != VK_SUCCESS) { return std::unexpected(Failure("vkCreateComputePipelines", result)); }
  return std::make_unique<VulkanPipeline>(std::move(state), std::move(reflected->Layout));
}

}
#endif
