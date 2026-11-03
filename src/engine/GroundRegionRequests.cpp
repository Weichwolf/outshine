#include "GroundQuality.h"
#include "TerrainLattice.h"
#include "EngineState.h"
#include "GroundRegionRequest.h"
#include "GroundRegionDefinition.h"
#include "TerrainRefinement.h"
#include <expected>
#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine {

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
  if (!World.Surface().HasVectorSource() || World.Surface().HasDeclaredVectors()) {
    definition.DeclaredDigest = World.Surface().DeclaredDigest();
  }
  return Generators::EncodeGroundRegionDefinition(definition);
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
  const auto coverage = TerrainCoverageAt(focus);
  const auto parameters = GroundRegionParameters(coverage, quality);
  if (!parameters) { return std::unexpected(parameters.error()); }
  auto request = assets->PlanRequest(focus,
                                     World.Surface().VectorZoom(),
                                     World.Surface().VectorSchema(),
                                     Ground::kVectorRing,
                                     World.Surface().Pool().Shaped(),
                                     *parameters);
  if (!request) { return std::unexpected(request.error()); }
  if (*request != World.GroundLookupRequest) {
    World.Surface().ForgetCachedRegionNetwork();
    World.GroundLookupRequest = std::move(*request);
    World.GroundLookup = std::make_unique<Generators::GroundRegionRequest>(
        *Services.Compute,
        Generators::GroundRegionRequest::Lookup(assets, World.GroundLookupRequest),
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
