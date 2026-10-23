#include "PrismRenderer.h"
#include "CasterChanges.h"
#include "BatchVisibility.h"
#include "math/Box.h"
#include "math/Mat4.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <bit>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine::Render {
namespace {
template <size_t Size>
bool SameBits(const std::array<float, Size> &before,
              const std::array<float, Size> &after) noexcept {
  return std::bit_cast<std::array<uint32_t, Size>>(before) ==
         std::bit_cast<std::array<uint32_t, Size>>(after);
}

bool SameDepth(const GpuPrismPatch &before, const GpuPrismPatch &after) noexcept {
  return SameBits(before.Current.Column, after.Current.Column) && before.Samples == after.Samples &&
         SameBits(before.Shape, after.Shape) && SameBits(before.Bounds, after.Bounds);
}

std::optional<Boxf> BoundsOf(const GpuPrismPatch &record) noexcept {
  const double halfWidth = static_cast<double>(record.Bounds[1]) / 2;
  const double halfDepth = static_cast<double>(record.Bounds[2]) / 2;
  const double middle = (static_cast<double>(record.Shape[3]) + record.Bounds[0]) / 2;
  const double halfHeight = (static_cast<double>(record.Bounds[0]) - record.Shape[3]) / 2;
  const auto local =
      PackSphere({halfWidth, middle, halfDepth}, std::hypot(halfWidth, halfHeight, halfDepth));
  if (!local) { return std::nullopt; }
  Mat4 row{};
  for (size_t at = 0; at < row.Column.size(); ++at) { row[at] = record.Current[at]; }
  const auto world = TransformedSphere(*local, row);
  return world ? CasterChanges::SphereBounds(*world) : std::nullopt;
}
}

void PrismRenderer::RecordShadowChanges(const PrismProduct &product,
                                        std::span<const GpuPrismPatch> records) noexcept {
  Boxf before;
  Boxf after;
  bool changed = false;
  bool known = true;
  const auto include = [&known](Boxf &into, const GpuPrismPatch &record) {
    const auto bounds = BoundsOf(record);
    if (bounds) {
      into.Cover(*bounds);
    } else {
      known = false;
    }
  };
  const size_t common = std::min(product.Records.size(), records.size());
  for (size_t at = 0; at < common; ++at) {
    if (SameDepth(product.Records[at], records[at])) { continue; }
    changed = true;
    include(before, product.Records[at]);
    include(after, records[at]);
  }
  for (size_t at = common; at < product.Records.size(); ++at) {
    changed = true;
    include(before, product.Records[at]);
  }
  for (size_t at = common; at < records.size(); ++at) {
    changed = true;
    include(after, records[at]);
  }
  if (!changed) { return; }
  if (!known) {
    ShadowChanges_.Unknown();
    return;
  }
  ShadowChanges_.Include(before.Empty() ? std::optional<Boxf>{} : before,
                         after.Empty() ? std::optional<Boxf>{} : after);
}
}
