#include "Wayfinding.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <expected>
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
  if (NextPoint_ == points) { Stage_ = Stage::Profile; }
}

void NetworkElevationJob::Profile() {
  Network_.StationsOfWays();
  Network_.SlopesOfWays();
  Network_.CountGrades(Statistics_);
  AtNode_.clear();
  HeightOf_ = {};
  Stage_ = Stage::Done;
}

std::expected<bool, std::string_view> NetworkElevationJob::Advance(size_t itemsMost) {
  if (itemsMost == 0) { return std::unexpected("network elevation work budget is zero"); }
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  switch (Stage_) {
    case Stage::SampleNodes: SampleNodes(itemsMost); break;
    case Stage::WritePoints: WritePoints(itemsMost); break;
    case Stage::Profile: Profile(); break;
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
    case Stage::Profile: Worst_.ProfileMs = std::max(Worst_.ProfileMs, elapsedMs); break;
    case Stage::Done: break;
  }
  return Stage_ == Stage::Done;
}

std::expected<NetworkElevationJob::Result, std::string_view> NetworkElevationJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("network elevation is incomplete"); }
  return Result{.Graph = std::move(Network_), .Statistics = Statistics_};
}

}
