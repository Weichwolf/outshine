#ifndef OUTSHINE_GENERATORS_WATER_WATERCOURSEPROFILE_H
#define OUTSHINE_GENERATORS_WATER_WATERCOURSEPROFILE_H

#include "Earth.h"
#include "PlanHierarchy.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace outshine::Generators {
struct WaterCourseSegment {
  std::array<EastNorth, 2> Points;
  std::array<double, 2> ModelHeightsM;
};

enum class WaterProfileError { InvalidInput, CapacityExceeded };

struct WaterProfileSample {
  double ModelHeightM = 0;
  uint32_t NearestSegment = 0;
  uint32_t References = 0;
  uint64_t NodesVisited = 0;
  uint64_t SegmentTests = 0;
};

class WaterCourseProfile {
public:
  static constexpr uint32_t MaximumSegments = 8192;
  static constexpr uint32_t MaximumReferences = 64;
  static constexpr double DefaultBlendM = 12;

  [[nodiscard]] static std::expected<WaterCourseProfile, WaterProfileError>
  Build(std::span<const WaterCourseSegment> segments, uint32_t segmentsMost = MaximumSegments);

  [[nodiscard]] std::expected<WaterProfileSample, WaterProfileError>
  Sample(EastNorth point,
         double blendM = DefaultBlendM,
         uint32_t referencesMost = MaximumReferences) const;

  [[nodiscard]] std::span<const WaterCourseSegment> Segments() const noexcept { return Segments_; }

  [[nodiscard]] size_t HeapBytes() const noexcept {
    return Segments_.capacity() * sizeof(WaterCourseSegment) +
           LengthsSquared_.capacity() * sizeof(double) + Hierarchy_.HeapBytes();
  }

private:
  PlanHierarchy Hierarchy_;
  std::vector<WaterCourseSegment> Segments_;
  std::vector<double> LengthsSquared_;
};
}
#endif
