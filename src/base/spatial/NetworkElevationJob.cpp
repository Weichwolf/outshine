#include "Wayfinding.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
#include <cmath>
#include <optional>
#include <ratio>
#include <string_view>
#include <utility>

namespace outshine::Path {

NetworkElevationJob::NetworkElevationJob(Network &&network, Network::HeightSource source)
    : Network_(std::move(network)), HeightOf_(std::move(source)) {
  Network_.HeightsM_.assign(Network_.Points_.size() / 2, 0.0);
  if (Network_.Woven_) {
    AtNode_.resize(Network_.Nodes_.size());
  } else {
    Stage_ = Stage::WritePoints;
  }
}

NetworkElevationJob NetworkElevationJob::Begin(Network &&network, Network::HeightSource source) {
  return {std::move(network), std::move(source)};
}

void NetworkElevationJob::SampleNodes(size_t itemsMost) {
  const size_t end = NextNode_ + std::min(itemsMost, AtNode_.size() - NextNode_);
  for (; NextNode_ < end; ++NextNode_) {
    const Network::Node &node = Network_.Nodes_[NextNode_];
    AtNode_[NextNode_] = Network::SampleFiniteHeight(
        HeightOf_, {.LongitudeDeg = node.LongitudeDeg, .LatitudeDeg = node.LatitudeDeg});
  }
  if (NextNode_ == AtNode_.size()) { Stage_ = Stage::WritePoints; }
}

void NetworkElevationJob::WritePoints(size_t itemsMost) {
  const size_t points = Network_.Points_.size() / 2;
  const size_t end = NextPoint_ + std::min(itemsMost, points - NextPoint_);
  for (; NextPoint_ < end; ++NextPoint_) {
    const size_t node = Network_.Woven_ && NextPoint_ < Network_.NodeOfPoint_.size()
                            ? Network_.NodeOfPoint_[NextPoint_]
                            : Network_.Nodes_.size();
    std::optional<double> height;
    if (node < Network_.Nodes_.size()) {
      height = AtNode_[node];
    } else {
      height = Network::SampleFiniteHeight(HeightOf_,
                                           {.LongitudeDeg = Network_.Points_[2 * NextPoint_ + 1],
                                            .LatitudeDeg = Network_.Points_[2 * NextPoint_]});
    }
    if (height) {
      Network_.HeightsM_[NextPoint_] = *height;
      ++Statistics_.Points;
    } else {
      Network_.HeightsM_[NextPoint_] =
          NextPoint_ > 0 && Network_.WayOf_[NextPoint_] == Network_.WayOf_[NextPoint_ - 1]
              ? Network_.HeightsM_[NextPoint_ - 1]
              : 0.0;
      ++Statistics_.Refused;
    }
  }
  if (NextPoint_ == points) { Stage_ = Stage::PrepareStations; }
}

void NetworkElevationJob::PrepareStations() {
  Network_.StationM_.assign(Network_.Points_.size() / 2, 0.0);
  NextWay_ = 0;
  NextProfilePoint_ = 0;
  Stage_ = Stage::Stations;
}

void NetworkElevationJob::BuildStations(size_t itemsMost) {
  size_t visited = 0;
  const Sphere on{.RadiusM = Network_.RadiusM_};
  while (NextWay_ < Network_.Ways_.size() && visited < itemsMost) {
    const Network::Way &way = Network_.Ways_[NextWay_];
    if (NextProfilePoint_ == 0) { NextProfilePoint_ = way.First + 1; }
    if (NextProfilePoint_ >= way.First + way.Count) {
      ++NextWay_;
      NextProfilePoint_ = 0;
      continue;
    }
    const size_t at = NextProfilePoint_++;
    const LongitudeLatitude from{.LongitudeDeg = Network_.Points_[2 * at - 1],
                                 .LatitudeDeg = Network_.Points_[2 * at - 2]};
    const LongitudeLatitude to{.LongitudeDeg = Network_.Points_[2 * at + 1],
                               .LatitudeDeg = Network_.Points_[2 * at]};
    Network_.StationM_[at] = Network_.StationM_[at - 1] + ApartM(from, to, on);
    ++visited;
  }
  if (NextWay_ == Network_.Ways_.size()) { Stage_ = Stage::PrepareSlopes; }
}

void NetworkElevationJob::PrepareSlopes() {
  Network_.SlopeM_.assign(Network_.Points_.size() / 2, 0.0);
  NextWay_ = 0;
  NextProfilePoint_ = 0;
  Stage_ = Stage::Slopes;
}

void NetworkElevationJob::BuildSlopes(size_t itemsMost) {
  size_t visited = 0;
  while (NextWay_ < Network_.Ways_.size() && visited < itemsMost) {
    const Network::Way &way = Network_.Ways_[NextWay_];
    if (way.Count < 2) {
      ++NextWay_;
      continue;
    }
    if (NextProfilePoint_ == 0) { NextProfilePoint_ = way.First; }
    const size_t last = way.First + way.Count - 1;
    if (NextProfilePoint_ > last) {
      ++NextWay_;
      NextProfilePoint_ = 0;
      continue;
    }
    const size_t at = NextProfilePoint_++;
    const size_t before = at == way.First ? at : at - 1;
    const size_t after = at == last ? at : at + 1;
    const double run = Network_.StationM_[after] - Network_.StationM_[before];
    Network_.SlopeM_[at] =
        run > 0.0 ? (Network_.HeightsM_[after] - Network_.HeightsM_[before]) / run : 0.0;
    ++visited;
  }
  if (NextWay_ == Network_.Ways_.size()) {
    NextProfilePoint_ = 0;
    Stage_ = Stage::CountGrades;
  }
}

void NetworkElevationJob::CountGrades(size_t itemsMost) {
  const size_t end =
      NextProfilePoint_ + std::min(itemsMost, Network_.SlopeM_.size() - NextProfilePoint_);
  for (; NextProfilePoint_ < end; ++NextProfilePoint_) {
    const double grade = std::fabs(Network_.SlopeM_[NextProfilePoint_]);
    if (grade > Network::kTenPercent) { ++Statistics_.OverTenPercent; }
    if (grade > Network::kThirtyPercent) { ++Statistics_.OverThirtyPercent; }
    Statistics_.SteepestGrade = std::max(Statistics_.SteepestGrade, grade);
    if (!Network_.Ways_[Network_.WayOf_[NextProfilePoint_]].Sealed) { continue; }
    if (grade > Network::kTenPercent) { ++Statistics_.SealedOverTenPercent; }
    Statistics_.SteepestSealedGrade = std::max(Statistics_.SteepestSealedGrade, grade);
  }
  if (NextProfilePoint_ != Network_.SlopeM_.size()) { return; }
  AtNode_.clear();
  HeightOf_ = {};
  Stage_ = Stage::Done;
}

std::expected<bool, std::string_view> NetworkElevationJob::Advance(size_t itemsMost) {
  return Advance({.SamplesMost = itemsMost, .ProfilePointsMost = itemsMost});
}

std::expected<bool, std::string_view> NetworkElevationJob::Advance(Budget budget) {
  if (budget.SamplesMost == 0 || budget.ProfilePointsMost == 0) {
    return std::unexpected("network elevation work budget is zero");
  }
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  switch (Stage_) {
    case Stage::SampleNodes: SampleNodes(budget.SamplesMost); break;
    case Stage::WritePoints: WritePoints(budget.SamplesMost); break;
    case Stage::PrepareStations: PrepareStations(); break;
    case Stage::Stations: BuildStations(budget.ProfilePointsMost); break;
    case Stage::PrepareSlopes: PrepareSlopes(); break;
    case Stage::Slopes: BuildSlopes(budget.ProfilePointsMost); break;
    case Stage::CountGrades: CountGrades(budget.ProfilePointsMost); break;
    case Stage::Done: return true;
  }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  switch (before) {
    case Stage::SampleNodes:
      Worst_.SampleNodesMs = std::max(Worst_.SampleNodesMs, elapsedMs);
      break;
    case Stage::WritePoints:
      Worst_.WritePointsMs = std::max(Worst_.WritePointsMs, elapsedMs);
      break;
    case Stage::PrepareStations:
    case Stage::Stations: Worst_.StationsMs = std::max(Worst_.StationsMs, elapsedMs); break;
    case Stage::PrepareSlopes:
    case Stage::Slopes: Worst_.SlopesMs = std::max(Worst_.SlopesMs, elapsedMs); break;
    case Stage::CountGrades: Worst_.GradesMs = std::max(Worst_.GradesMs, elapsedMs); break;
    case Stage::Done: break;
  }
  return Stage_ == Stage::Done;
}

std::expected<NetworkElevationJob::Result, std::string_view> NetworkElevationJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("network elevation is incomplete"); }
  return Result{.Graph = std::move(Network_), .Statistics = Statistics_};
}

}
