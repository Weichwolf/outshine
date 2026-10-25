#ifndef OUTSHINE_TEST_BRIDGEGROUNDFIXTURE_H
#define OUTSHINE_TEST_BRIDGEGROUNDFIXTURE_H

#include "BridgeGroundSamples.h"
#include "TransportCoreFixture.h"
#include "HeightField.h"
#include "TerrainRevisionIndex.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Test {
inline constexpr double kBridgeExtentDeg = 0.25;
inline constexpr float kWestGroundAslM = 0;
inline constexpr float kEastGroundAslM = 2000;
inline constexpr uint64_t kBridgeTerrainScope = 17;
inline constexpr Generators::BridgeGroundLimits kGroundLimits{.NodesMost = 64,
                                                              .TerrainBlocksMost = 16,
                                                              .SourcesPerBlockMost = 16,
                                                              .ContainerCapacityMost =
                                                                  size_t{64} * 1024};

struct BridgeGroundFixture {
  Generators::BridgeInputRequirements Inputs;
  std::shared_ptr<const Ground::HeightField> Terrain;
  std::unique_ptr<Ground::TerrainRevisionIndex> Revisions;
};

inline BridgeGroundFixture MakeBridgeGroundFixture() {
  using namespace Ground;
  using namespace Generators;
  BridgeGroundFixture fixture;
  const auto source = BridgeAcrossCore(kBridgeExtentDeg);
  const std::array seeds{World::TransportEdgeId{.PathId = kCoreBridgePath}};
  const auto selected = SelectBridgeInputRequirements(
      source, seeds, {.SourceEdgesMost = 64, .ContainerCapacityMost = size_t{64} * 1024});
  auto revisions = TerrainRevisionIndex::Create(16);
  if (!selected || !revisions) { return fixture; }
  fixture.Inputs = *selected;
  fixture.Revisions = std::move(*revisions);
  constexpr int zoom = 4;
  std::vector<HeightField::Block> blocks;
  for (size_t at = 0; at < fixture.Inputs.GroundNodes.size(); ++at) {
    const auto *node = source->Topology().FindNode(fixture.Inputs.GroundNodes[at]);
    if (node == nullptr) { return {}; }
    const auto spot = HeightField::SpotOf(
        {.LongitudeDeg = node->LongitudeDeg, .LatitudeDeg = node->LatitudeDeg}, zoom);
    const Data::TileId tile{
        .Zoom = spot.Zoom, .X = static_cast<uint32_t>(spot.X), .Y = static_cast<uint32_t>(spot.Y)};
    const auto stamp = fixture.Revisions->IssueDeliveryStamp(tile);
    if (!stamp) { return {}; }
    auto raster = std::make_shared<TerrainField>(2, 2);
    for (uint32_t row = 0; row < 2; ++row) {
      for (uint32_t col = 0; col < 2; ++col) {
        raster->SetM(row, col, at == 0 ? kWestGroundAslM : kEastGroundAslM);
      }
    }
    raster->AddSource({.Kind = Data::DataKind::Elevation,
                       .Tile = tile,
                       .SourceId = "bridge-dem",
                       .Revision = "r1"});
    raster->SetCertificate(TerrainCertificate::FromDelivery(tile, *stamp, kBridgeTerrainScope));
    HeightField::Block block;
    if (!HeightField::SharesField(raster, tile, block)) { return {}; }
    blocks.push_back(std::move(block));
  }
  fixture.Terrain = HeightField::Of(zoom, std::move(blocks));
  return fixture;
}
}
#endif
