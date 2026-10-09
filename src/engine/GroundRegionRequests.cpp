#include "EngineHeld.h"
#include "BinaryValueArchive.h"
#include "GroundLattice.h"
#include "TerrainRefinement.h"
#include "GeodeticCamera.h"
#include <expected>
#include <cstdint>
#include <memory>
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
  BinaryValueWriter parameters(4096);
  if (!parameters(coverage.LatitudeDeg,
                  coverage.LongitudeDeg,
                  coverage.Zoom,
                  coverage.Levels,
                  coverage.Grid,
                  coverage.PlayableOnly,
                  quality,
                  anchor.LatitudeDeg,
                  anchor.LongitudeDeg,
                  eye.EyeM.Axis,
                  eye.Kind,
                  eye.YfovRad,
                  eye.YMagM,
                  Picture.Frame.HeightPx,
                  Generators::TerrainRefinementDetail{}.ErrorPx,
                  Render::GroundLattice::kMaximumPages)) {
    return std::unexpected("ground region parameters exceed their encoding limit");
  }
  return std::move(parameters.Out).TakeBytes();
}

bool Engine::State::CanPrimeGroundRegion(GroundQuality quality) const {
  if (quality != GroundQuality::Refined || World.GroundPublished.Current() ||
      !World.Stack.RegionAssets() || !World.Pool || !Picture.Standing || !Session.Views ||
      (World.OsmSource &&
       World.OsmSource->CurrentPhase() != Generators::Osm::SourceAcquisition::Phase::Inactive) ||
      !World.OsmRoutes.empty() || !World.Stack.HasVectorSource() ||
      World.Stack.HasDeclaredVectors()) {
    return false;
  }
  const auto &view = Session.Views->Active();
  return view.Placement == Scenario::CameraPlacement::Local ||
         view.Placement == Scenario::CameraPlacement::Geodetic;
}

Holds<bool> Engine::State::PositionGroundRegionCamera(const Scenario::View &view) {
  if (view.Placement == Scenario::CameraPlacement::Geodetic) {
    const auto anchor = Session.Declared.Ground.Origin;
    const auto position = ResolveGeodeticCamera(
        view,
        {.LongitudeDeg = anchor.LongitudeDeg, .LatitudeDeg = anchor.LatitudeDeg},
        &World.Stack.Ground());
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
  const auto assets = World.Stack.RegionAssets();
  const auto focus = CurrentGeographicFocus();
  const auto coverage = TerrainCoverageAt(focus);
  const auto parameters = GroundRegionParameters(coverage, quality);
  if (!parameters) { return std::unexpected(parameters.error()); }
  auto request = assets->PlanRequest(focus,
                                     World.Stack.VectorZoom(),
                                     World.Stack.VectorSchema(),
                                     Ground::kVectorRing,
                                     World.Stack.Pool().Shaped(),
                                     *parameters);
  if (!request) { return std::unexpected(request.error()); }
  if (*request != World.GroundLookupRequest) {
    World.Stack.ForgetCachedRegionNetwork();
    World.GroundLookupRequest = std::move(*request);
    World.GroundLookup = std::make_unique<GroundRegionPreparation>(
        *World.Pool,
        [assets, key = World.GroundLookupRequest](const std::stop_token &stop)
            -> std::expected<GroundRegionPreparation::Completed, std::string> {
          if (stop.stop_requested()) { return std::unexpected("ground region lookup canceled"); }
          auto loaded = assets->LoadRequest(key);
          if (!loaded) { return std::unexpected(std::move(loaded.error())); }
          const auto product = *loaded ? (**loaded).Key : std::string{};
          return GroundRegionPreparation::Completed{
              .Key = product, .Loaded = std::move(*loaded), .RequestKey = key, .RequestHit = true};
        });
  }
  if (!World.GroundLookup) { return true; }
  const auto *result = World.GroundLookup->Peek();
  if (result == nullptr) { return false; }
  if (!*result) { return std::unexpected(result->error()); }
  if (result->value().Loaded) {
    World.Stack.UseCachedRegionNetwork(focus);
    const auto &classes = result->value().Loaded->Region.Classes;
    if (classes && World.Stack.Classes().Read() != classes && World.Stack.RestoreClasses(classes)) {
      Published.RecordMetric("ground region: classes restored before fields", 1.0, "hit");
    }
  }
  return true;
}
}
