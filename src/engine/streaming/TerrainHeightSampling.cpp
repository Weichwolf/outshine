#include "TerrainSurfacePublication.h"
#include "GeodeticHeight.h"
#include "TerrainFieldSet.h"
#include "Vec2.h"
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>

namespace outshine {
std::expected<void, TerrainSamplingError>
TerrainSurfacePublication::SampleRenderHeights(std::span<const Vec2> positions,
                                               std::span<float> heights,
                                               double spacingM,
                                               const std::stop_token &stop) const noexcept {
  if (positions.size() != heights.size() || !std::isfinite(spacingM) || spacingM <= 0) {
    return std::unexpected(TerrainSamplingError::InvalidInput);
  }
  if (PublishedSurface() == nullptr) { return std::unexpected(TerrainSamplingError::Unpublished); }
  const auto at = [this](LongitudeLatitude geo) noexcept {
    if (const auto native = HeightMAt(geo)) { return native; }
    return RenderFallback_ ? RenderFallback_->AslMAt(0, geo) : std::optional<double>{};
  };
  for (size_t i = 0; i < positions.size(); ++i) {
    if (stop.stop_requested()) { return std::unexpected(TerrainSamplingError::Cancelled); }
    if (!std::isfinite(positions[i][0]) || !std::isfinite(positions[i][1])) {
      return std::unexpected(TerrainSamplingError::InvalidInput);
    }
    auto height = HeightInFrame(Frame_, positions[i], spacingM, at);
    if (!height && RenderFallback_) {
      const auto coarse = [this](LongitudeLatitude geo) noexcept {
        return RenderFallback_->AslMAt(0, geo);
      };
      height = HeightInFrame(Frame_, positions[i], spacingM, coarse);
    }
    heights[i] = height && std::abs(*height) <= std::numeric_limits<float>::max()
                     ? static_cast<float>(*height)
                     : std::numeric_limits<float>::quiet_NaN();
  }
  return {};
}
}
