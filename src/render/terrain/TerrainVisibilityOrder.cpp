#include "TerrainVisibilityOrder.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>

namespace outshine::Render {

void OrderTerrainSources(std::span<TerrainVisibleSource> sources, size_t realCount) noexcept {
  for (auto &source : sources) {
    if (!std::isfinite(source.Depth)) { source.Depth = std::numeric_limits<double>::infinity(); }
  }
  std::ranges::sort(sources, [realCount](const auto &a, const auto &b) noexcept {
    const bool realA = a.Index < realCount;
    const bool realB = b.Index < realCount;
    if (realA != realB) { return realA; }
    if (a.Depth != b.Depth) { return a.Depth < b.Depth; }
    return a.Index < b.Index;
  });
}

}
