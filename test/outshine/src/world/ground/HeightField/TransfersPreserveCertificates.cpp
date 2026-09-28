#include "HeightField.h"
#include "TerrainRevisionIndex.h"
#include "Check.h"
#include <array>
#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto made = TerrainRevisionIndex::Create(16);
  auto &index = **made;
  const Data::TileId parent{.Zoom = 1, .X = 0, .Y = 0};
  const Data::TileId child{.Zoom = 2, .X = 0, .Y = 0};
  const auto stamp = *index.IssueDeliveryStamp(parent);
  for (const bool partial : {false, true}) {
    auto raster = std::make_shared<TerrainField>(2, 2);
    raster->AddSource({.Kind = Data::DataKind::Elevation,
                       .Tile = parent,
                       .SourceId = "certificate-dem",
                       .Revision = "r1"});
    raster->SetCertificate(TerrainCertificate::FromDelivery(parent, stamp));
    if (partial) { raster->MarkMissingBoundary(); }
    const auto borrowed = GroundBlock::Over(raster->Data(),
                                            {.Zoom = 1, .X = 0, .Y = 0},
                                            {.Side = 2, .Postings = 2},
                                            raster->Sources(),
                                            partial,
                                            &raster->Certificate());
    std::array<HeightField::Block, 4> blocks;
    CHECK(HeightField::Copies(borrowed, blocks[0]), "ground view copies certificate with heights");
    CHECK(HeightField::CopiesField(*raster, parent, blocks[1]), "field copy preserves certificate");
    CHECK(HeightField::SharesField(raster, parent, blocks[2]), "field share preserves certificate");
    CHECK(HeightField::ResamplesSourcedAncestor(*raster, parent, child, blocks[3]),
          "ancestor resampling preserves requested dependency certificate");
    for (auto &block : blocks) {
      CHECK(block.Certificate.Dependencies().size() == 1 &&
                block.Certificate.Dependencies()[0] == stamp,
            "transfer retains exact registration and delivery identity");
      auto field = HeightField::Of(block.At.Zoom, {std::move(block)});
      CHECK(field && field->Certificate().IsComplete() == !partial,
            "height input never promotes a partial certificate");
      CHECK(field && index.AreCurrent(field->Certificate().Dependencies()),
            "height input retains valid metadata independently of raster representation");
    }
  }
  HeightField::Block a, b;
  TerrainField first(2, 2), second(2, 2);
  const Data::TileId neighbour{.Zoom = 1, .X = 1, .Y = 0};
  const auto other = *index.IssueDeliveryStamp(neighbour);
  first.AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = parent, .SourceId = "dem", .Revision = "r1"});
  second.AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = neighbour, .SourceId = "dem", .Revision = "r1"});
  first.SetCertificate(TerrainCertificate::FromDelivery(parent, stamp));
  second.SetCertificate(TerrainCertificate::FromDelivery(neighbour, other));
  CHECK(HeightField::CopiesField(first, parent, a) &&
            HeightField::CopiesField(second, neighbour, b),
        "independent inputs are copied");
  const auto combined = HeightField::Of(1, {std::move(a), std::move(b)});
  CHECK(combined && combined->Certificate().IsComplete() &&
            combined->Certificate().Dependencies().size() == 2,
        "height input combines distinct requested dependencies");
  CHECK(index.IssueDeliveryStamp(neighbour).has_value(), "one contributing delivery changes");
  CHECK(combined && !index.AreCurrent(combined->Certificate().Dependencies()),
        "changed contributor revokes the aggregate certificate");
  const auto empty = HeightField::Of(1, {});
  CHECK(!empty->Qualified() && !empty->Certificate().IsComplete(),
        "empty height input cannot qualify or certify terrain");
  {
    auto raster = std::make_shared<TerrainField>(2, 2);
    raster->AddSource({.Kind = Data::DataKind::Elevation,
                       .Tile = parent,
                       .SourceId = "detached-dem",
                       .Revision = "r1"});
    raster->SetCertificate(TerrainCertificate::FromDelivery(parent, stamp));
    const std::weak_ptr<const TerrainField> lifetime = raster;
    HeightField::Block copied;
    CHECK(HeightField::CopiesField(*raster, parent, copied), "independent raster is copied");
    const auto field = HeightField::Of(1, {std::move(copied)});
    raster.reset();
    CHECK(lifetime.expired(), "copy certificate retains no original raster handle");
    CHECK(field->Certificate().IsComplete() &&
              index.AreCurrent(field->Certificate().Dependencies()),
          "copied certificate remains usable after original raster destruction");
  }
  return Report();
}
