#include "HeightField.h"
#include "Check.h"
#include <cstdint>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::TerrainField source(5, 5);
  for (uint32_t row = 0; row < 5; ++row) {
    for (uint32_t column = 0; column < 5; ++column) {
      source.SetM(
          row, column, 25.0f * static_cast<float>(column) + 50.0f * static_cast<float>(row));
    }
  }
  const Data::TileId parent{.Zoom = 1, .X = 1, .Y = 0};
  source.AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = parent, .SourceId = "source", .Revision = "r1"});
  Ground::HeightField::Block upperLeft;
  Ground::HeightField::Block upperRight;
  CHECK(Ground::HeightField::ResamplesSourcedAncestor(
            source, parent, {.Zoom = 2, .X = 2, .Y = 0}, upperLeft) &&
            upperLeft.Raster.Side == 3 && upperLeft.Sources.size() == 1,
        "sourced parent supplies a child at its native posting density");
  CHECK(upperLeft.Nodes == std::vector<float>({0, 25, 50, 50, 75, 100, 100, 125, 150}),
        "child samples retain the parent bilinear coordinates");
  CHECK(Ground::HeightField::ResamplesSourcedAncestor(
            source, parent, {.Zoom = 2, .X = 3, .Y = 0}, upperRight) &&
            upperRight.Nodes.front() == 50 && upperRight.Nodes.back() == 200 &&
            upperRight.Sources == upperLeft.Sources,
        "adjacent child starts at the shared parent midpoint with the same provenance");
  Ground::HeightField::Block refused;
  CHECK(!Ground::HeightField::ResamplesSourcedAncestor(
            source, parent, {.Zoom = 2, .X = 0, .Y = 0}, refused),
        "unrelated tile cannot inherit another region's height source");
  source = Ground::TerrainField(5, 5);
  CHECK(!Ground::HeightField::ResamplesSourcedAncestor(
            source, parent, {.Zoom = 2, .X = 2, .Y = 0}, refused),
        "unsourced terrain cannot certify a refined building input");
  return Report();
}
