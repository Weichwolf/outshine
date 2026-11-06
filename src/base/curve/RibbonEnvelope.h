#ifndef OUTSHINE_BASE_CURVE_RIBBONENVELOPE_H
#define OUTSHINE_BASE_CURVE_RIBBONENVELOPE_H

#include "Vec3.h"
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace outshine {
enum class RibbonRoof : uint8_t { Upper, Lower };

struct RibbonEnvelope {
  // Indices address two complete, ordered four-point stored rings.
  std::array<uint8_t, 6> Triangles;
  // Symmetric distance to the finite reference, including exact run-end stitching.
  double PositionBoundM = 0;
  double RingResidualM = 0;
};

// Final source-frame Float32 words; no continuous-curve, material or raster certificate.
[[nodiscard]] std::optional<RibbonEnvelope>
BoundRibbonSpan(std::span<const Vec3f, 4> first,
                std::span<const Vec3f, 4> second,
                RibbonRoof roof = RibbonRoof::Upper) noexcept;
}
#endif
