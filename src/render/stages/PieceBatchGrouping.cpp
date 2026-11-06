#include "GeometryRenderer.h"
#include "BatchVisibility.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace outshine::Render {
namespace {
std::optional<BatchSphere> Joined(std::optional<BatchSphere> first,
                                  std::optional<BatchSphere> second) {
  if (!first || !second) { return std::nullopt; }
  const auto &a = *first;
  const auto &b = *second;
  std::array<double, 3> delta;
  for (size_t axis = 0; axis < 3; ++axis) { delta[axis] = double{b[axis]} - a[axis]; }
  const double distance = std::nextafter(std::hypot(delta[0], delta[1], delta[2]),
                                         std::numeric_limits<double>::infinity());
  if (distance + b[3] <= a[3]) { return a; }
  if (distance + a[3] <= b[3]) { return b; }
  const double radius = (distance + a[3] + b[3]) * .5;
  const double fraction = (radius - a[3]) / distance;
  std::array<double, 3> centre;
  for (size_t axis = 0; axis < 3; ++axis) { centre[axis] = a[axis] + delta[axis] * fraction; }
  const double coverA = std::hypot(centre[0] - a[0], centre[1] - a[1], centre[2] - a[2]) + a[3];
  const double coverB = std::hypot(centre[0] - b[0], centre[1] - b[1], centre[2] - b[2]) + b[3];
  return PackSphere(centre, std::max(coverA, coverB));
}
}

void GeometryRenderer::MergeLastPieceBatch(Piece &one) {
  if (!one.Placement || one.Rows.size() != 1 || one.Clusters.empty() || !one.FirstBatch ||
      *one.FirstBatch + 1 != Batches.size() || Batches.size() < 2 ||
      TableRoots_.size() != Batches.size()) {
    return;
  }
  const size_t first = Batches.size() - 2;
  auto &previous = Batches[first];
  const auto &next = Batches.back();
  if ((previous.Kind != SurfaceKind::Opaque && previous.Kind != SurfaceKind::Masked) ||
      previous.MaterialSlot != next.MaterialSlot || previous.Layout != next.Layout ||
      previous.Passes != next.Passes || previous.ModelSlot != next.ModelSlot ||
      previous.Motion != next.Motion || previous.Instances != 1 || next.Instances != 1 ||
      previous.JobCount == 0 || next.JobCount == 0 ||
      uint64_t{previous.FirstIndex} + previous.IndexCount != next.FirstIndex ||
      uint64_t{previous.FirstJob} + previous.JobCount != next.FirstJob ||
      uint64_t{previous.IndexCount} + next.IndexCount > std::numeric_limits<uint32_t>::max()) {
    return;
  }
  auto &root = TableRoots_[first];
  const auto &last = TableRoots_.back();
  if (uint64_t{root.IndexCapacity} + last.IndexCapacity > std::numeric_limits<uint32_t>::max()) {
    return;
  }
  const auto local = Joined(
      (root.Flags & kGeometryRootBound) != 0 ? std::optional(root.LocalSphere) : std::nullopt,
      (last.Flags & kGeometryRootBound) != 0 ? std::optional(last.LocalSphere) : std::nullopt);
  root.LocalSphere = local.value_or(BatchSphere{});
  if (!local) { root.Flags &= ~kGeometryRootBound; }
  root.TemplateCount += last.TemplateCount;
  root.IndexCapacity += last.IndexCapacity;
  previous.IndexCount += next.IndexCount;
  previous.JobCount += next.JobCount;
  previous.Draws += next.Draws;
  previous.WorldSphere = Joined(previous.WorldSphere, next.WorldSphere);
  one.FirstBatch = static_cast<uint32_t>(first);
  Batches.pop_back();
  BatchLayout.pop_back();
  TableRoots_.pop_back();
}
}
