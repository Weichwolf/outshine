#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGFOUNDATIONJOB_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGFOUNDATIONJOB_H
#include "BuildingPhysicalProduct.h"

#include "GeographicRing.h"
#include "BuildingFootprint.h"
#include "BuildingGeometry.h"
#include "EarthworkStamp.h"
#include "TangentFrame.h"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Generators {

class BuildingFoundationJob {
public:
  struct Work {
    uint64_t Generation = 0;
    size_t UnitsMost = 0;
  };

  BuildingFoundationJob(TangentFrame frame,
                        uint64_t generation,
                        std::vector<::outshine::Ground::BuildingProductLease> products)
      : PinnedProducts_(std::move(products)), Frame_(frame), Generation_(generation) {}

  [[nodiscard]] std::expected<bool, std::string_view> Advance(Work work);
  [[nodiscard]] std::expected<std::vector<EarthworkStamp>, std::string_view> Take();
  [[nodiscard]] size_t HeapBytes() const noexcept;

private:
  enum class Phase : uint8_t { Unstarted, Choose, Ring, Seam, Holes, Done };

  [[nodiscard]] std::expected<void, std::string_view> PrepareProducts();
  [[nodiscard]] std::expected<void, std::string_view> AppendHolePoint();
  void FinishStamp();
  [[nodiscard]] std::expected<void, std::string_view> Choose();
  [[nodiscard]] std::expected<void, std::string_view> SelectProduct();
  [[nodiscard]] std::expected<void, std::string_view> AdvanceStep();
  [[nodiscard]] std::expected<void, std::string_view> AppendOuterPoint();

  [[nodiscard]] size_t HoleCount() const noexcept {
    return Part_ != nullptr ? Part_->Holes.size() : FootprintOf().HoleCount;
  }

  [[nodiscard]] const ::outshine::Ground::BuildingFootprint &FootprintOf() const noexcept {
    return *ProductFootprint_;
  }

  [[nodiscard]] EastNorthUp PartPosition(EastNorth point) const {
    return Frame_.ToLocalPosition(PartFrame_.ToEcefPosition(
        {.EastM = point.EastM, .NorthM = point.NorthM, .UpM = Part_->HeightAslM}));
  }

  [[nodiscard]] std::span<const double> PointsOf() const noexcept { return Geometry_->Points(); }

  [[nodiscard]] std::span<const GeographicRing> RingsOf() const noexcept {
    return Geometry_->Rings();
  }

  const ::outshine::Ground::BuildingGeometry *Geometry_ = nullptr;
  std::vector<::outshine::Ground::BuildingProductLease> PinnedProducts_;
  const ::outshine::Ground::BuildingFootprint *ProductFootprint_ = nullptr;
  size_t NextProduct_ = 0;
  const ::outshine::Ground::BuildingTerrainPart *Part_ = nullptr;
  TangentFrame Frame_;
  TangentFrame PartFrame_;
  uint64_t Generation_ = 0;
  std::vector<EarthworkStamp> Stamps_;
  EarthworkStamp Current_;
  size_t FinishedHeapBytes_ = 0;
  size_t FootprintCount_ = 0;
  size_t NextHole_ = 0;
  size_t NextFootprint_ = 0;
  size_t NextPoint_ = 0;
  size_t NextSeam_ = 0;
  size_t NextPart_ = 0;
  size_t PartCount_ = 0;
  Phase Phase_ = Phase::Unstarted;
};

}
#endif
