#include "HeightField.h"
#include "Check.h"
#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const Data::TileId parent{.Zoom = 1, .X = 0, .Y = 0};
  const Data::TileId child{.Zoom = 2, .X = 0, .Y = 0};
  for (const bool partial : {false, true}) {
    auto field = std::make_shared<TerrainField>(2, 2);
    field->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = parent,
                      .SourceId = "boundary-dem",
                      .Revision = "r1"});
    if (partial) { field->MarkMissingBoundary(); }
    HeightField::Block groundCopy;
    const auto ground = GroundBlock::Over(field->Data(),
                                          {.Zoom = 1, .X = 0, .Y = 0},
                                          {.Side = 2, .Postings = 2},
                                          field->Sources(),
                                          partial);
    CHECK(HeightField::Copies(ground, groundCopy), "ground block preserves usable heights");
    CHECK(HeightField::Of(parent.Zoom, {std::move(groundCopy)})->Qualified() == !partial,
          "ground copy never promotes a partial boundary to qualified input");
    HeightField::Block copied;
    CHECK(HeightField::CopiesField(*field, parent, copied), "copy preserves usable heights");
    CHECK(HeightField::Of(parent.Zoom, {std::move(copied)})->Qualified() == !partial,
          "copy never promotes a partial boundary to qualified input");
    HeightField::Block shared;
    CHECK(HeightField::SharesField(field, parent, shared), "share preserves usable heights");
    CHECK(HeightField::Of(parent.Zoom, {std::move(shared)})->Qualified() == !partial,
          "share never promotes a partial boundary to qualified input");
    HeightField::Block sampled;
    CHECK(HeightField::ResamplesSourcedAncestor(*field, parent, child, sampled),
          "ancestor resampling preserves usable heights");
    CHECK(HeightField::Of(child.Zoom, {std::move(sampled)})->Qualified() == !partial,
          "resampling never promotes a partial boundary to qualified input");
  }
  return Report();
}
