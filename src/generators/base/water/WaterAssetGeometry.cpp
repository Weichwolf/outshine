#include "WaterAsset.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
const WaterAsset::SurfaceGeometry *WaterAsset::PreparedSurfaces() const noexcept {
  return Geometry_.get();
}

bool WaterAsset::SurfacesReady() const noexcept {
  return Geometry_ != nullptr || Surfaces().empty();
}

std::expected<WaterAsset, std::string> WaterAsset::WithSurfaces(SurfaceGeometry geometry) const {
  std::vector<uint8_t> covered(Surfaces().size());
  for (const auto &face : geometry.Mesh.Faces()) {
    if (face.Surface >= covered.size()) {
      return std::unexpected("water facet references an unknown source surface");
    }
    covered[face.Surface] = 1;
  }
  for (const auto &rejection : geometry.Rejections) {
    if (rejection.Surface >= covered.size() || covered[rejection.Surface] != 0 ||
        (rejection.Reason != SurfaceRejectionReason::InvalidTopology &&
         rejection.Reason != SurfaceRejectionReason::CapacityExceeded)) {
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
