#ifndef OUTSHINE_GENERATORS_EARTH_GROUNDREGIONDEFINITION_H
#define OUTSHINE_GENERATORS_EARTH_GROUNDREGIONDEFINITION_H

#include "GroundMesher.h"
#include "GroundQuality.h"
#include "Earth.h"
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace outshine::Generators {
enum class GroundProjection : uint8_t { Perspective, Orthographic };

struct GroundRegionDefinition {
  Around Coverage{};
  LongitudeLatitude Anchor{};
  GroundQuality Quality = GroundQuality::Refined;
  std::array<double, 3> EyeM{};
  GroundProjection Projection = GroundProjection::Perspective;
  double YfovRad = 0;
  double YMagM = 0;
  int HeightPx = 0;
  double ErrorPx = 0;
  uint32_t PagesMost = 0;
  std::optional<std::string> DeclaredDigest = std::nullopt;
};

[[nodiscard]] std::expected<std::vector<uint8_t>, std::string>
EncodeGroundRegionDefinition(const GroundRegionDefinition &definition);
}
#endif
