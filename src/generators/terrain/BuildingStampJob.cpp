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
    Products_ = work.Products;
    FootprintCount_ = footprints.size();
    PointCount_ = points.size();
    RingCount_ = work.Rings.size();
    Phase_ = Phase::Choose;
  } else if (Products_ != work.Products || FootprintCount_ != footprints.size() ||
             PointCount_ != points.size() || RingCount_ != work.Rings.size()) {
    return std::unexpected("building stamp source dimensions changed during construction");
  }
  for (size_t visited = 0; visited < work.UnitsMost && Phase_ != Phase::Done; ++visited) {
    if (const auto step = AdvanceStep(work); !step) { return std::unexpected(step.error()); }
  }
  if (Phase_ == Phase::Choose && NextFootprint_ == footprints.size()) { Phase_ = Phase::Done; }
  return Phase_ == Phase::Done;
}

std::expected<void, std::string_view> BuildingStampJob::Choose(Work work) {
  const auto footprints = work.Footprints;
  if (NextFootprint_ == footprints.size()) {
    Phase_ = Phase::Done;
    return {};
  }
  Geometry_ =
      work.Products != nullptr ? work.Products->GeometryOfFootprint(NextFootprint_) : nullptr;
  if (work.Products != nullptr && Geometry_ == nullptr) {
    return std::unexpected("building footprint has no pinned geometry");
  }
  const auto points = PointsOf(work);
  const auto rings = RingsOf(work);
  const auto &footprint = footprints[NextFootprint_];
  const size_t first = footprint.FirstPoint;
  const size_t count = footprint.PointCount;
  if (footprint.MinimumHeightM > 0.0f ||
      (footprint.MinimumHeightM < 0.0f && footprint.HeightM <= 0.0f) || count < 3 ||
      first > points.size() / 2u || count > points.size() / 2u - first) {
    ++NextFootprint_;
    return {};
  }
  if (footprint.HoleCount != 0 && (footprint.FirstHole > rings.size() ||
                                   footprint.HoleCount > rings.size() - footprint.FirstHole)) {
    return std::unexpected("building courtyard ring range is invalid");
  }
  Current_ = EarthworkStamp{};
  Current_.RingEastNorthM.reserve(count * 2u);
  Current_.LowE = Current_.LowN = kBeyondAnyCoordinate;
  Current_.HighE = Current_.HighN = -kBeyondAnyCoordinate;
  NextPoint_ = 0;
  NextHole_ = 0;
  Phase_ = Phase::Ring;
  return {};
}

void BuildingStampJob::AppendOuterPoint(Work work) {
  const auto &footprint = work.Footprints[NextFootprint_];
  const auto points = PointsOf(work);
  const size_t point = static_cast<size_t>(footprint.FirstPoint) + NextPoint_;
  const EastNorthUp seated =
      Frame_.ToLocalPosition({.LongitudeDeg = points[2u * point + 1u],
                              .LatitudeDeg = points[2u * point],
                              .HeightM = static_cast<double>(footprint.SeatM)});
  Current_.RingEastNorthM.push_back(seated.EastM);
  Current_.RingEastNorthM.push_back(seated.NorthM);
  Current_.LowE = std::min(Current_.LowE, seated.EastM);
  Current_.HighE = std::max(Current_.HighE, seated.EastM);
  Current_.LowN = std::min(Current_.LowN, seated.NorthM);
  Current_.HighN = std::max(Current_.HighN, seated.NorthM);
  ++NextPoint_;
  if (NextPoint_ == footprint.PointCount) {
    const size_t first = static_cast<size_t>(footprint.FirstPoint) * 2u;
    Current_.PlateauM = Frame_
                            .ToLocalPosition({.LongitudeDeg = points[first + 1u],
                                              .LatitudeDeg = points[first],
                                              .HeightM = static_cast<double>(footprint.SeatM)})
                            .UpM;
    Current_.ApronM = kPadApronM;
    Current_.YieldM =
        std::fabs(static_cast<double>(footprint.SeatM) - static_cast<double>(footprint.BaseM));
    NextSeam_ = 0;
    Phase_ = Phase::Seam;
  }
}

std::expected<void, std::string_view> BuildingStampJob::AdvanceStep(Work work) {
  switch (Phase_) {
    case Phase::Choose: return Choose(work);
    case Phase::Ring: AppendOuterPoint(work); return {};
    case Phase::Holes:
      if (const auto appended = AppendHolePoint(work); !appended) { return appended; }
      if (NextHole_ == work.Footprints[NextFootprint_].HoleCount) { FinishStamp(); }
      return {};
    case Phase::Seam:
      Current_.SeamEastNorthM.push_back(Current_.RingEastNorthM[NextSeam_++]);
      if (NextSeam_ == Current_.RingEastNorthM.size()) {
        if (work.Footprints[NextFootprint_].HoleCount == 0) {
          FinishStamp();
        } else {
          NextPoint_ = 0;
          Phase_ = Phase::Holes;
        }
      }
      return {};
    case Phase::Done: return {};
    case Phase::Unstarted: return std::unexpected("building stamp phase is uninitialized");
  }
  return std::unexpected("building stamp phase is invalid");
}

void BuildingStampJob::FinishStamp() {
  FinishedHeapBytes_ += Current_.HeapBytes();
  Stamps_.push_back(std::move(Current_));
  Current_ = EarthworkStamp{};
  ++NextFootprint_;
  Phase_ = Phase::Choose;
}

std::expected<void, std::string_view> BuildingStampJob::AppendHolePoint(Work work) {
  const auto &footprint = work.Footprints[NextFootprint_];
  const auto points = PointsOf(work);
  const auto &ring = RingsOf(work)[static_cast<size_t>(footprint.FirstHole) + NextHole_];
  if (ring.Exterior || ring.Count < 3 || ring.First > points.size() / 2 ||
      ring.Count > points.size() / 2 - ring.First) {
    return std::unexpected("building courtyard coordinates are invalid");
  }
  if (NextPoint_ == 0) {
    Current_.HoleRingsEastNorthM.emplace_back().reserve(static_cast<size_t>(ring.Count) * 2);
  }
  const size_t point = static_cast<size_t>(ring.First) + NextPoint_;
  const auto local = Frame_.ToLocalPosition({.LongitudeDeg = points[point * 2 + 1],
                                             .LatitudeDeg = points[point * 2],
                                             .HeightM = footprint.SeatM});
  auto &hole = Current_.HoleRingsEastNorthM.back();
  hole.push_back(local.EastM);
  hole.push_back(local.NorthM);
  if (++NextPoint_ == ring.Count) {
    NextPoint_ = 0;
    ++NextHole_;
  }
  return {};
}

std::expected<std::vector<EarthworkStamp>, std::string_view> BuildingStampJob::Take() && {
  if (Phase_ != Phase::Done) { return std::unexpected("building stamps are incomplete"); }
  FinishedHeapBytes_ = 0;
  return std::exchange(Stamps_, {});
}

size_t BuildingStampJob::HeapBytes() const noexcept {
  return Stamps_.capacity() * sizeof(EarthworkStamp) + FinishedHeapBytes_ + Current_.HeapBytes();
}

}
