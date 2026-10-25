#ifndef OUTSHINE_TEST_SOURCEBUILDINGFIXTURE_H
#define OUTSHINE_TEST_SOURCEBUILDINGFIXTURE_H
#include "SurfacePreparation.h"
#include "StructureBuildQueue.h"
#include "BuildingField.h"
#include "TerrainRevisionIndex.h"
#include "Check.h"
#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

namespace outshine::Test::SourceBuildingFixture {
inline constexpr double kEyeLongitudeDeg = 8.5659;
inline constexpr double kEyeLatitudeDeg = 49.3274;
inline constexpr double kWestDeg = 8.565899;
inline constexpr double kSouthDeg = 49.327399;
inline constexpr double kEastDeg = 8.565901;
inline constexpr double kNorthDeg = 49.327401;
inline constexpr double kTileSpanM = 1000;
inline constexpr double kFocalPx = 720;
inline constexpr int kAttempts = 100;
inline constexpr double kWaitS = 0.02;
using TerrainFields = std::vector<std::pair<Data::TileId, std::shared_ptr<Ground::TerrainField>>>;

struct BuildView {
  Ground::SurfacePreparation &stack;
  Generators::Osm::BuildingField &prints;
  StructureBuildQueue &queue;
  const Generators::Osm::StructurePolicy &policy;
  const StructureBuildQueue::HeightSource &heights;
  LongitudeLatitude eye;
  TangentFrame frame;
};

inline bool PrepareTerrain(const StructureBuildQueue &queue,
                           const Ground::SurfacePreparation &stack,
                           TerrainFields &fields) {
  using namespace Ground;
  const auto tiles = queue.OriginalHeightTiles(stack.FinestZoomOf(Data::DataKind::Elevation));
  CHECK(tiles && !tiles->empty(), "original geometry requests its own DEM fields");
  if (!tiles || tiles->empty()) { return false; }
  auto revisions = TerrainRevisionIndex::Create(128);
  CHECK(revisions.has_value(), "analytic terrain has independent delivery identity");
  if (!revisions) { return false; }
  for (const auto tile : *tiles) {
    const auto stamp = (**revisions).IssueDeliveryStamp(tile);
    CHECK(stamp.has_value(), "every demanded terrain block has a delivery");
    if (!stamp) { return false; }
    auto field = std::make_shared<TerrainField>(3, 3);
    std::fill_n(field->Data(), 9, 100.0f);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = tile,
                      .SourceId = "analytic-plane",
                      .Revision = "one"});
    field->SetCertificate(TerrainCertificate::FromDelivery(tile, *stamp, 11));
    fields.emplace_back(tile, std::move(field));
  }
  return true;
}
}
#endif
