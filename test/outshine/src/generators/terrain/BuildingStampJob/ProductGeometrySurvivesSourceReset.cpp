#include "BuildingStampJob.h"
#include "Check.h"

#include <array>
#include <memory>
#include <span>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Field = ::outshine::Generators::Osm::BuildingField;
  Field field;
  ::outshine::Generators::Osm::OsmField vectors(14, {});
  field.AnchorAt({{1, 2, 3}});
  std::array<std::weak_ptr<const Ground::BuildingGeometry>, 2> owners;
  for (uint32_t tile = 0; tile < 2; ++tile) {
    auto geometry = std::make_shared<Ground::BuildingGeometry>();
    const double longitude = static_cast<double>(tile) * 0.01;
    geometry->Points = {
        0, longitude, 0, longitude + 0.001, 0.001, longitude + 0.001, 0.001, longitude};
    owners[tile] = geometry;
    const Ground::BuildingFootprint footprint{.PointCount = 4, .SeatM = 12};
    const Field::Baked product{.Coordinates = geometry, .Prints = std::span(&footprint, 1)};
    field.PreparesAcceptances({.Prints = 1, .Tiles = 1});
    field.Take(tile);
    auto pending = field.PrepareAcceptance(tile, product);
    field.CommitAcceptance(std::move(pending), vectors, product);
  }
  Field snapshot = field.SnapshotAccepted();
  field.ResetDerived();
  CHECK(!owners[0].expired() && !owners[1].expired(),
        "accepted products retain geometry after producer reset");
  const TangentFrame frame = TangentFrame::At({.LongitudeDeg = 0, .LatitudeDeg = 0});
  Generators::BuildingStampJob job(frame, snapshot.Revision());
  bool done = false;
  for (size_t step = 0; step < 100 && !done; ++step) {
    const auto advanced = job.Advance({.Products = &snapshot,
                                       .Footprints = snapshot.Footprints(),
                                       .VectorGeneration = snapshot.Revision(),
                                       .UnitsMost = 1});
    CHECK(advanced.has_value(), "product-owned stamp advances without an external point buffer");
    if (!advanced) { break; }
    done = *advanced;
  }
  CHECK(done, "interrupted work completes both products");
  auto stamps = std::move(job).Take();
  CHECK(stamps && stamps->size() == 2, "each product contributes its own grounded polygon");
  if (stamps && stamps->size() == 2) {
    const auto second =
        frame.ToLocalPosition({.LongitudeDeg = 0.01, .LatitudeDeg = 0, .HeightM = 12});
    CHECK((*stamps)[0].RingEastNorthM.front() == 0 &&
              (*stamps)[1].RingEastNorthM.front() == second.EastM,
          "identical local point indices resolve against different product geometry");
  }
  snapshot.ResetDerived();
  CHECK(owners[0].expired() && owners[1].expired(), "retiring products releases their geometry");
  return Report();
}
