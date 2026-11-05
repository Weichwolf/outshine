#include "GeometryClusterAddresses.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Render {
std::vector<GeometryClusterAddress>
BuildClusterAddresses(std::span<const GeometryRoot> roots,
                      std::span<const GeometryClusterTemplate> templates) {
  size_t count = 0;
  for (const auto &root : roots) { count += root.TemplateCount; }
  std::vector<GeometryClusterAddress> result;
  result.reserve(count);
  for (size_t row = 0; row < roots.size(); ++row) {
    const auto &root = roots[row];
    for (const auto &one : templates.subspan(root.TemplateFirst, root.TemplateCount)) {
      result.push_back({.SourceFirst = root.IndexBase + one.RelativeFirstIndex,
                        .ModelSlot = root.ModelSlot,
                        .RootRow = static_cast<uint32_t>(row),
                        .BatchRow = root.BatchRow});
    }
  }
  return result;
}
}
