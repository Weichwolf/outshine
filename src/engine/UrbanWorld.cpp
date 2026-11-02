#include "EngineState.h"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine {
void Engine::State::PublishUrbanMeasurements() {
  if (!World.UrbanAreas) { return; }
  const auto urban = World.UrbanAreas->Costs();
  Published.RecordMetric("urban sources", static_cast<double>(urban.Sources), "areas");
  Published.RecordMetric("urban requested groups", static_cast<double>(urban.Groups), "groups");
  Published.RecordMetric("urban resident groups", static_cast<double>(urban.Resident), "groups");
  Published.RecordMetric("urban terrain captures", static_cast<double>(urban.Captures), "captures");
  Published.RecordMetric(
      "urban scene publications", static_cast<double>(urban.Publications), "publications");
}

std::string_view Engine::State::UrbanWorldBlocker() const noexcept {
  if (!World.Surface().UrbanAssets() || !World.Surface().HasVectorSource() ||
      !Session.Declared.Ground.Osm.empty()) {
    return {};
  }
  return World.UrbanAreas && World.UrbanAreas->Ready() ? "" : "pending resolved urban assets";
}

bool Engine::State::AdvanceUrbanAreas() {
  if (!World.Surface().UrbanAssets() || !World.Surface().HasVectorSource() ||
      !Session.Declared.Ground.Osm.empty() || !Picture.Standing || World.GroundBuild ||
      !World.GroundPublished.CanPublish()) {
    return true;
  }
  const auto &revision = World.GroundPublished.Current();
  const auto *terrain = World.TerrainSurface.PublishedSurface();
  const auto eye = CurrentGeographicEyeEcef();
  if (!revision || revision->Quality != GroundQuality::Refined || terrain == nullptr || !eye) {
    return true;
  }
  if (!World.UrbanAreas) {
    World.UrbanAreas =
        std::make_unique<UrbanAreaStreaming>(*Services.Compute,
                                             World.Surface().UrbanAssets(),
                                             World.Surface().VectorSchema(),
                                             Session.Under.AssetCacheDirectory + "/assets.sqlite");
    World.UrbanAreas->Into(Picture.Device);
  }
  const auto origin = Session.Declared.Ground.Origin;
  (void)World.BuildingCoverage.Update(World.Pieces, World.Surface().Footprints());
  auto advanced =
      World.UrbanAreas->Advance({.Focus = CurrentGeographicFocus(),
                                 .EyeEcef = *eye,
                                 .FocalPx = World.Surface().Footprints().Projection().FocalPx,
                                 .RadiusM = TerrainSightM(),
                                 .Frame = TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg,
                                                            .LatitudeDeg = origin.LatitudeDeg})},
                                *terrain,
                                World.Rebuilds,
                                World.BuildingCoverage.Regions(),
                                World.Pieces.Digest());
  if (!advanced) {
    Error = std::move(advanced.error());
    return false;
  }
  return true;
}
}
