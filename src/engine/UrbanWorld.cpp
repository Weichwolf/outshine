#include "GroundQuality.h"
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
  if (const auto areas = World.Surface().UrbanAssets()) {
    const auto costs = areas->Costs();
    Published.RecordMetric(
        "urban area generations", static_cast<double>(costs.Generations), "areas");
    Published.RecordMetric("urban area writes", static_cast<double>(costs.Writes), "areas");
    Published.RecordMetric(
        "urban area native reads", static_cast<double>(costs.ReadBytes), "bytes");
    Published.RecordMetric(
        "urban area retained publication", static_cast<double>(costs.PublicationBytes), "bytes");
    Published.RecordMetric("urban area publication attempts",
                           static_cast<double>(costs.PublicationAttempts),
                           "attempts");
    Published.RecordMetric("urban area publication deferred",
                           static_cast<double>(costs.PublicationDeferred),
                           "attempts");
  }
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
    World.UrbanAreas = std::make_unique<UrbanAreaStreaming>(World.Surface().UrbanAssets(),
                                                            World.Surface().VectorSchema());
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
