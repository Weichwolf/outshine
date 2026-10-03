#include "BuildingField.h"
#include "Check.h"
#include <array>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  ::outshine::Generators::Osm::OsmField empty(14, {});
  ::outshine::Generators::Osm::BuildingField indexed;
  const size_t unpreparedIndexBytes = indexed.HeapBytes();
  indexed.PreparesAcceptances({});
  const size_t preparedIndexBytes = indexed.HeapBytes() - unpreparedIndexBytes;
  ::outshine::Generators::Osm::BuildingField measured;
  const size_t unpreparedBytes = measured.HeapBytes();
  measured.PreparesAcceptances({.Spread = 7, .Across = 11});
  CHECK(measured.HeapBytes() - unpreparedBytes ==
                preparedIndexBytes + measured.MeasurementBytes() &&
            measured.MeasurementBytes() >= 18u * sizeof(double),
        "heap budget includes all reserved contact measurements");
  ::outshine::Generators::Osm::BuildingField field;
  const auto originalOwner = field.ReservationOwner();
  ::outshine::Generators::Osm::BuildingField copied = field;
  CHECK(copied.ReservationOwner() != originalOwner,
        "copied reservation state has a distinct owner domain");
  copied = field;
  CHECK(copied.ReservationOwner() != originalOwner,
        "copy assignment cannot alias a reservation owner");
  ::outshine::Generators::Osm::BuildingField moved = std::move(copied);
  const auto movedOwner = moved.ReservationOwner();
  copied = std::move(moved);
  CHECK(copied.ReservationOwner() == movedOwner,
        "moves transfer the reservation domain with its state");
  field.ResetDerived();
  CHECK(field.ReservationOwner() != originalOwner,
        "reset revokes the old domain while queued leases can keep it alive");
  const Vec3 anchor{{1.0, 2.0, 3.0}};
  field.AnchorAt(anchor);
  field.SeenWith({.FocalPx = 720.0, .AllowedErrorPx = 0.25});
  field.TilesSpan(2400.0);
  const std::array<::outshine::Ground::BuildingFootprint, 1> prints{{{.HeightM = 12.0f}}};
  const std::array<double, 1> spread{2.0}, across{8.0};
  for (int cycle = 0; cycle < 3; ++cycle) {
    const auto before = field.Revision();
    field.Take(0);
    field.Release(0);
    CHECK(field.IngestedTiles() == 0 && field.Footprints().empty() && field.Revision() == before,
          "abandoned bake restores the pending tile without publishing data");
    field.Take(0);
    const ::outshine::Generators::Osm::BuildingField::Baked baked{.Prints = prints,
                                                                  .SeatSpreadM = spread,
                                                                  .AcrossM = across,
                                                                  .Triangles = 12,
                                                                  .OsmHeights = 1};
    field.PreparesAcceptances({.Prints = 1, .Spread = 1, .Across = 1, .Tiles = 1});
    auto pending = field.PrepareAcceptance(0, baked);
    CHECK(field.Footprints().empty() && field.Revision() == before,
          "prepared acceptance does not publish footprints");
    field.CommitAcceptance(std::move(pending), empty, baked);
    CHECK(field.Footprints().size() == 1, "rebaking does not append stale footprints");
    CHECK(field.MeasurementBytes() >= 2u * sizeof(double), "accepted measurements own storage");
    CHECK(field.TrianglesHanded() == 12, "new bake owns its own triangle count");
    CHECK(field.Revision() > before, "accepted bake invalidates dependent terrain");
    const auto accepted = field.Revision();
    field.ResetDerived();
    CHECK(field.Revision() > accepted,
          "reset invalidates dependent terrain even when counts recur");
    CHECK(field.Footprints().empty() && field.OfTile(0).empty(),
          "geometry and tile ranges reset together");
    CHECK(field.IngestedTiles() == 0 && field.Ingested(empty),
          "empty field has no outstanding or completed tile marks");
    CHECK(field.TrianglesHanded() == 0 && field.OsmHeights() == 0, "derived counters reset");
    CHECK(field.SeatSpreadM().empty() && field.FootprintAcrossM().empty(),
          "derived measurements reset");
    CHECK(field.Projection().FocalPx == 720.0 && field.Projection().AllowedErrorPx == 0.25 &&
              field.TileSpanM() == 2400.0,
          "generation configuration survives");
    CHECK(field.Anchor()[0] == 1.0 && field.Anchor()[1] == 2.0 && field.Anchor()[2] == 3.0,
          "world anchor survives");
  }
  return Report();
}
