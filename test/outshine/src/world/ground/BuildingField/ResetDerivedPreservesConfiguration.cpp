#include "BuildingField.h"
#include "Check.h"
#include <array>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  OsmField empty(14, {});
  BuildingField field;
  const Vec3 anchor{{1.0, 2.0, 3.0}};
  field.AnchorAt(anchor);
  field.SeenWith(720.0);
  field.TilesSpan(2400.0);
  const std::array<BuildingField::Footprint, 1> prints{{{.HeightM = 12.0f}}};
  const std::array<double, 1> spread{2.0}, across{8.0};
  for (int cycle = 0; cycle < 3; ++cycle) {
    field.Take(0);
    field.Accept(0,
                 empty,
                 {.Prints = prints,
                  .SeatSpreadM = spread,
                  .AcrossM = across,
                  .Triangles = 12,
                  .OsmHeights = 1});
    CHECK(field.Footprints().size() == 1, "rebaking does not append stale footprints");
    CHECK(field.TrianglesHanded() == 12, "new bake owns its own triangle count");
    field.ResetDerived();
    CHECK(field.Footprints().empty() && field.OfTile(0).empty(),
          "geometry and tile ranges reset together");
    CHECK(field.IngestedTiles() == 0 && field.Ingested(empty),
          "empty field has no outstanding or completed tile marks");
    CHECK(field.TrianglesHanded() == 0 && field.OsmHeights() == 0, "derived counters reset");
    CHECK(field.SeatSpreadM().empty() && field.FootprintAcrossM().empty(),
          "derived measurements reset");
    CHECK(field.FocalPx() == 720.0 && field.TileSpanM() == 2400.0,
          "generation configuration survives");
    CHECK(field.Anchor()[0] == 1.0 && field.Anchor()[1] == 2.0 && field.Anchor()[2] == 3.0,
          "world anchor survives");
  }
  return Report();
}
