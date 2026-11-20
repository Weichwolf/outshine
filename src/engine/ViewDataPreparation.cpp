#include "EngineState.h"
#include "TerrainPathPreparation.h"
#include "RoadHeightCoverage.h"
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
  for (;;) {
    const auto position = GeodeticCameraPosition(view);
    if (!position) { return std::unexpected(std::string(position.error())); }
    if (*position) { return GeographicFocusFor(**position); }
    const double remaining = std::chrono::duration<double>(deadline - Clock::now()).count();
    if (remaining <= 0.0) {
      return std::unexpected(
          "view data preparation timed out resolving the geodetic camera height");
    }
    static_cast<void>(World.Surface().Pool().AwaitLanding(std::min(remaining, kMostAwaitS)));
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
  return prepareViewData(durationS, patienceS, {});
}

Result Engine::prepareCameraPathData(std::span<const Vec3> positionsM, double patienceS) {
  if (positionsM.empty() || positionsM.size() > TerrainPathPlan::MaximumPoints) {
    return std::unexpected("view data path has an invalid point count");
  }
  return prepareViewData(0.0, patienceS, positionsM);
}

Result
Engine::prepareViewData(double durationS, double patienceS, std::span<const Vec3> positionsM) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) { return permission; }
  if (!std::isfinite(durationS) || durationS < 0.0 || !std::isfinite(patienceS) ||
      patienceS < 0.0) {
    return std::unexpected("view data preparation requires finite nonnegative duration and budget");
  }
  const auto began = Clock::now();
  if (!S_->Session.Declared.Ground.Declared) { return {}; }
  if (!S_->World.Surface().Opened() || !S_->Picture.Standing || !S_->Session.Views) {
    return std::unexpected("view data preparation requires assembled ground and an active view");
  }
  const double mostBudget = std::chrono::duration<double>(Clock::time_point::max() - began).count();
  if (patienceS >= mostBudget) {
    return std::unexpected("view data budget exceeds the steady-clock range");
  }
  const auto &view = S_->Session.Views->Active();
  const auto deadline =
      began + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(patienceS));
  auto path = [&] -> Holds<std::vector<Around>> {
    if (positionsM.empty()) { return S_->ViewPreparationPath(view, durationS, deadline); }
    std::vector<Around> coverage;
    coverage.reserve(positionsM.size());
    for (const Vec3 &position : positionsM) {
      if (!std::ranges::all_of(position.Axis, [](double value) { return std::isfinite(value); })) {
        return std::unexpected("view data path has a nonfinite camera position");
      }
      coverage.push_back(S_->TerrainCoverageAt(S_->GeographicFocusFor(position)));
    }
    return coverage;
  }();
  if (!path) { return std::unexpected(path.error()); }
  std::vector<Data::TileId> roadTiles;
  if (S_->World.OsmTransport && S_->World.OsmTransport->Current()) {
    const auto coverage =
        RoadHeightCoverage::Select(*S_->World.OsmTransport->Current(),
                                   {.Zoom = path->front().Zoom,
                                    .MaximumEdges = RoadHeightCoverage::MaximumCandidateEdges,
                                    .MaximumTiles = RoadHeightCoverage::MaximumCandidateTiles});
    if (!coverage) { return std::unexpected(coverage.error()); }
    roadTiles = coverage->Tiles;
  }
  const auto classification = S_->World.Surface().Classes().RequestWindows();
  const auto classWindows =
      S_->World.Surface().Classes().HasSourceRequests()
          ? std::span<const Ground::ClassificationPreparation::SourceWindow>(classification)
          : std::span<const Ground::ClassificationPreparation::SourceWindow>();
  const bool vectors = S_->World.Surface().HasVectorSource() &&
                       !S_->World.Surface().HasDeclaredVectors() &&
                       S_->World.Surface().RecipesReady();
  auto plan = PlanTerrainPath(*path,
                              S_->World.Surface().Ground(),
                              {.VectorZoom = vectors ? S_->World.Surface().VectorZoom() : -1,
                               .Classification = classWindows,
                               .RoadTiles = roadTiles});
  if (!plan) { return std::unexpected(plan.error()); }
  S_->Published.RecordMetric(
      "view data: camera samples", static_cast<double>(path->size()), "points");
  S_->Published.RecordMetric(
      "view data: planned fields", static_cast<double>(plan->Fields.size()), "tiles");
  S_->Published.RecordMetric(
      "view data: planned vectors", static_cast<double>(plan->Vectors.size()), "tiles");

  auto prepared = PrepareTerrainPath(std::move(*plan), S_->World.Surface(), deadline);
  const auto *terrain = S_->World.TerrainSurface.PublishedSurface();
  if (!prepared || !S_->World.UrbanAreas || terrain == nullptr || positionsM.empty()) {
    return prepared;
  }
  return S_->World.UrbanAreas->PrepareMotion(positionsM,
                                             *terrain,
                                             S_->World.Rebuilds,
                                             S_->World.BuildingCoverage.Regions(),
                                             S_->World.BuildingCoverage.Version(),
                                             deadline);
}
}
