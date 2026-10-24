#include "BuildingStampJob.h"

#include "Units.h"

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
  const size_t footprintCount =
      work.Products != nullptr ? work.Products->FootprintCount() : work.Footprints.size();
  const auto points = work.Points;
  if (work.VectorGeneration != VectorGeneration_) {
    return std::unexpected("building stamps require their pinned vector generation");
  }
  if (work.UnitsMost == 0) { return std::unexpected("building stamp work budget is zero"); }
  if (Phase_ == Phase::Unstarted) {
    Products_ = work.Products;
    FootprintCount_ = footprintCount;
    if (Products_ != nullptr) { PinnedProducts_ = Products_->PinProducts(); }
    PointCount_ = points.size();
    RingCount_ = work.Rings.size();
    Phase_ = Phase::Choose;
  } else if (Products_ != work.Products || FootprintCount_ != footprintCount ||
             PointCount_ != points.size() || RingCount_ != work.Rings.size()) {
    return std::unexpected("building stamp source dimensions changed during construction");
  }
  for (size_t visited = 0; visited < work.UnitsMost && Phase_ != Phase::Done; ++visited) {
    if (const auto step = AdvanceStep(work); !step) { return std::unexpected(step.error()); }
  }
  if (Phase_ == Phase::Choose && NextFootprint_ == FootprintCount_) { Phase_ = Phase::Done; }
  return Phase_ == Phase::Done;
}

std::expected<void, std::string_view> BuildingStampJob::SelectProduct() {
  if (Products_ != nullptr) {
    while (NextProduct_ < PinnedProducts_.size()) {
      const auto &product = PinnedProducts_[NextProduct_];
      if (NextFootprint_ < product.FirstFootprint + product.Physical->Prints.size()) { break; }
      ++NextProduct_;
    }
    if (NextProduct_ == PinnedProducts_.size() || !PinnedProducts_[NextProduct_].Coordinates) {
      return std::unexpected("building footprint has no pinned geometry");
    }
    const auto &product = PinnedProducts_[NextProduct_];
    Geometry_ = product.Coordinates.get();
    ProductFootprint_ = &product.Physical->Prints[NextFootprint_ - product.FirstFootprint];
  }
  return {};
}

std::expected<void, std::string_view> BuildingStampJob::Choose(Work work) {
  if (NextFootprint_ == FootprintCount_) {
    Phase_ = Phase::Done;
    return {};
  }
  if (const auto selected = SelectProduct(); !selected) { return selected; }
  const auto points = PointsOf(work);
  const auto rings = RingsOf(work);
  const auto &footprint = FootprintOf(work);
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
  if (footprint.TerrainCeiling && (footprint.TerrainCeiling->OriginPoint >= points.size() / 2u ||
                                   !std::isfinite(footprint.TerrainCeiling->HeightAslM))) {
    return std::unexpected("building terrain ceiling is invalid");
  }
  Part_ = nullptr;
  PartCount_ = footprint.TerrainContacts.PartsView().size();
  if (PartCount_ != 0) {
    if (!footprint.TerrainCeiling) {
      return std::unexpected("building terrain parts have no source frame");
    }
    Part_ = &footprint.TerrainContacts.PartsView()[NextPart_];
    if (Part_->Ring.size() < 3 || !std::isfinite(Part_->HeightAslM)) {
      return std::unexpected("building terrain part is invalid");
    }
    const auto origin = static_cast<size_t>(footprint.TerrainCeiling->OriginPoint) * 2u;
    PartFrame_ =
        TangentFrame::At({.LongitudeDeg = points[origin + 1u], .LatitudeDeg = points[origin]});
  }
  Current_ = EarthworkStamp{};
  Current_.RingEastNorthM.reserve((Part_ != nullptr ? Part_->Ring.size() : count) * 2u);
  Current_.LowE = Current_.LowN = kBeyondAnyCoordinate;
  Current_.HighE = Current_.HighN = -kBeyondAnyCoordinate;
  NextPoint_ = 0;
  NextHole_ = 0;
  Phase_ = Phase::Ring;
  return {};
}

std::expected<void, std::string_view> BuildingStampJob::AppendOuterPoint(Work work) {
  const auto &footprint = FootprintOf(work);
  const auto points = PointsOf(work);
  const size_t point = static_cast<size_t>(footprint.FirstPoint) + NextPoint_;
  const EastNorthUp seated =
      Part_ != nullptr ? PartPosition(Part_->Ring[NextPoint_])
                       : Frame_.ToLocalPosition({.LongitudeDeg = points[2u * point + 1u],
                                                 .LatitudeDeg = points[2u * point],
                                                 .HeightM = static_cast<double>(footprint.SeatM)});
  if (!std::isfinite(seated.EastM) || !std::isfinite(seated.NorthM) || !std::isfinite(seated.UpM)) {
    return std::unexpected("building terrain ring contains a nonfinite point");
  }
  Current_.RingEastNorthM.push_back(seated.EastM);
  Current_.RingEastNorthM.push_back(seated.NorthM);
  Current_.LowE = std::min(Current_.LowE, seated.EastM);
  Current_.HighE = std::max(Current_.HighE, seated.EastM);
  Current_.LowN = std::min(Current_.LowN, seated.NorthM);
  Current_.HighN = std::max(Current_.HighN, seated.NorthM);
  ++NextPoint_;
  if (NextPoint_ == (Part_ != nullptr ? Part_->Ring.size() : footprint.PointCount)) {
    const size_t first = static_cast<size_t>(footprint.FirstPoint) * 2u;
    Current_.PlateauM = Frame_
                            .ToLocalPosition({.LongitudeDeg = points[first + 1u],
                                              .LatitudeDeg = points[first],
                                              .HeightM = static_cast<double>(footprint.SeatM)})
                            .UpM;
    if (footprint.TerrainCeiling) {
      const auto &ceiling = *footprint.TerrainCeiling;
      const size_t origin = static_cast<size_t>(ceiling.OriginPoint) * 2u;
      const LongitudeLatitudeHeight position{.LongitudeDeg = points[origin + 1u],
                                             .LatitudeDeg = points[origin],
                                             .HeightM = Part_ != nullptr ? Part_->HeightAslM
                                                                         : ceiling.HeightAslM};
      const auto at = Frame_.ToLocalPosition(position);
      const auto normal = Frame_.ToLocalDirection(EnuAxesEcef(position).Up);
      if (!std::isfinite(normal.UpM) || normal.UpM <= 0.0) {
        return std::unexpected("building terrain ceiling is not a height graph in this frame");
      }
      Current_.AtE = at.EastM;
      Current_.AtN = at.NorthM;
      Current_.PlateauM = at.UpM;
      Current_.SlopeE = -normal.EastM / normal.UpM;
      Current_.SlopeN = -normal.NorthM / normal.UpM;
    }
    Current_.ApronM = kPadApronM;
    Current_.YieldM =
        std::fabs((Part_ != nullptr ? Part_->HeightAslM : static_cast<double>(footprint.SeatM)) -
                  static_cast<double>(footprint.BaseM));
    NextSeam_ = 0;
    Phase_ = Phase::Seam;
  }
  return {};
}

std::expected<void, std::string_view> BuildingStampJob::AdvanceStep(Work work) {
  switch (Phase_) {
    case Phase::Choose: return Choose(work);
    case Phase::Ring: return AppendOuterPoint(work);
    case Phase::Holes:
      if (const auto appended = AppendHolePoint(work); !appended) { return appended; }
      if (NextHole_ == HoleCount(work)) { FinishStamp(); }
      return {};
    case Phase::Seam:
      Current_.SeamEastNorthM.push_back(Current_.RingEastNorthM[NextSeam_++]);
      if (NextSeam_ == Current_.RingEastNorthM.size()) {
        if (HoleCount(work) == 0) {
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
  if (Part_ != nullptr && NextPart_ + 1u < PartCount_) {
    ++NextPart_;
  } else {
    NextPart_ = 0;
    ++NextFootprint_;
  }
  Phase_ = Phase::Choose;
}

std::expected<void, std::string_view> BuildingStampJob::AppendHolePoint(Work work) {
  if (Part_ != nullptr) {
    const auto &hole = Part_->Holes[NextHole_];
    if (hole.size() < 3) { return std::unexpected("building terrain courtyard is invalid"); }
    const auto at = PartPosition(hole[NextPoint_]);
    if (!std::isfinite(at.EastM) || !std::isfinite(at.NorthM) || !std::isfinite(at.UpM)) {
      return std::unexpected("building terrain courtyard contains a nonfinite point");
    }
    if (NextPoint_ == 0) { Current_.HoleRingsEastNorthM.emplace_back().reserve(hole.size() * 2u); }
    auto &out = Current_.HoleRingsEastNorthM.back();
    out.push_back(at.EastM);
    out.push_back(at.NorthM);
    if (++NextPoint_ == hole.size()) {
      NextPoint_ = 0;
      ++NextHole_;
    }
    return {};
  }
  const auto &footprint = FootprintOf(work);
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

std::expected<std::vector<EarthworkStamp>, std::string_view> BuildingStampJob::Take() {
  if (Phase_ != Phase::Done) { return std::unexpected("building stamps are incomplete"); }
  FinishedHeapBytes_ = 0;
  PinnedProducts_ = decltype(PinnedProducts_){};
  ProductFootprint_ = nullptr;
  Geometry_ = nullptr;
  return std::exchange(Stamps_, {});
}

size_t BuildingStampJob::HeapBytes() const noexcept {
  return Stamps_.capacity() * sizeof(EarthworkStamp) + FinishedHeapBytes_ + Current_.HeapBytes() +
         PinnedProducts_.capacity() *
             sizeof(::outshine::Generators::Osm::BuildingField::PinnedProduct);
}

}
