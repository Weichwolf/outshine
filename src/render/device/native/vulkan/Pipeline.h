#ifndef OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_PIPELINE_H
#define OUTSHINE_RENDER_DEVICE_NATIVE_VULKAN_PIPELINE_H

#ifdef __linux__
#include "Context.h"
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render::Native::Vulkan {

struct ImageRequirement {
  uint32_t Set = 0;
  uint32_t Slot = 0;
  bool Array = false;
  VkFormat StorageFormat = VK_FORMAT_UNDEFINED;
};

struct ComputeReflection {
  ComputeLayout Layout;
  std::vector<ImageRequirement> Images;
};

[[nodiscard]] VkDescriptorType DescriptorType(BindingKind kind) noexcept;

struct PipelineState {
  std::shared_ptr<Context> Owner;
  VkPipeline Handle = VK_NULL_HANDLE;
  VkPipelineLayout Layout = VK_NULL_HANDLE;
  std::vector<VkDescriptorSetLayout> Sets;
  std::vector<ImageRequirement> Images;

  PipelineState() = default;
  ~PipelineState();
  PipelineState(const PipelineState &) = delete;
  PipelineState &operator=(const PipelineState &) = delete;
};

class VulkanPipeline final : public ComputePipeline {
public:
  VulkanPipeline(std::shared_ptr<PipelineState> state, ComputeLayout layout)
      : ComputePipeline(std::move(layout)), State_(std::move(state)) {}

  [[nodiscard]] const std::shared_ptr<PipelineState> &State() const noexcept { return State_; }

private:
  std::shared_ptr<PipelineState> State_;
};

[[nodiscard]] Result<ComputeReflection> ReflectCompute(std::span<const uint32_t> spirv,
                                                       std::string_view entry);
[[nodiscard]] Result<std::unique_ptr<ComputePipeline>> CreatePipeline(
    std::shared_ptr<Context> context, std::span<const uint32_t> spirv, std::string_view entry);

}
#endif
#endif
