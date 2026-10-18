#ifndef OUTSHINE_GENERATORS_FLORA_PLANTLIMBPLAN_H
#define OUTSHINE_GENERATORS_FLORA_PLANTLIMBPLAN_H

#include "math/Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {
struct PlantLimbAnchor {
  uint32_t Node = 0;
  Vec3 PositionM;
  double RadiusM = 0;
};

struct PlantLimbSample {
  Vec3 CentreM, Tangent, Curvature;
  double SpeedM = 0, RadiusM = 0, RadiusSlopeM = 0;
};

class PlantLimbPlan {
public:
  static constexpr size_t MostAnchors = 65;

  [[nodiscard]] static std::expected<PlantLimbPlan, std::string_view>
  Create(std::span<const PlantLimbAnchor> anchors);

  [[nodiscard]] std::span<const uint32_t> Nodes() const noexcept { return Nodes_; }

  [[nodiscard]] std::expected<PlantLimbSample, std::string_view>
  Sample(double parameter) const noexcept;

private:
  friend class PlantLimbArchive;

  struct Segment {
    double Parameter = 0, Span = 0;
    std::array<Vec3, 4> Controls;
    std::array<double, 4> Radius;
  };

  PlantLimbPlan() = default;
  std::vector<uint32_t> Nodes_;
  std::vector<Segment> Segments_;
};
}
#endif
