#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCESUPPORT_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCESUPPORT_H
#include "Geometry.h"
#include "Lens.h"
#include "Viewing.h"
#include <expected>
#include <cstddef>
#include <string>

namespace outshine {
class ImpostorAtlas;
}

namespace outshine::Render {
struct RadiancePlane {
  Vec3 Centre;
  Vec3 TowardEye;
  double HalfExtentM = 0;
  // Normalized displacement along TowardEye: a*u + b*v + c, in the capture frame.
  Vec3 DepthCoefficients{};
};

struct RadiancePlaneFit {
  Vec3 DepthCoefficients{};
  size_t SourcePoints = 0;
  double NormalizedDepthRms = 0;
};

struct RadiancePlaneFitRequest {
  size_t View = 0;
  size_t TexelsMost = 0;
};

[[nodiscard]] std::expected<RadiancePlaneFit, std::string>
FitRadiancePlane(const ImpostorAtlas &source, RadiancePlaneFitRequest request);

// Pixel-aligned support uses the current raster/lens, including jitter; it grants no depth
// authority.
[[nodiscard]] std::expected<Geometry, std::string> BuildProjectedRadianceSupport(
    const RadiancePlane &source, const CameraBasis &camera, const Lens &lens);
}
#endif
