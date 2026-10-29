#ifndef OUTSHINE_RENDER_IMPOSTOR_RADIANCESUPPORT_H
#define OUTSHINE_RENDER_IMPOSTOR_RADIANCESUPPORT_H
#include "Geometry.h"
#include "Lens.h"
#include "Viewing.h"
#include <expected>
#include <string>

namespace outshine::Render {
struct RadiancePlane {
  Vec3 Centre;
  Vec3 TowardEye;
  double HalfExtentM = 0;
};

// Pixel-aligned support uses the current raster/lens, including jitter; it grants no depth
// authority.
[[nodiscard]] std::expected<Geometry, std::string> BuildProjectedRadianceSupport(
    const RadiancePlane &source, const CameraBasis &camera, const Lens &lens);
}
#endif
