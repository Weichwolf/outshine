#include "EngineState.h"
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>
#include <map>
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

namespace {
std::vector<Ground::GeoBounds>
PublishedBuildingTiles(const Render::GeometryPageResidency &pieces,
                       const Generators::Osm::BuildingField &footprints) {
  std::map<uint32_t, uint64_t> masks;
  pieces.ForEachDigest([&](const Render::GeometryPageResidency::DigestRecord &piece) {
    const auto *input = footprints.InputOfTile(piece.Tile);
    if (!input || !input->Qualified || !input->Vector || input->SourceKey != piece.SourceKey ||
        piece.FallbackHeights) {
      return;
    }
    if (piece.Cell == 0) {
      masks[piece.Tile] = input->OccupiedCells;
    } else if (piece.Cell <= Ground::kStructureCellsPerTile) {
      masks[piece.Tile] |= uint64_t{1} << (piece.Cell - 1);
    }
  });
  std::vector<Ground::GeoBounds> bounds;
  bounds.reserve(masks.size());
  for (const auto &[tile, mask] : masks) {
    const auto &input = *footprints.InputOfTile(tile);
    if (input.Vector && mask == input.OccupiedCells) {
      bounds.push_back(Ground::TileBounds(input.Vector->Tile));
    }
  }
  return bounds;
}
}

std::string_view Engine::State::UrbanWorldBlocker() const noexcept {
  if (!World.Stack.UrbanAssets() || !World.Stack.HasVectorSource() ||
      !Session.Declared.Ground.Osm.empty()) {
    return {};
  }
  return World.UrbanAreas && World.UrbanAreas->Ready() ? "" : "pending resolved urban assets";
}

bool Engine::State::AdvanceUrbanAreas() {
  if (!World.Stack.UrbanAssets() || !World.Stack.HasVectorSource() ||
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
        std::make_unique<UrbanAreaStreaming>(*World.Pool,
                                             World.Stack.UrbanAssets(),
                                             World.Stack.VectorSchema(),
                                             Session.Under.AssetCacheDirectory + "/assets.sqlite");
    World.UrbanAreas->Into(Picture.Device);
  }
  const auto origin = Session.Declared.Ground.Origin;
  if (World.UrbanChildrenRevision != World.Pieces.Digest()) {
    World.UrbanChildren = PublishedBuildingTiles(World.Pieces, World.Stack.Footprints());
    World.UrbanChildrenRevision = World.Pieces.Digest();
  }
  auto advanced =
      World.UrbanAreas->Advance({.Focus = CurrentGeographicFocus(),
                                 .EyeEcef = *eye,
                                 .FocalPx = World.Stack.Footprints().Projection().FocalPx,
                                 .RadiusM = TerrainSightM(),
                                 .Frame = TangentFrame::At({.LongitudeDeg = origin.LongitudeDeg,
                                                            .LatitudeDeg = origin.LatitudeDeg})},
                                *terrain,
                                World.Rebuilds,
                                World.UrbanChildren,
                                World.Pieces.Digest());
  if (!advanced) {
    Error = std::move(advanced.error());
    return false;
  }
  return true;
}
}
