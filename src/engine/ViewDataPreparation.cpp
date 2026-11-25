#include "EngineState.h"
#include "Box.h"
#include "GroundPatchwork.h"
#include "RuntimeScene.h"
#include "TerrainPathPreparation.h"
#include "RoadHeightCoverage.h"
#include "ViewPath.h"
#include <chrono>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <cmath>
#include <expected>
#include <span>
#include <tuple>
#include <vector>

namespace outshine {
namespace {
using Clock = std::chrono::steady_clock;
constexpr double kMostAwaitS = 0.01;

struct CameraMotion {
  Box Eyes;
  ViewPath Path;
};

CameraMotion PlanCameraMotion(std::span<const Vec3> positions) {
  CameraMotion motion{.Eyes = {}, .Path = {.Start = positions.front(), .End = positions.back()}};
  for (const Vec3 position : positions) {
    motion.Eyes.Cover(position);
    motion.Path.RadiusM = std::max(motion.Path.RadiusM, motion.Path.DistanceTo(position));
  }
  double roundoff = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    const double guard =
        64 * std::numeric_limits<double>::epsilon() *
        std::max({std::abs(motion.Eyes.Min[axis]), std::abs(motion.Eyes.Max[axis]), 1.0});
    motion.Eyes.Min[axis] -= guard;
    motion.Eyes.Max[axis] += guard;
    roundoff += guard;
  }
  motion.Path.RadiusM += roundoff;
  return motion;
}

std::shared_ptr<TerrainSweep> PlanMotionTerrain(std::span<const Around> coverage,
                                                const CameraMotion &motion,
                                                std::vector<Data::TileId> requested,
                                                double sampleRadiusM) {
  const auto initial = PlanPatchworkTiles(coverage.front());
  if (!initial || *initial == requested) { return {}; }
  if (std::ranges::all_of(coverage, [&](const Around &at) {
        return PatchworkCoversFocus(coverage.front(), at, sampleRadiusM);
      })) {
    return {};
  }
  const int levels = std::ranges::max(coverage, {}, &Around::Levels).Levels;
  auto patches = RemoveCoveredPatchworkTiles(requested);
  return std::make_shared<TerrainSweep>(
      TerrainSweep{.Requested = std::move(requested),
                   .Patches = std::move(patches),
                   .Focus = {.LongitudeDeg = coverage.front().LongitudeDeg,
                             .LatitudeDeg = coverage.front().LatitudeDeg},
                   .Eyes = motion.Eyes,
                   .EyePath = motion.Path,
                   .Levels = levels});
}
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

Holds<Camera> Engine::prepareCameraPose(double patienceS) {
  [[maybe_unused]] const auto logs = S_->Logs();
  if (const auto permission = S_->MutationPermission(); !permission) {
    return std::unexpected(permission.error());
  }
  if (!S_->Session.Views || !S_->Picture.Standing) {
    return std::unexpected("camera pose preparation requires an active view");
  }
  const auto at = Clock::now();
  const double most = std::chrono::duration<double>(Clock::time_point::max() - at).count();
  if (!std::isfinite(patienceS) || patienceS < 0.0 || patienceS >= most) {
    return std::unexpected(
        "camera pose preparation requires a finite budget within the clock range");
  }
  if (S_->Session.Declared.Ground.Declared) {
    const auto deadline =
        at + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(patienceS));
    const auto focus = S_->PreparationFocus(S_->Session.Views->Active(), deadline);
    if (!focus) { return std::unexpected(focus.error()); }
  }
  if (!S_->UpdateActiveCamera()) { return std::unexpected(S_->Error); }
  Camera pose;
  const auto &scene = *S_->Picture.Standing;
  Render::CameraOf(scene.Watched() ? scene.Watching() : scene.Aimed(), pose);
  return pose;
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

  return S_->PrepareViewTerrain(*path, positionsM, std::move(*plan), deadline);
}

Result Engine::State::PrepareViewTerrain(std::span<const Around> coverage,
                                         std::span<const Vec3> positions,
                                         TerrainPathPlan plan,
                                         Clock::time_point deadline) {
  if (!positions.empty() && !World.GroundPublished.Current()) {
    const auto motion = PlanCameraMotion(positions);
    World.CachedCameraSweepPending = false;
    PlanCameraSourceCoverage(coverage);
    World.PlannedCameraPath = motion.Path;
    const auto &origin = Session.Declared.Ground.Origin;
    World.PlannedTravelEcef = ViewPathInEcef(
        motion.Path,
        TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg, .LatitudeDeg = origin.LatitudeDeg}));
    InstallTerrainSweep(
        PlanMotionTerrain(coverage, motion, std::move(plan.Meshes), FinestTerrainSampleRadiusM()));
    return {};
  }
  const auto prepared = PrepareTerrainPath(std::move(plan), World.Surface(), deadline);
  const auto *terrain = World.TerrainSurface.PublishedSurface();
  if (!prepared || !World.UrbanAreas || terrain == nullptr || positions.empty()) {
    return prepared;
  }
  return World.UrbanAreas->PrepareMotion(positions,
                                         *terrain,
                                         World.Rebuilds,
                                         World.BuildingCoverage.Regions(),
                                         World.BuildingCoverage.Version(),
                                         deadline);
}

void Engine::State::PlanCameraSourceCoverage(std::span<const Around> coverage) {
  std::vector<LongitudeLatitude> route;
  route.reserve(coverage.size());
  for (const auto &at : coverage) {
    route.push_back({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg});
  }
  World.Surface().PlanMotionCoverage(route);
}

void Engine::State::InstallTerrainSweep(std::shared_ptr<TerrainSweep> sweep) {
  if (sweep) {
    constexpr uint64_t kSweptTerrainRegion = uint64_t{1} << 63u;
    sweep->Region = kSweptTerrainRegion | ++World.NextTerrainSweep;
    Published.RecordMetric(
        "view data: swept terrain patches", static_cast<double>(sweep->Patches.size()), "tiles");
  }
  World.PlannedTerrainSweep = std::move(sweep);
}

void Engine::State::PlanCachedCameraDemand() {
  if (World.PlannedCameraPath) { return; }
  const auto &view =
      Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
  const auto eye = view.EyeM;
  const double widthM = FinestTerrainSampleHalfWidthM();
  std::array<Vec3, 10> positions{};
  positions.front() = positions.back() = eye;
  for (size_t corner = 0; corner < 8; ++corner) {
    for (size_t axis = 0; axis < 3; ++axis) {
      const bool positive = (corner & (1u << (2u - axis))) != 0;
      positions[corner + 1][axis] = eye[axis] + (positive ? widthM : -widthM);
    }
  }
  const auto motion = PlanCameraMotion(positions);
  World.PlannedCameraPath = motion.Path;
  std::array<Around, positions.size()> coverage;
  for (size_t at = 0; at < positions.size(); ++at) {
    coverage[at] = TerrainCoverageAt(GeographicFocusFor(positions[at]));
  }
  const auto initial = PlanPatchworkTiles(coverage.front());
  if (!initial) { return; }
  auto tiles = *initial;
  for (const auto &window : std::span(coverage).subspan(1)) {
    const auto nearby = PlanPatchworkTiles(window);
    if (!nearby) { return; }
    for (const auto tile : *nearby) {
      if (tile.Zoom == coverage.front().Zoom) { tiles.push_back(tile); }
    }
  }
  std::ranges::sort(
      tiles, {}, [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); });
  tiles.erase(std::ranges::unique(tiles).begin(), tiles.end());
  auto sweep = PlanMotionTerrain(coverage, motion, std::move(tiles), FinestTerrainSampleRadiusM());
  if (!sweep) { return; }
  PlanCameraSourceCoverage(coverage);
  InstallTerrainSweep(std::move(sweep));
  World.CachedCameraSweepPending = true;
}
}
