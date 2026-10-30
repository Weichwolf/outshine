#ifdef __linux__
#include "VulkanGraphics.h"
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <utility>

namespace outshine::Render::Native::Vulkan {
std::optional<GraphicsPrograms::Found> GraphicsPrograms::Find(const GraphicsRecipe &recipe) {
  const auto entry = Entries_.find(recipe);
  if (entry != Entries_.end()) {
    if (auto pipeline = entry->second.Pipeline.lock()) {
      return Found{.Pipeline = std::move(pipeline), .Layout = entry->second.Layout};
    }
  }
  std::erase_if(Entries_, [](const auto &value) { return value.second.Pipeline.expired(); });
  return std::nullopt;
}

void GraphicsPrograms::Remember(GraphicsRecipe recipe,
                                const std::shared_ptr<PipelineState> &pipeline,
                                const GraphicsLayout &layout) {
  Entries_.insert_or_assign(std::move(recipe), Entry{.Pipeline = pipeline, .Layout = layout});
}

size_t GraphicsPrograms::RecipeCapacityBytes() const noexcept {
  size_t bytes = 0;
  for (const auto &entry : Entries_) { bytes += entry.first.Bytes.capacity(); }
  return bytes;
}
}
#endif
