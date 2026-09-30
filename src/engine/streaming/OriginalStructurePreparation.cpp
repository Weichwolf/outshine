#include "OriginalStructurePreparation.h"

#include "OsmBuildingFootprints.h"
#include "TangentFrame.h"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <utility>

namespace outshine {
namespace {

std::expected<Generators::RawTile, std::string>
PrepareOriginal(std::shared_ptr<const Data::OsmSourceSnapshot> source,
                Generators::OriginalStructurePolicy policy,
                const std::stop_token &stop) {
  if (!source || source->Coverage.empty()) {
    return std::unexpected("original buildings require declared source coverage");
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto buildings = Ground::OsmBuildingFootprints::Build(source, policy.PointsMost);
  if (!buildings) {
    return std::unexpected("original building footprint failed at object " +
                           std::to_string(buildings.error().Source.Id) + " with code " +
                           std::to_string(static_cast<int>(buildings.error().Code)));
  }
  Data::SourceCoverage bounds = source->Coverage.front();
  for (const auto &coverage : source->Coverage) {
    bounds.WestDeg = std::min(bounds.WestDeg, coverage.WestDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, coverage.SouthDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, coverage.EastDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, coverage.NorthDeg);
  }
  const auto points = buildings->Points();
  for (size_t point = 0; point < points.size(); point += 2u) {
    const auto at =
        TangentFrame::At({.LongitudeDeg = points[point + 1u], .LatitudeDeg = points[point]});
    const double half = policy.PointWidthM / 2.0;
    const auto low = at.ApproximateGeographicAt({.EastM = -half, .NorthM = -half});
    const auto high = at.ApproximateGeographicAt({.EastM = half, .NorthM = half});
    bounds.WestDeg = std::min(bounds.WestDeg, low.LongitudeDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, low.LatitudeDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, high.LongitudeDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, high.LatitudeDeg);
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto raw = Generators::OriginalStructureInput(
      *buildings, {.Snapshot = std::move(source), .Bounds = bounds}, policy);
  if (!raw) {
    return std::unexpected("original building input failed with code " +
                           std::to_string(static_cast<int>(raw.error())));
  }
  return std::move(*raw);
}

}

OriginalStructurePreparation::OriginalStructurePreparation(
    Tasks &pool,
    std::shared_ptr<const Data::OsmSourceSnapshot> source,
    Generators::OriginalStructurePolicy policy)
    : Pool_(&pool), Output_(std::make_shared<Output>()) {
  Handle_ =
      pool.Post([source = std::move(source), policy, output = Output_, stop = Stop_.get_token()] {
        output->Value = PrepareOriginal(source, policy, stop);
      });
}

OriginalStructurePreparation::~OriginalStructurePreparation() {
  (void)Stop_.request_stop();
  if (Handle_ != Tasks::kNoTask) { Pool_->Wait(Handle_); }
}

OriginalStructurePreparation::Phase OriginalStructurePreparation::Poll() {
  if (Handle_ == Tasks::kNoTask || !Pool_->Done(Handle_)) { return Phase_; }
  Handle_ = Tasks::kNoTask;
  if (Output_->Value) {
    Input_ = std::make_shared<const Generators::RawTile>(std::move(*Output_->Value));
    Phase_ = Phase::Ready;
  } else {
    Error_ = std::move(Output_->Value.error());
    Phase_ = Phase::Failed;
  }
  Output_.reset();
  return Phase_;
}

bool OriginalStructurePreparation::AwaitSlice(double seconds) const {
  return Handle_ != Tasks::kNoTask && Pool_->AwaitCompletion(seconds);
}

}
