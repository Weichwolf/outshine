#include "BuildingStampJob.h"

#include "math/Units.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {

namespace {
constexpr double kPadApronM = 6.0;
}

std::expected<bool, std::string_view> BuildingStampJob::Advance(Work work) {
  const auto footprints = work.Footprints;
  const auto points = work.Points;
  if (work.VectorGeneration != VectorGeneration_) {
    return std::unexpected("building stamps require their pinned vector generation");
  }
  if (work.UnitsMost == 0) { return std::unexpected("building stamp work budget is zero"); }
  if (Phase_ == Phase::Unstarted) {
    FootprintCount_ = footprints.size();
    PointCount_ = points.size();
    Phase_ = Phase::Choose;
  } else if (FootprintCount_ != footprints.size() || PointCount_ != points.size()) {
    return std::unexpected("building stamp source dimensions changed during construction");
  }
  size_t visited = 0;
  while (visited < work.UnitsMost && Phase_ != Phase::Done) {
    if (Phase_ == Phase::Choose) {
      if (NextFootprint_ == footprints.size()) {
        Phase_ = Phase::Done;
        break;
      }
      const auto &footprint = footprints[NextFootprint_];
      const size_t first = footprint.FirstPoint;
      const size_t count = footprint.PointCount;
      if (count < 3 || first > points.size() / 2u || count > points.size() / 2u - first) {
        ++NextFootprint_;
        ++visited;
        continue;
      }
      Current_ = Yields{};
      Current_.RingEastNorthM.reserve(count * 2u);
      Current_.LowE = Current_.LowN = kBeyondAnyCoordinate;
      Current_.HighE = Current_.HighN = -kBeyondAnyCoordinate;
      NextPoint_ = 0;
      Phase_ = Phase::Ring;
      ++visited;
      continue;
    }
    const auto &footprint = footprints[NextFootprint_];
    if (Phase_ == Phase::Ring) {
      const size_t point = static_cast<size_t>(footprint.FirstPoint) + NextPoint_;
      const EastNorthUp seated = Frame_.Place({.LongitudeDeg = points[2u * point + 1u],
                                               .LatitudeDeg = points[2u * point],
                                               .HeightM = static_cast<double>(footprint.SeatM)});
      Current_.RingEastNorthM.push_back(seated.EastM);
      Current_.RingEastNorthM.push_back(seated.NorthM);
      Current_.LowE = std::min(Current_.LowE, seated.EastM);
      Current_.HighE = std::max(Current_.HighE, seated.EastM);
      Current_.LowN = std::min(Current_.LowN, seated.NorthM);
      Current_.HighN = std::max(Current_.HighN, seated.NorthM);
      ++NextPoint_;
      ++visited;
      if (NextPoint_ == footprint.PointCount) {
        const size_t first = static_cast<size_t>(footprint.FirstPoint) * 2u;
        Current_.PlateauM = Frame_
                                .Place({.LongitudeDeg = points[first + 1u],
                                        .LatitudeDeg = points[first],
                                        .HeightM = static_cast<double>(footprint.SeatM)})
                                .UpM;
        Current_.ApronM = kPadApronM;
        Current_.YieldM =
            std::fabs(static_cast<double>(footprint.SeatM) - static_cast<double>(footprint.BaseM));
        NextSeam_ = 0;
        Phase_ = Phase::Seam;
      }
      continue;
    }
    Current_.SeamEastNorthM.push_back(Current_.RingEastNorthM[NextSeam_++]);
    ++visited;
    if (NextSeam_ == Current_.RingEastNorthM.size()) {
      Stamps_.push_back(std::move(Current_));
      Current_ = Yields{};
      ++NextFootprint_;
      Phase_ = Phase::Choose;
    }
  }
  if (Phase_ == Phase::Choose && NextFootprint_ == footprints.size()) { Phase_ = Phase::Done; }
  return Phase_ == Phase::Done;
}

std::expected<std::vector<Yields>, std::string_view> BuildingStampJob::Take() && {
  if (Phase_ != Phase::Done) { return std::unexpected("building stamps are incomplete"); }
  return std::move(Stamps_);
}

size_t BuildingStampJob::HeapBytes() const noexcept {
  size_t held = Stamps_.capacity() * sizeof(Yields);
  for (const Yields &stamp : Stamps_) { held += stamp.HeapBytes(); }
  held += Current_.HeapBytes();
  return held;
}

}
