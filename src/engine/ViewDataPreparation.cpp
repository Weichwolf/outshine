#include "EngineHeld.h"
#include "TerrainPathPreparation.h"
#include "RoadHeightCoverage.h"
#include "GeodeticCamera.h"
#include <chrono>
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>
#include <cmath>
#include <expected>
#include <span>
#include <vector>

namespace outshine {
namespace {
using Clock = std::chrono::steady_clock;
constexpr double kMostAwaitS = 0.01;
}

Holds<LongitudeLatitude> Engine::State::PreparationFocus(const Scenario::View &view,
                                                         Clock::time_point deadline) const {
  if (view.Placement == Scenario::CameraPlacement::Local) {
    return GeographicFocusFor(view.Sees.PositionM + view.OffsetM);
  }
  if (view.Placement != Scenario::CameraPlacement::Geodetic) { return CurrentGeographicFocus(); }
  const LongitudeLatitude origin{.LongitudeDeg = Session.Declared.Ground.Origin.LongitudeDeg,
                                 .LatitudeDeg = Session.Declared.Ground.Origin.LatitudeDeg};
  for (;;) {
    const auto position = ResolveGeodeticCamera(view, origin, &World.Stack.Ground());
    if (!position) { return std::unexpected(std::string(position.error())); }
    if (*position) { return GeographicFocusFor(**position); }
    const double remaining = std::chrono::duration<double>(deadline - Clock::now()).count();
    if (remaining <= 0.0) {
      return std::unexpected(
          "view data preparation timed out resolving the geodetic camera height");
    }
    static_cast<void>(World.Stack.Pool().AwaitLanding(std::min(remaining, kMostAwaitS)));
  }
}

Holds<std::vector<Around>> Engine::State::ViewPreparationPath(const Scenario::View &view,
                                                              double durationS,
                                                              Clock::time_point deadline) const {
  const double step = Session.Declared.Motion.StepS;
  if (!std::isfinite(step) || step <= 0.0) {
    return std::unexpected("view data preparation requires a finite positive simulation step");
  }
  const bool route = view.Placement == Scenario::CameraPlacement::Route;
  if (!route && view.Placement != Scenario::CameraPlacement::Geodetic &&
      view.Placement != Scenario::CameraPlacement::Local) {
    return std::unexpected("view data preparation requires a static or route camera");
  }
  const double count = std::ceil(durationS / step);
  if (route &&
      (!std::isfinite(count) || count >= static_cast<double>(TerrainPathPlan::MaximumPoints))) {
    return std::unexpected("view data preparation exceeds its camera sample budget");
  }
  std::vector<Around> path;
  const size_t points = route ? static_cast<size_t>(count) + 1u : 1u;
  path.reserve(points);
  const auto stationary = PreparationFocus(view, deadline);
  if (!stationary) { return std::unexpected(stationary.error()); }
  double time = Ticking.ElapsedS;
  for (size_t point = 0; point < points; ++point) {
    LongitudeLatitude at = *stationary;
    if (route) {
      const auto sample = SampleRouteCamera(view, time);
      if (!sample) { return std::unexpected(sample.error()); }
      at = GeographicFocusFor(sample->EyeM);
    }
    path.push_back(TerrainCoverageAt(at));
    time += step;
  }
  return path;
}

Result Engine::prepareViewData(double durationS, double patienceS) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!std::isfinite(durationS) || durationS < 0.0 || !std::isfinite(patienceS) ||
      patienceS < 0.0) {
    return std::unexpected("view data preparation requires finite nonnegative duration and budget");
  }
  const auto began = Clock::now();
  if (!S_->Session.Declared.Ground.Declared) { return {}; }
  if (!S_->World.Stack.Opened() || !S_->Picture.Standing || !S_->Session.Views) {
    return std::unexpected("view data preparation requires assembled ground and an active view");
  }
  const double mostBudget = std::chrono::duration<double>(Clock::time_point::max() - began).count();
  if (patienceS >= mostBudget) {
    return std::unexpected("view data budget exceeds the steady-clock range");
  }
  const auto &view = S_->Session.Views->Active();
  const auto deadline =
      began + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(patienceS));
  auto path = S_->ViewPreparationPath(view, durationS, deadline);
  if (!path) { return std::unexpected(path.error()); }
  std::vector<Data::TileId> roadTiles;
  if (S_->World.OsmTransportLoader && S_->World.OsmTransportLoader->Current()) {
    const auto coverage =
        RoadHeightCoverage::Select(*S_->World.OsmTransportLoader->Current(),
                                   {.Zoom = path->front().Zoom,
                                    .MaximumEdges = RoadHeightCoverage::MaximumCandidateEdges,
                                    .MaximumTiles = RoadHeightCoverage::MaximumCandidateTiles});
    if (!coverage) { return std::unexpected(coverage.error()); }
    roadTiles = coverage->Tiles;
  }
  const auto classification = S_->World.Stack.Classes().RequestWindows();
  const auto classWindows = S_->World.Stack.Classes().HasSourceRequests()
                                ? std::span<const Ground::ClassField::SourceWindow>(classification)
                                : std::span<const Ground::ClassField::SourceWindow>();
  const bool vectors = S_->World.Stack.HasVectorSource() && !S_->World.Stack.HasDeclaredVectors() &&
                       S_->World.Stack.Vegetated();
  auto plan = PlanTerrainPath(*path,
                              S_->World.Stack.Ground(),
                              {.VectorZoom = vectors ? S_->World.Stack.VectorZoom() : -1,
                               .Classification = classWindows,
                               .RoadTiles = roadTiles});
  if (!plan) { return std::unexpected(plan.error()); }
  S_->Published.Places("view data: camera samples", static_cast<double>(path->size()), "points");
  S_->Published.Places(
      "view data: planned fields", static_cast<double>(plan->Fields.size()), "tiles");
  S_->Published.Places(
      "view data: planned vectors", static_cast<double>(plan->Vectors.size()), "tiles");

  return PrepareTerrainPath(std::move(*plan), S_->World.Stack, deadline);
}
}
