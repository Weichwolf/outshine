#include "TransportNetwork.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Earth.h"
#include "OsmField.h"

namespace outshine::World {
TransportNetworkBuildJob::TransportNetworkBuildJob(Path::Network &&graph)
    : Graph_(std::move(graph)) {}

std::expected<TransportNetworkBuildJob, std::string>
TransportNetworkBuildJob::Begin(const Ground::GroundStack &stack) {
  const auto began = std::chrono::steady_clock::now();
  const Ground::OsmField *const vectors = stack.Vectors();
  if (vectors == nullptr) { return std::unexpected("transport vectors are absent"); }
  auto created = Path::Network::Create(Path::Snap{.CellM = TransportNetwork::kNodeSnapM},
                                       Path::Sphere{.RadiusM = kWgs84A});
  if (!created) { return std::unexpected(std::string(created.error())); }
  TransportNetworkBuildJob job(std::move(*created));
  if (const auto laid = TransportNetwork::LayWays(stack.Ways(), vectors->Points(), job.Graph_);
      !laid) {
    return std::unexpected(std::string(laid.error()));
  }
  job.Built_.Ways = job.Graph_.WayCount();
  job.Built_.LayMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return job;
}

std::expected<void, std::string> TransportNetworkBuildJob::BeginWeave() {
  if (Built_.Ways == 0) {
    Stage_ = Stage::Crossings;
    return {};
  }
  auto started = Path::NetworkWeaveJob::Begin(std::move(Graph_));
  if (!started) { return std::unexpected(std::move(started.error())); }
  Weave_ = std::make_unique<Path::NetworkWeaveJob>(std::move(*started));
  Stage_ = Stage::Weave;
  return {};
}

std::expected<void, std::string> TransportNetworkBuildJob::AdvanceWeave(size_t itemsMost) {
  assert(Weave_ != nullptr);
  auto advanced = Weave_->Advance(itemsMost);
  if (!advanced) { return std::unexpected(std::move(advanced.error())); }
  if (!*advanced) { return {}; }
  Built_.WeaveSlices = Weave_->LongestSlices();
  auto woven = std::move(*Weave_).Take();
  if (!woven) { return std::unexpected(std::string(woven.error())); }
  Weave_.reset();
  Graph_ = std::move(*woven);
  Stage_ = Stage::Crossings;
  return {};
}

std::expected<void, std::string> TransportNetworkBuildJob::ClassifyCrossings() {
  std::vector<Path::Network::Crossing> crossings;
  if (auto swept = Graph_.Crossings(crossings); !swept) {
    return std::unexpected(std::string(swept.error()));
  }
  Built_.Nodes = Graph_.NodeCount();
  Built_.Edges = Graph_.EdgeCount();
  Built_.Junctions = Graph_.JunctionCount();
  Stage_ = Stage::BeginElevation;
  return {};
}

void TransportNetworkBuildJob::BeginElevation(const Ground::GroundStack &stack) {
  Elevation_ = std::make_unique<Path::NetworkElevationJob>(Path::NetworkElevationJob::Begin(
      std::move(Graph_), [&stack](LongitudeLatitude at) { return stack.Ground().At(at).AslM(); }));
  Stage_ = Stage::Elevation;
}

std::expected<void, std::string> TransportNetworkBuildJob::AdvanceElevation(size_t itemsMost) {
  assert(Elevation_ != nullptr);
  auto advanced = Elevation_->Advance(itemsMost);
  if (!advanced) { return std::unexpected(std::string(advanced.error())); }
  if (!*advanced) { return {}; }
  Built_.ElevationSlices = Elevation_->LongestSlices();
  auto elevated = std::move(*Elevation_).Take();
  if (!elevated) { return std::unexpected(std::string(elevated.error())); }
  Elevation_.reset();
  Graph_ = std::move(elevated->Graph);
  Built_.Elevated = elevated->Statistics;
  Stage_ = Stage::Publish;
  return {};
}

void TransportNetworkBuildJob::Publish() {
  Built_.Graph = std::make_shared<Path::Network>(std::move(Graph_));
  Stage_ = Stage::Done;
}

std::expected<bool, std::string> TransportNetworkBuildJob::Advance(const Ground::GroundStack &stack,
                                                                   size_t itemsMost) {
  if (itemsMost == 0) { return std::unexpected("transport network work budget is zero"); }
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  std::expected<void, std::string> progressed;
  switch (Stage_) {
    case Stage::BeginWeave: progressed = BeginWeave(); break;
    case Stage::Weave: progressed = AdvanceWeave(itemsMost); break;
    case Stage::Crossings: progressed = ClassifyCrossings(); break;
    case Stage::BeginElevation: BeginElevation(stack); break;
    case Stage::Elevation: progressed = AdvanceElevation(itemsMost); break;
    case Stage::Publish: Publish(); break;
    case Stage::Done: return true;
  }
  if (!progressed) { return std::unexpected(std::move(progressed.error())); }
  const double elapsedMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  LongestSliceMs_ = std::max(LongestSliceMs_, elapsedMs);
  switch (before) {
    case Stage::BeginWeave:
    case Stage::Weave:
      Built_.WeaveMs += elapsedMs;
      Built_.WeaveLongestMs = std::max(Built_.WeaveLongestMs, elapsedMs);
      break;
    case Stage::Crossings: Built_.CrossingsMs += elapsedMs; break;
    case Stage::BeginElevation:
    case Stage::Elevation:
      Built_.ElevateMs += elapsedMs;
      Built_.ElevateLongestMs = std::max(Built_.ElevateLongestMs, elapsedMs);
      break;
    case Stage::Publish:
    case Stage::Done: break;
  }
  return Stage_ == Stage::Done;
}

std::expected<TransportNetwork::Built, std::string_view> TransportNetworkBuildJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("transport network is incomplete"); }
  return std::move(Built_);
}

}
