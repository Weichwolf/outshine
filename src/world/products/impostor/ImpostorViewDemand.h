#ifndef OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWDEMAND_H
#define OUTSHINE_WORLD_PRODUCTS_IMPOSTOR_IMPOSTORVIEWDEMAND_H

#include "Vec3.h"
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine {

struct ImpostorEyeSweep {
  Vec3 Begin;
  Vec3 End;
};

struct ImpostorDistanceRamp {
  double BeginM;
  double EndM;

  [[nodiscard]] float Weight(double distanceM) const;
};

enum class ImpostorDistanceMode { Geometry, Handoff, Impostor };

[[nodiscard]] ImpostorDistanceRamp ImpostorProjectionRamp(double halfExtentM,
                                                          uint32_t side,
                                                          double focalPixels,
                                                          double maximumTexelPx = 1);
[[nodiscard]] ImpostorDistanceMode ClassifyImpostorSweep(const ImpostorEyeSweep &eye,
                                                         const ImpostorDistanceRamp &ramp);

[[nodiscard]] std::vector<size_t> SelectImpostorDirections(std::span<const Vec3> directions,
                                                           const ImpostorEyeSweep &eye,
                                                           bool requireElevatedView = false);
[[nodiscard]] std::vector<size_t> SelectBlendedImpostorDirections(std::span<const Vec3> directions,
                                                                  const ImpostorEyeSweep &eye);
[[nodiscard]] float ImpostorDirectionWeight(double aboveCut);
[[nodiscard]] double ImpostorDirectionCut(std::span<const double> scores);

}
#endif
