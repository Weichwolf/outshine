#include "Check.h"
#include "TerrainTiles.h"

#include <array>
#include <limits>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Ground;

class Meters final : public TerrainSource {
public:
  bool Missing = false;

  TerrainBytes Take(Data::TileId at) override {
    TerrainField field(4, 4);
    for (uint32_t row = 0; row < 4; ++row) {
      for (uint32_t col = 0; col < 4; ++col) {
        field.SetM(
            row, col, Missing ? std::numeric_limits<float>::quiet_NaN() : Height(at, row, col));
      }
    }
    return TerrainBytes::From(
        at,
        std::move(field),
        {.Tile = at, .SourceId = "native-orthometric-meters", .Revision = "original"},
        "original-source-key");
  }

  static float Height(Data::TileId at, uint32_t row, uint32_t col) {
    return -42.54321f + static_cast<float>((at.Y * 3 + row) * 16 + at.X * 3 + col) * 0.00031f;
  }
};
}

int main() {
  using namespace outshine::Test;
  const Data::TileId at{.Zoom = 3, .X = 3, .Y = 3};
  TerrainField original(4, 4);
  original.SetM(1, 2, Meters::Height(at, 1, 2));
  const auto *allocation = original.Data();
  auto delivered = TerrainBytes::From(at, std::move(original), {.Tile = at});
  auto payload = delivered.Take();
  CHECK(payload && std::get<TerrainField>(payload->Samples).Data() == allocation,
        "native terrain delivery transfers its float allocation without encoding or copying");
  Meters source;
  TerrainTiles tiles(source, EnuFrame::At({}), {});
  const auto stitched = tiles.StitchedGrid(at.Zoom, at.X, at.Y);
  const auto *field = stitched.TryField();
  CHECK(field && field->Rows() == 4 && field->Cols() == 4 && !field->HasMissingBoundary(),
        "native meters use the existing complete-neighbour terrain stitching path");
  if (field) {
    for (uint32_t row = 0; row < 4; ++row) {
      for (uint32_t col = 0; col < 4; ++col) {
        CHECK(field->AtM(row, col) == Meters::Height(at, row, col),
              "submillimeter changes and negative elevations remain exact native floats");
      }
    }
    CHECK(field->Sources().size() == 9 && !field->Certificate().IsComplete(),
          "source provenance merges without inventing a current terrain certificate");
  }
  source.Missing = true;
  TerrainTiles missing(source, EnuFrame::At({}), {});
  const auto refused = missing.StitchedGrid(at.Zoom, at.X, at.Y);
  CHECK(!refused.TryField() && refused.Failure() &&
            refused.Failure()->Reason == Data::FetchFailureReason::CorruptPayload,
        "missing native postings cannot become zero ground or renderable NaN geometry");
  return Report();
}
