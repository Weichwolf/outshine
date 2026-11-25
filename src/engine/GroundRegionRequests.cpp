#include "PreparedGroundRegions.h"
#include "GroundQuality.h"
#include "TerrainLattice.h"
#include "EngineState.h"
#include "GroundRegionRequest.h"
#include "GroundRegionDefinition.h"
#include "BinaryValueArchive.h"
#include "TerrainRefinement.h"
#include "SourceProductSelection.h"
#include "TangentFrame.h"
#include <cmath>
#include <expected>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

namespace {
bool AppendClassificationMotion(const Ground::ClassificationPreparation &classification,
                                std::vector<uint8_t> &parameters) {
  const auto motion = classification.MotionCoverages();
  if (motion[0] == nullptr && motion[1] == nullptr) { return true; }
  constexpr uint64_t kClassMotionDefinition = 0x5353414c43544150;
  BinaryValueWriter writer(256);
  if (!writer(kClassMotionDefinition)) { return false; }
  for (const auto *tier : motion) {
    if (!writer(tier != nullptr)) { return false; }
    if (tier == nullptr) { continue; }
    const auto &box = tier->Layout;
    const auto &tiles = tier->Vectors.Window;
    if (!writer(box.WestM,
                box.SouthM,
                box.Width,
                box.Height,
                tiles.MinX,
                tiles.MaxX,
                tiles.MinY,
                tiles.MaxY)) {
      return false;
    }
  }
  parameters.append_range(writer.Out.Bytes());
  return true;
}
}

namespace {
bool IntersectsNearTerrain(const Box &bounds,
                           const TangentFrame &frame,
                           const Box &eyes,
                           double reachM) {
  Box local;
  for (unsigned corner = 0; corner < 8; ++corner) {
    const auto point = frame.ToLocalPosition(bounds.Corner(corner));
    local.Cover(Vec3{{point.EastM, point.UpM, -point.NorthM}});
  }
  return local.Min[0] <= eyes.Max[0] + reachM && local.Max[0] >= eyes.Min[0] - reachM &&
         local.Min[2] <= eyes.Max[2] + reachM && local.Max[2] >= eyes.Min[2] - reachM;
}
}

Generators::Osm::GroundTerrainErrorLimit
Engine::State::TerrainPackErrorLimit(const Around &coverage) const {
  if (!coverage.Sweep || !coverage.Sweep->EyePath || !Picture.Standing) { return {}; }
  const auto &eye =
      Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
  if (eye.Kind != Render::CameraKind::Perspective) { return {}; }
  const auto anchor = Session.Declared.Ground.Origin;
  const TangentFrame frame =
      TangentFrame::At({.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg});
  const auto path = ViewPathInEcef(*coverage.Sweep->EyePath, frame);
  const Vec2 viewport{
      {static_cast<double>(Picture.Frame.WidthPx), static_cast<double>(Picture.Frame.HeightPx)}};
  const ProjectedErrorBudget allowed{.FocalPx = viewport[1] / (2 * std::tan(eye.YfovRad * 0.5)),
                                     .AllowedErrorPx =
                                         Generators::TerrainRefinementDetail{}.ErrorPx};
  const auto eyes = coverage.Sweep->Eyes;
  const double exactReachM = FinestTerrainSampleRadiusM();
  return [frame, path, viewport, allowed, eyes, exactReachM](const Box &bounds) {
    if (IntersectsNearTerrain(bounds, frame, eyes, exactReachM)) { return -1.0; }
    return SourceProductSelection::GeometryErrorLimitM(bounds, path, viewport, allowed);
  };
}

Holds<std::vector<uint8_t>> Engine::State::GroundRegionParameters(const Around &coverage,
                                                                  GroundQuality quality) const {
  const auto anchor = Session.Declared.Ground.Origin;
  const auto eye =
      Picture.Standing->Watched() ? Picture.Standing->Watching() : Picture.Standing->Aimed();
  Generators::GroundRegionDefinition definition{
      .Coverage = coverage,
      .Anchor = {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg},
      .Quality = quality,
      .EyeM = eye.EyeM.Axis,
      .Projection = static_cast<Generators::GroundProjection>(std::to_underlying(eye.Kind)),
      .YfovRad = eye.YfovRad,
      .YMagM = eye.YMagM,
      .HeightPx = Picture.Frame.HeightPx,
      .ErrorPx = Generators::TerrainRefinementDetail{}.ErrorPx,
      .PagesMost = Render::TerrainLattice::kMaximumPages,
      .DeclaredDigest = std::nullopt};
  if (coverage.Sweep) { definition.EyeM = coverage.Sweep->Eyes.Middle().Axis; }
  if (!World.Surface().HasVectorSource() || World.Surface().HasDeclaredVectors()) {
    definition.DeclaredDigest = World.Surface().DeclaredDigest();
  }
  auto parameters = Generators::EncodeGroundRegionDefinition(definition);
  if (!parameters) { return parameters; }
  if (!AppendClassificationMotion(World.Surface().Classes(), *parameters)) {
    return std::unexpected("classification path exceeds its encoding limit");
  }
  return parameters;
}

bool Engine::State::CanPrimeGroundRegion(GroundQuality quality) const {
  if (quality != GroundQuality::Refined || World.GroundPublished.Current() ||
      !World.Surface().RegionAssets() || !Services.Compute || !Picture.Standing || !Session.Views ||
      (World.OsmSource &&
       World.OsmSource->CurrentPhase() != Generators::Osm::SourceAcquisition::Phase::Inactive) ||
      !World.OsmRoutes.empty() || !World.Surface().HasVectorSource() ||
      World.Surface().HasDeclaredVectors()) {
    return false;
  }
  const auto &view = Session.Views->Active();
  return view.Placement == Scenario::CameraPlacement::Local ||
         view.Placement == Scenario::CameraPlacement::Geodetic;
}

Holds<bool> Engine::State::PositionGroundRegionCamera(const Scenario::View &view) {
  if (view.Placement == Scenario::CameraPlacement::Geodetic) {
    const auto position = GeodeticCameraPosition(view);
    if (!position) { return std::unexpected(std::string(position.error())); }
    if (!*position) { return false; }
  }
  if (!UpdateActiveCamera()) { return std::unexpected(Error); }
  return true;
}

Holds<bool> Engine::State::PrimeGroundRegion(GroundQuality quality) {
  if (!Session.Views || !CanPrimeGroundRegion(quality)) { return true; }
  const auto positioned = PositionGroundRegionCamera(Session.Views->Active());
  if (!positioned || !*positioned) { return positioned; }
  const auto assets = World.Surface().RegionAssets();
  const auto focus = CurrentGeographicFocus();
  const auto coverage = RequestedTerrainCoverage(focus);
  const auto parameters = GroundRegionParameters(coverage, quality);
  if (!parameters) { return std::unexpected(parameters.error()); }
  const auto window = World.Surface().VectorWindowAt(focus, Ground::kVectorRing);
  if (!window) { return std::unexpected(std::string(window.error())); }
  auto request = assets->PlanRequest(*window,
                                     World.Surface().VectorZoom(),
                                     World.Surface().VectorSchema(),
                                     World.Surface().Pool().Shaped(),
                                     *parameters);
  if (!request) { return std::unexpected(request.error()); }
  if (*request != World.GroundLookupRequest) {
    World.Surface().ForgetCachedRegionNetwork();
    World.GroundLookupRequest = std::move(*request);
    World.GroundLookup = std::make_unique<Generators::GroundRegionRequest>(
        *Services.Compute,
        Generators::GroundRegionRequest::Lookup(
            assets, World.GroundLookupRequest, {}, false, TerrainPackErrorLimit(coverage)),
        World.Surface().RoadAssets());
  }
  if (!World.GroundLookup) { return true; }
  World.GroundLookup->Advance();
  const auto *result = World.GroundLookup->Peek();
  if (result == nullptr) { return false; }
  if (!*result) { return std::unexpected(result->error()); }
  if (result->value().Loaded) {
    World.Surface().UseCachedRegionNetwork(focus);
    const auto &classes = result->value().Loaded->Region.Classes;
    if (classes && World.Surface().Classes().Read() != classes &&
        World.Surface().RestoreClasses(classes)) {
      Published.RecordMetric("ground region: classes restored before fields", 1.0, "hit");
    }
  }
  return true;
}
}
