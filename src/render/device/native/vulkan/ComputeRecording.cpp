#ifdef __linux__
#include "Recording.h"
#include <expected>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>
#include <vector>

namespace outshine::Render::Native::Vulkan {

namespace {

struct ImageUse {
  TextureView View;
  bool Writes = false;
};

Result<void> CheckImageAliases(const ComputeLayout &layout, std::span<const Binding> bindings) {
  std::vector<ImageUse> images;
  for (const auto &declared : layout.Bindings) {
    const auto bound = std::ranges::find_if(bindings, [&declared](const Binding &binding) {
      return declared.Set == binding.Set && declared.Slot == binding.Slot;
    });
    if (bound == bindings.end()) { continue; }
    if (const auto *sampled = std::get_if<SampledTexture>(&bound->Value)) {
      images.push_back({.View = sampled->Image});
    }
    if (const auto *storage = std::get_if<StorageTexture>(&bound->Value)) {
      images.push_back({.View = storage->Image, .Writes = declared.Usage != Access::Read});
    }
  }
  for (std::size_t left = 0; left < images.size(); ++left) {
    const auto &first = images[left];
    for (std::size_t right = left + 1; right < images.size(); ++right) {
      const auto &second = images[right];
      const auto intersects = [](uint32_t a, uint32_t sizeA, uint32_t b, uint32_t sizeB) {
        return uint64_t(a) < uint64_t(b) + sizeB && uint64_t(b) < uint64_t(a) + sizeA;
      };
      if (first.View.Image != nullptr && first.View.Image == second.View.Image &&
          (first.Writes || second.Writes) &&
          intersects(
              first.View.FirstMip, first.View.Mips, second.View.FirstMip, second.View.Mips) &&
          intersects(first.View.FirstLayer,
                     first.View.Layers,
                     second.View.FirstLayer,
                     second.View.Layers)) {
        return std::unexpected(
            Error{.Kind = ErrorKind::Invalid, .Message = "overlapping writable image bindings"});
      }
    }
  }
  return {};
}

}

Result<void> Recording::Dispatch(const ComputePipeline &pipeline,
                                 std::span<const Binding> bindings,
                                 Workgroups groups) {
  if (auto owner = CheckOwner(*Context_); !owner) { return owner; }
  const auto *const native = dynamic_cast<const VulkanPipeline *>(&pipeline);
  if (native == nullptr || native->State()->Owner != Context_ || Finished_ || Render_) {
    return std::unexpected(
        Error{.Kind = ErrorKind::Invalid, .Message = "invalid compute pipeline or command state"});
  }
  const std::array counts{groups.X, groups.Y, groups.Z};
  for (std::size_t at = 0; at < counts.size(); ++at) {
    if (counts[at] == 0 || counts[at] > Context_->Limits.MaxWorkgroupCount[at]) {
      return std::unexpected(Error{.Kind = ErrorKind::Invalid,
                                   .Message = "compute dispatch exceeds device workgroup limit"});
    }
  }
  if (auto valid = CheckImageAliases(native->Layout(), bindings); !valid) { return valid; }
  const auto sets = BindResources(*native->State(),
                                  native->Layout().Bindings,
                                  bindings,
                                  VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT);
  if (!sets) { return std::unexpected(sets.error()); }
  if (std::ranges::find(Pipelines_, native->State()) == Pipelines_.end()) {
    Pipelines_.push_back(native->State());
  }
  vkCmdBindPipeline(Commands_, VK_PIPELINE_BIND_POINT_COMPUTE, native->State()->Handle);
  if (!sets->empty()) {
    vkCmdBindDescriptorSets(Commands_,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            native->State()->Layout,
                            0,
                            static_cast<uint32_t>(sets->size()),
                            sets->data(),
                            0,
                            nullptr);
  }
  const auto query = Timings_.Begin(Commands_);
  if (!query) { return std::unexpected(query.error()); }
  vkCmdDispatch(Commands_, groups.X, groups.Y, groups.Z);
  Timings_.End(Commands_, *query);
  return {};
}

}
#endif
