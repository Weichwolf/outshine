#include "BuildingSourceView.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <limits>
#include <unordered_set>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
Vec3 BuildingCaptureFrame::Local(const Vec3 &point) const noexcept {
  const Vec3 relative = point - Origin;
  return {{Dot(relative, Axes.East), Dot(relative, Axes.Up), -Dot(relative, Axes.North)}};
}

Vec3 BuildingCaptureFrame::SourceDirection(const Vec3 &direction) const noexcept {
  return Axes.East * direction[0] + Axes.Up * direction[1] - Axes.North * direction[2];
}

bool BuildingCaptureFrame::Valid() const noexcept {
  const auto finite = [](const Vec3 &v) {
    return std::ranges::all_of(v, [](double x) { return std::isfinite(x); });
  };
  const auto &axes = Axes;
  constexpr double tolerance = 1e-10;
  return finite(Origin) && finite(axes.East) && finite(axes.North) && finite(axes.Up) &&
         std::abs(Dot(axes.East, axes.East) - 1) <= tolerance &&
         std::abs(Dot(axes.North, axes.North) - 1) <= tolerance &&
         std::abs(Dot(axes.Up, axes.Up) - 1) <= tolerance &&
         std::abs(Dot(axes.East, axes.North)) <= tolerance &&
         std::abs(Dot(axes.East, axes.Up)) <= tolerance &&
         std::abs(Dot(axes.North, axes.Up)) <= tolerance &&
         Dot(Cross(axes.East, axes.North), axes.Up) >= 1 - tolerance;
}

BuildingSourceView::BuildingSourceView(std::shared_ptr<const PreparedStructureTile> base,
                                       BuildingCaptureFrame frame,
                                       Box bounds,
                                       std::vector<uint32_t> sources,
                                       std::vector<uint32_t> unsupported)
    : Base_(std::move(base)),
      Frame_(frame),
      Bounds_(bounds),
      Sources_(std::move(sources)),
      Unsupported_(std::move(unsupported)) {}

std::expected<BuildingSourceView, BuildingSourceView::Error>
BuildingSourceView::Open(std::shared_ptr<const PreparedStructureTile> base,
                         std::span<const uint32_t> cells,
                         BuildingCaptureFrame frame) {
  if (!base || base->Structures.size() != base->Surfaces.size() || cells.empty() ||
      base->Surfaces.size() > std::numeric_limits<uint32_t>::max() || !frame.Valid()) {
    return std::unexpected(Error{.Message = "invalid building source view inputs"});
  }
  const std::unordered_set<uint32_t> selected(cells.begin(), cells.end());
  std::vector<uint32_t> ordinals;
  std::vector<uint32_t> unsupported;
  Box bounds;
  for (size_t at = 0; at < base->Surfaces.size(); ++at) {
    if (!selected.contains(base->Structures[at].Layout.Cell.Index)) { continue; }
    const auto &source = base->Surfaces[at];
    if (!source.SupportsProjection() || source.FaceCount() == 0 || source.Bounds().Empty()) {
      unsupported.push_back(static_cast<uint32_t>(at));
      continue;
    }
    for (size_t axis = 0; axis < 3; ++axis) {
      if (!std::isfinite(source.Bounds().Min[axis]) || !std::isfinite(source.Bounds().Max[axis]) ||
          source.Bounds().Min[axis] > source.Bounds().Max[axis]) {
        return std::unexpected(Error{.Source = static_cast<uint32_t>(at),
                                     .Message = "invalid building source bounds"});
      }
    }
    ordinals.push_back(static_cast<uint32_t>(at));
    for (unsigned corner = 0; corner < 8; ++corner) {
      const auto local = frame.Local(source.Bounds().Corner(corner));
      if (!std::ranges::all_of(local, [](double value) { return std::isfinite(value); })) {
        return std::unexpected(Error{.Source = static_cast<uint32_t>(at),
                                     .Message = "invalid local building source bounds"});
      }
      bounds.Cover(local);
    }
  }
  if (ordinals.empty() || ordinals.size() >= std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(Error{.Message = "building source selection unavailable"});
  }
  return BuildingSourceView(
      std::move(base), frame, bounds, std::move(ordinals), std::move(unsupported));
}

}
