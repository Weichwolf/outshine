#ifndef OUTSHINE_GENERATORS_TERRAIN_BUILDINGSTAMPJOB_H
#define OUTSHINE_GENERATORS_TERRAIN_BUILDINGSTAMPJOB_H

#include "GeographicRing.h"
#include "BuildingField.h"
#include "GroundMesher.h"
#include "TangentFrame.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators {

class BuildingStampJob {
public:
  struct Work {
    const ::outshine::Generators::Osm::BuildingField *Products = nullptr;
    std::span<const ::outshine::Ground::BuildingFootprint> Footprints;
    std::span<const double> Points;
    std::span<const GeographicRing> Rings;
    uint64_t VectorGeneration = 0;
    size_t UnitsMost = 0;
  };

  BuildingStampJob(TangentFrame frame, uint64_t vectorGeneration)
      : Frame_(frame), VectorGeneration_(vectorGeneration) {}

  [[nodiscard]] std::expected<bool, std::string_view> Advance(Work work);
  [[nodiscard]] std::expected<std::vector<EarthworkStamp>, std::string_view> Take() &&;
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  enum class Phase : uint8_t { Unstarted, Choose, Ring, Seam, Holes, Done };

  [[nodiscard]] std::expected<void, std::string_view> AppendHolePoint(Work work);
  void FinishStamp();
  [[nodiscard]] std::expected<void, std::string_view> Choose(Work work);
  [[nodiscard]] std::expected<void, std::string_view> AdvanceStep(Work work);
  void AppendOuterPoint(Work work);

  [[nodiscard]] std::span<const double> PointsOf(Work work) const noexcept {
    return Geometry_ != nullptr ? std::span<const double>(Geometry_->Points) : work.Points;
  }

  [[nodiscard]] std::span<const GeographicRing> RingsOf(Work work) const noexcept {
    return Geometry_ != nullptr ? std::span<const GeographicRing>(Geometry_->Rings) : work.Rings;
  }

  const ::outshine::Generators::Osm::BuildingField *Products_ = nullptr;
  const Ground::BuildingGeometry *Geometry_ = nullptr;
  TangentFrame Frame_;
  uint64_t VectorGeneration_ = 0;
  std::vector<EarthworkStamp> Stamps_;
  EarthworkStamp Current_;
  size_t FinishedHeapBytes_ = 0;
  size_t FootprintCount_ = 0;
  size_t PointCount_ = 0;
  size_t RingCount_ = 0;
  size_t NextHole_ = 0;
  size_t NextFootprint_ = 0;
  size_t NextPoint_ = 0;
  size_t NextSeam_ = 0;
  Phase Phase_ = Phase::Unstarted;
};

}
#endif
