#ifdef __linux__
#include "Recording.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <variant>
#include <vector>

namespace outshine::Render::Native::Vulkan {

Result<VkDescriptorBufferInfo> Recording::BindBuffer(const ShaderBinding &declared,
                                                     const Binding &binding,
                                                     VkPipelineStageFlags2 stages) {
  std::shared_ptr<BufferState> data;
  ByteRange range;
  VkAccessFlags2 access = VK_ACCESS_2_SHADER_READ_BIT;
  if (declared.Kind == BindingKind::Uniform) {
    const auto *const uniform = std::get_if<Uniform>(&binding.Value);
    if (uniform == nullptr || uniform->Bytes.empty() ||
        uniform->Bytes.size() > Context_->Limits.MaxUniformBufferBytes ||
        uniform->Bytes.size() < declared.MinimumBytes) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "invalid shader uniform bytes"});
    }
    auto allocated = Storage_.UploadUniform(uniform->Bytes);
    if (!allocated) { return std::unexpected(allocated.error()); }
    data = allocated->Buffer;
    range = allocated->Range;
    access = VK_ACCESS_2_UNIFORM_READ_BIT;
  } else {
    const auto *const storage = std::get_if<StorageBuffer>(&binding.Value);
    if (storage == nullptr || storage->Data == nullptr) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "missing shader storage buffer"});
    }
    const auto held = BufferFrom(*storage->Data, *Context_);
    if (!held) { return std::unexpected(held.error()); }
    data = *held;
    range = storage->Range;
    const bool storageUse = (static_cast<uint32_t>(data->Description.Use) &
                             static_cast<uint32_t>(BufferUse::Storage)) != 0;
    if (!storageUse || !Contains(data->Description.Bytes, range) ||
        range.Bytes < declared.MinimumBytes ||
        range.Bytes > Context_->Limits.MaxStorageBufferBytes ||
        range.Offset % Context_->Limits.StorageBufferAlignment != 0) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "invalid shader storage buffer range or alignment"});
    }
    if (declared.Usage != Access::Read) { access |= VK_ACCESS_2_SHADER_WRITE_BIT; }
    if (declared.Usage == Access::Write) { access = VK_ACCESS_2_SHADER_WRITE_BIT; }
  }
  if (declared.Kind != BindingKind::Uniform) {
    if (auto held = Hold(data); !held) { return std::unexpected(held.error()); }
  }
  Use(*data, range, {.Stage = stages, .Access = access});
  return VkDescriptorBufferInfo{
      .buffer = data->Handle, .offset = range.Offset, .range = range.Bytes};
}

Result<std::vector<VkDescriptorSet>> Recording::BindResources(const PipelineState &pipeline,
                                                              std::span<const ShaderBinding> layout,
                                                              std::span<const Binding> bindings,
                                                              VkPipelineStageFlags2 stages) {
  if (bindings.size() != layout.size()) {
    return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                 .Message = "shader binding count does not match reflection"});
  }
  std::vector<DescriptorResource> resources;
  resources.reserve(layout.size());
  for (const auto &declared : layout) {
    const auto bound = std::ranges::find_if(bindings, [&declared](const Binding &binding) {
      return declared.Set == binding.Set && declared.Slot == binding.Slot;
    });
    if (bound == bindings.end()) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Invalid, .Message = "shader reflected set/slot is not bound"});
    }
    DescriptorResource resource{
        .Set = declared.Set, .Slot = declared.Slot, .Type = DescriptorType(declared.Kind)};
    if (declared.Kind == BindingKind::Uniform || declared.Kind == BindingKind::StorageBuffer) {
      const auto info = BindBuffer(declared, *bound, stages);
      if (!info) { return std::unexpected(info.error()); }
      resource.Buffer = *info;
    } else {
      const auto info = BindImage(declared, *bound, pipeline, stages);
      if (!info) { return std::unexpected(info.error()); }
      resource.Image = *info;
    }
    resources.push_back(resource);
  }
  std::vector<VkDescriptorSet> sets(pipeline.Sets.size());
  size_t first = 0;
  for (size_t set = 0; set < pipeline.Sets.size(); ++set) {
    size_t end = first;
    while (end < resources.size() && resources[end].Set == set) { ++end; }
    auto bound = Storage_.DescriptorSet(pipeline.Sets[set],
                                        std::span(resources).subspan(first, end - first));
    if (!bound) { return std::unexpected(bound.error()); }
    sets[set] = *bound;
    first = end;
  }
  return sets;
}

}
#endif
