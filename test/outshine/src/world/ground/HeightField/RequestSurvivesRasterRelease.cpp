#include "Check.h"
#include "HeightField.h"
#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  auto terrain = std::make_shared<TerrainField>(2, 2);
  std::fill_n(terrain->Data(), 4, 12.0f);
  const std::weak_ptr<const TerrainField> released = terrain;
  terrain->AddSource({.Kind = Data::DataKind::Elevation,
                      .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                      .SourceId = "ancestor",
                      .Revision = "one"});
  HeightField::Block east;
  HeightField::Block west;
  CHECK(HeightField::SharesField(terrain, {.Zoom = 15, .X = 9, .Y = 14}, east) &&
            HeightField::SharesField(terrain, {.Zoom = 15, .X = 8, .Y = 14}, west),
        "two child requests share one resolved ancestor");
  auto field = HeightField::Of(15, {east, west});
  auto request = field->CaptureRequest();
  CHECK(field->Sources().size() == 1 && request.Zoom == 15 && !request.Fallback &&
            request.Tiles.size() == 2 && request.Tiles[0].Zoom == 15 && request.Tiles[0].X == 9 &&
            request.Tiles[0].Y == 14 && request.Tiles[1].X == 8,
        "recipe preserves original child addresses and order instead of ancestor identity");
  auto reverse = HeightField::Of(15, {west, east});
  CHECK(reverse->RasterDigest() != field->RasterDigest(),
        "equal sources and samples do not make block order interchangeable");
  auto otherZoom = HeightField::Of(14, {east, west});
  CHECK(otherZoom->CaptureRequest().Zoom == 14 &&
            otherZoom->RasterDigest() != field->RasterDigest(),
        "field zoom is retained independently of block zoom");
  const auto fallback = HeightField::Of(15, {east, west}, true)->CaptureRequest();
  CHECK(fallback.Fallback && fallback.Tiles.size() == 2, "recipe retains fallback qualification");
  const auto repeated = HeightField::Of(15, {east, east})->CaptureRequest();
  CHECK(repeated.Tiles.size() == 2 && repeated.Tiles[0].X == repeated.Tiles[1].X,
        "capture does not sort or deduplicate digest inputs");
  field.reset();
  reverse.reset();
  otherZoom.reset();
  east.Terrain.reset();
  west.Terrain.reset();
  terrain.reset();
  CHECK(released.expired() && request.Tiles.size() == 2 && request.Tiles.front().X == 9,
        "owned request remains after producer release");
  return Report();
}
