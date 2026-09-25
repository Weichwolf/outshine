#include "VectorStreetGraph.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <limits>
#include <ratio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Earth.h"
#include "OsmField.h"

namespace outshine::Ground {
VectorStreetGraphBuildJob::VectorStreetGraphBuildJob(Path::Network &&graph,
                                                     Path::Network::HeightSource heightOf)
    : Graph_(std::move(graph)), HeightOf_(std::move(heightOf)) {}

std::expected<VectorStreetGraphBuildJob, std::string>
VectorStreetGraphBuildJob::Begin(const Ground::GroundStack &stack,
                                 Path::Network::HeightSource heightOf) {
  const auto began = std::chrono::steady_clock::now();
  const Ground::OsmField *const vectors = stack.Vectors();
  if (vectors == nullptr) { return std::unexpected("vector streets are absent"); }
  auto created = Path::Network::Create(Path::Snap{.CellM = VectorStreetGraph::kNodeSnapM},
                                       Path::Sphere{.RadiusM = kWgs84A});
  if (!created) { return std::unexpected(std::string(created.error())); }
  if (!heightOf) { return std::unexpected("street corridor height source is absent"); }
  VectorStreetGraphBuildJob job(std::move(*created), std::move(heightOf));
  if (const auto laid = VectorStreetGraph::LayWays(stack.Ways(), vectors->Points(), job.Graph_);
      !laid) {
    return std::unexpected(std::string(laid.error()));
  }
  job.Built_.Ways = job.Graph_.WayCount();
  job.Built_.LayMs =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  return job;
}

std::expected<void, std::string> VectorStreetGraphBuildJob::BeginWeave() {
  if (Built_.Ways == 0) {
    Stage_ = Stage::BeginCrossings;
    return {};
  }
  auto started = Path::NetworkWeaveJob::Begin(std::move(Graph_));
  if (!started) { return std::unexpected(std::move(started.error())); }
  Weave_ = std::make_unique<Path::NetworkWeaveJob>(std::move(*started));
  Stage_ = Stage::Weave;
  return {};
}

std::expected<void, std::string> VectorStreetGraphBuildJob::AdvanceWeave(size_t itemsMost) {
  assert(Weave_ != nullptr);
  auto advanced = Weave_->Advance(itemsMost);
  if (!advanced) { return std::unexpected(std::move(advanced.error())); }
  if (!*advanced) { return {}; }
  Built_.WeaveSlices = Weave_->LongestSlices();
  auto woven = std::move(*Weave_).Take();
  if (!woven) { return std::unexpected(std::string(woven.error())); }
  Graph_ = std::move(*woven);
  Stage_ = Stage::CleanupWeave;
  return {};
}

std::expected<void, std::string> VectorStreetGraphBuildJob::CleanupWeave(size_t itemsMost) {
  assert(Weave_ != nullptr);
  constexpr size_t kCleanupItemsPerBuildItem = 16;
  const size_t cleanupItemsMost =
      itemsMost > std::numeric_limits<size_t>::max() / kCleanupItemsPerBuildItem
          ? std::numeric_limits<size_t>::max()
          : itemsMost * kCleanupItemsPerBuildItem;
  auto released = Weave_->ReleaseTemporary(cleanupItemsMost);
  if (!released) { return std::unexpected(std::string(released.error())); }
  if (*released) {
    Weave_.reset();
    Stage_ = Stage::BeginCrossings;
  }
  return {};
}

std::expected<void, std::string> VectorStreetGraphBuildJob::BeginCrossings() {
  auto started = Path::NetworkCrossingJob::Begin(std::move(Graph_));
  if (!started) { return std::unexpected(std::string(started.error())); }
  Crossings_ = std::make_unique<Path::NetworkCrossingJob>(std::move(*started));
  Stage_ = Stage::Crossings;
  return {};
}

std::expected<void, std::string> VectorStreetGraphBuildJob::AdvanceCrossings(size_t pairsMost) {
  assert(Crossings_ != nullptr);
  constexpr size_t kCrossingPairsPerBuildItem = 128;
  const size_t crossingPairsMost =
      pairsMost > std::numeric_limits<size_t>::max() / kCrossingPairsPerBuildItem
          ? std::numeric_limits<size_t>::max()
          : pairsMost * kCrossingPairsPerBuildItem;
  auto advanced = Crossings_->Advance(crossingPairsMost);
  if (!advanced) { return std::unexpected(std::string(advanced.error())); }
  if (!*advanced) { return {}; }
  Built_.CrossingSlices = Crossings_->LongestSlices();
  auto crossed = std::move(*Crossings_).Take();
  if (!crossed) { return std::unexpected(std::string(crossed.error())); }
  Crossings_.reset();
  Graph_ = std::move(crossed->Graph);
  Built_.CrossingSweep = crossed->Statistics;
  Built_.Nodes = Graph_.NodeCount();
  Built_.Edges = Graph_.EdgeCount();
  Built_.Junctions = Graph_.JunctionCount();
  Stage_ = Stage::BeginElevation;
  return {};
}

void VectorStreetGraphBuildJob::BeginElevation() {
  Elevation_ = std::make_unique<Path::NetworkElevationJob>(
      Path::NetworkElevationJob::Begin(std::move(Graph_), std::move(HeightOf_)));
  Stage_ = Stage::Elevation;
}

std::expected<void, std::string> VectorStreetGraphBuildJob::AdvanceElevation(size_t itemsMost) {
  assert(Elevation_ != nullptr);
  constexpr size_t kProfileItemsPerSample = 4;
  const size_t profileItemsMost =
      itemsMost > std::numeric_limits<size_t>::max() / kProfileItemsPerSample
          ? std::numeric_limits<size_t>::max()
          : itemsMost * kProfileItemsPerSample;
  auto advanced =
      Elevation_->Advance({.SamplesMost = itemsMost, .ProfilePointsMost = profileItemsMost});
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

void VectorStreetGraphBuildJob::Publish() {
  Built_.Graph = std::make_shared<Path::Network>(std::move(Graph_));
  Stage_ = Stage::Done;
}

const char *VectorStreetGraphBuildJob::PhaseName() const noexcept {
  switch (Stage_) {
    case Stage::BeginWeave: return "begin-weave";
    case Stage::Weave: return "weave";
    case Stage::CleanupWeave: return "cleanup-weave";
    case Stage::BeginCrossings: return "begin-crossings";
    case Stage::Crossings: return "crossings";
    case Stage::BeginElevation: return "begin-elevation";
    case Stage::Elevation: return "elevation";
    case Stage::Publish: return "publish";
    case Stage::Done: return "done";
  }
  return "unknown";
}

std::expected<bool, std::string> VectorStreetGraphBuildJob::Advance(size_t itemsMost) {
  if (itemsMost == 0) { return std::unexpected("street corridor work budget is zero"); }
  const auto began = std::chrono::steady_clock::now();
  const Stage before = Stage_;
  std::expected<void, std::string> progressed;
  switch (Stage_) {
    case Stage::BeginWeave: progressed = BeginWeave(); break;
    case Stage::Weave: progressed = AdvanceWeave(itemsMost); break;
    case Stage::CleanupWeave: progressed = CleanupWeave(itemsMost); break;
    case Stage::BeginCrossings: progressed = BeginCrossings(); break;
    case Stage::Crossings: progressed = AdvanceCrossings(itemsMost); break;
    case Stage::BeginElevation: BeginElevation(); break;
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
      Built_.BeginWeaveMs = elapsedMs;
      Built_.WeaveMs += elapsedMs;
      Built_.WeaveLongestMs = std::max(Built_.WeaveLongestMs, elapsedMs);
      break;
    case Stage::Weave:
      Built_.WeaveMs += elapsedMs;
      Built_.WeaveLongestMs = std::max(Built_.WeaveLongestMs, elapsedMs);
      break;
    case Stage::CleanupWeave:
      Built_.CleanupWeaveMs += elapsedMs;
      Built_.CleanupWeaveLongestMs = std::max(Built_.CleanupWeaveLongestMs, elapsedMs);
      break;
    case Stage::BeginCrossings:
    case Stage::Crossings:
      Built_.CrossingsMs += elapsedMs;
      Built_.CrossingsLongestMs = std::max(Built_.CrossingsLongestMs, elapsedMs);
      break;
    case Stage::BeginElevation:
      Built_.BeginElevationMs = elapsedMs;
      Built_.ElevateMs += elapsedMs;
      Built_.ElevateLongestMs = std::max(Built_.ElevateLongestMs, elapsedMs);
      break;
    case Stage::Elevation:
      Built_.ElevateMs += elapsedMs;
      Built_.ElevateLongestMs = std::max(Built_.ElevateLongestMs, elapsedMs);
      break;
    case Stage::Publish: Built_.PublishMs = elapsedMs; break;
    case Stage::Done: break;
  }
  return Stage_ == Stage::Done;
}

std::expected<VectorStreetGraph::Built, std::string_view> VectorStreetGraphBuildJob::Take() && {
  if (Stage_ != Stage::Done) { return std::unexpected("street corridor graph is incomplete"); }
  return std::move(Built_);
}

}
