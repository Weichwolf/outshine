#include "Check.h"
#include "SourcedTerrainFields.h"
#include <array>
#include <memory>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t budget = 32 * 1024;
  const Data::TileId tile{.Zoom = 4, .X = 0, .Y = 0};
  auto field = std::make_shared<Ground::TerrainField>(257, 257);
  field->AddSource({.Kind = Data::DataKind::Elevation,
                    .Tile = tile,
                    .SourceId = "dem",
                    .Revision = "immutable"});
  for (uint32_t row = 0; row < 257; ++row) {
    for (uint32_t column = 0; column < 257; ++column) { field->SetM(row, column, 42); }
  }
  CHECK(field->HeapBytes() > budget, "the resident raster exceeds the per-job allocation budget");
  const std::array fields{SourcedTerrainFields::Entry{tile, field}};
  const std::array exact{Ground::TileSpot{.Zoom = 4, .X = 0, .Y = 0}};
  const auto captured = SourcedTerrainFields::Capture(fields, exact, budget);
  CHECK(captured && captured->RetainedBytes() > budget && captured->PreparationBytes() < budget,
        "capturing shared terrain charges pointer storage while still accounting retained data");
  if (captured) {
    CHECK(captured->FitsPreparation(exact, budget),
          "sharing an exact raster fits a small preparation allocation budget");
    Ground::HeightField::Block block;
    CHECK(captured->ShareSourcedField(tile, block) && block.Terrain == field && block.Nodes.empty(),
          "the prepared block references the original raster without a sample copy");
  }
  const std::array child{Ground::TileSpot{.Zoom = 5, .X = 0, .Y = 0}};
  const auto resampling = SourcedTerrainFields::Capture(fields, child, budget);
  CHECK(resampling && !resampling->FitsPreparation(child, budget),
        "a new 129 by 129 float raster exceeds 32 KiB and remains refused");
  const auto refused =
      SourcedTerrainFields::Capture(fields, exact, sizeof(SourcedTerrainFields::Entry) - 1);
  CHECK(!refused && refused.error() == SourcedTerrainFields::CaptureError::OverBudget,
        "even shared source capture requires sufficient pointer storage");
  return Report();
}
