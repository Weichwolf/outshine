#include "PrismResidency.h"
#include "CasterChanges.h"
#include "BatchVisibility.h"
#include "Box.h"
#include "Mat4.h"
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

std::optional<BatchSphere> SphereOf(const Boxf &bounds) noexcept {
  std::array<double, 3> centre{};
  std::array<double, 3> half{};
  for (size_t axis = 0; axis < 3; ++axis) {
    const double low = bounds.Min[axis];
    const double high = bounds.Max[axis];
    centre[axis] = (high + low) / 2;
    half[axis] = (high - low) / 2;
  }
  return PackSphere(centre, std::hypot(half[0], half[1], half[2]));
}

std::optional<Boxf> BoundsOf(const GpuPrismPatch &record) noexcept {
  const auto local = SphereOf(record.LocalBounds());
  if (!local) { return std::nullopt; }
  Mat4 row{};
  for (size_t at = 0; at < row.Column.size(); ++at) { row[at] = record.Current[at]; }
  const auto world = TransformedSphere(*local, row);
  return world ? CasterChanges::SphereBounds(*world) : std::nullopt;
}
}

std::optional<BatchSphere>
PrismResidency::ComputeWorldSphere(std::span<const GpuPrismPatch> records) noexcept {
  Boxf world;
  for (const auto &record : records) {
    const auto bounds = BoundsOf(record);
    if (!bounds) { return std::nullopt; }
    world.Cover(*bounds);
  }
  return world.Empty() ? std::nullopt : SphereOf(world);
}

void PrismResidency::RecordShadowChanges(std::span<const GpuPrismPatch> previous,
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
  const size_t common = std::min(previous.size(), records.size());
  for (size_t at = 0; at < common; ++at) {
    if (SameDepth(previous[at], records[at])) { continue; }
    changed = true;
    include(before, previous[at]);
    include(after, records[at]);
  }
  for (size_t at = common; at < previous.size(); ++at) {
    changed = true;
    include(before, previous[at]);
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
