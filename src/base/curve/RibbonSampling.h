#ifndef OUTSHINE_BASE_CURVE_RIBBONSAMPLING_H
#define OUTSHINE_BASE_CURVE_RIBBONSAMPLING_H

#include "Ribbon.h"
#include <array>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace outshine {
struct RibbonVertex {
  Vec3f PositionM;
  Vec3f NormalM;
  float AcrossM = 0;
};

struct RibbonSectionSamples {
  std::array<RibbonVertex, kRibbonAcross * 2 + 4> Vertices;
  size_t Count = 0;
};

struct RibbonSamplingPlan {
  Vec3 OriginM;
  std::vector<double> StationsM;
};

[[nodiscard]] std::expected<RibbonSamplingPlan, std::string>
PrepareRibbonSampling(const ReferenceLine &along,
                      const Section &section,
                      double fromM,
                      double toM,
                      double stepM,
                      std::span<const double> requiredStations = {});

[[nodiscard]] RibbonSectionSamples SampleRibbonSection(const Placed &on,
                                                       const Section &section,
                                                       const Vec3 &originM,
                                                       RibbonForm form) noexcept;
}
#endif
