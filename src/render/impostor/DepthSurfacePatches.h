#ifndef OUTSHINE_RENDER_IMPOSTOR_DEPTHSURFACEPATCHES_H
#define OUTSHINE_RENDER_IMPOSTOR_DEPTHSURFACEPATCHES_H

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Render {

struct DepthSurfaceSource {
  uint32_t Width = 0, Height = 0;
  std::span<const float> Depth;
  std::span<const uint32_t> Surface;
  double AllowedError = 0.0;
};

struct DepthSurfacePatch {
  uint32_t Left = 0, Top = 0, Right = 0, Bottom = 0;
  double OriginDepth = 0.0, SlopeX = 0.0, SlopeY = 0.0;

  [[nodiscard]] double At(double x, double y) const noexcept {
    return OriginDepth + SlopeX * x + SlopeY * y;
  }
};

enum class DepthSurfaceError { InvalidDimensions, InvalidSamples, InvalidAllowance };

[[nodiscard]] std::expected<std::vector<DepthSurfacePatch>, DepthSurfaceError>
BuildDepthSurfacePatches(DepthSurfaceSource source);

}
#endif
