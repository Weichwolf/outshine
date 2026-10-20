#include "WaterAsset.h"
#include "Capacity.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool MarkSources(const WaterAsset::SurfaceGeometry &geometry, std::span<uint8_t> covered) {
  const auto faces = geometry.Mesh.Faces();
  const auto &offsets = geometry.FaceSourceOffsets;
  const auto &sources = geometry.FaceSources;
  if (offsets.empty()) {
    if (!sources.empty()) { return false; }
    return std::ranges::all_of(faces, [&](const auto &face) {
      if (face.Surface >= covered.size()) { return false; }
      covered[face.Surface] = 1;
      return true;
    });
  }
  if (offsets.size() != faces.size() + 1 || offsets.front() != 0 ||
      offsets.back() != sources.size()) {
    return false;
  }
  for (size_t face = 0; face < faces.size(); ++face) {
    const size_t first = offsets[face];
    const size_t end = offsets[face + 1];
    if (first >= end || end > sources.size()) { return false; }
    bool primary = false;
    for (size_t at = first; at < end; ++at) {
      const auto source = sources[at];
      if (source >= covered.size() || (at > first && sources[at - 1] >= source)) { return false; }
      covered[source] = 1;
      primary = primary || source == faces[face].Surface;
    }
    if (!primary) { return false; }
  }
  return true;
}
}

const WaterAsset::SurfaceGeometry *WaterAsset::PreparedSurfaces() const noexcept {
  return Geometry_.get();
}

bool WaterAsset::SurfacesReady() const noexcept {
  return Geometry_ != nullptr || Surfaces().empty();
}

size_t WaterAsset::PreparedSurfaceBytes() const noexcept {
  return Geometry_ ? sizeof(SurfaceGeometry) + Geometry_->Mesh.HeapBytes() +
                         CapacityBytes(Geometry_->Rejections) +
                         CapacityBytes(Geometry_->FaceSourceOffsets) +
                         CapacityBytes(Geometry_->FaceSources)
                   : 0;
}

std::expected<WaterAsset, std::string> WaterAsset::WithSurfaces(SurfaceGeometry geometry) const {
  std::vector<uint8_t> covered(Surfaces().size());
  if (!MarkSources(geometry, covered)) {
    return std::unexpected("water facet source memberships are invalid");
  }
  for (const auto &rejection : geometry.Rejections) {
    if (rejection.Surface >= covered.size() || covered[rejection.Surface] != 0 ||
        (rejection.Reason != SurfaceRejectionReason::InvalidTopology &&
         rejection.Reason != SurfaceRejectionReason::CapacityExceeded &&
         rejection.Reason != SurfaceRejectionReason::InvalidHeightModel)) {
      return std::unexpected("water surface rejection is duplicate or invalid");
    }
    covered[rejection.Surface] = 1;
  }
  if (std::ranges::any_of(covered, [](uint8_t mark) { return mark == 0; })) {
    return std::unexpected("water surface has neither prepared facets nor an explicit rejection");
  }
  WaterAsset asset = *this;
  asset.Geometry_ = std::make_shared<const SurfaceGeometry>(std::move(geometry));
  return asset;
}
}
