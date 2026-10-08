#include "GroundSnapshot.h"
#include "Check.h"
#include <array>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const Tile region(14, 8556, 5799);
  const auto a = region.Geo({.EastM = 10, .NorthM = 10});
  const auto b = region.Geo({.EastM = 20, .NorthM = 10});
  const auto c = region.Geo({.EastM = 10, .NorthM = 20});
  const std::array<std::string, 1> layers{Osm::OsmLayerName(Osm::OsmLayer::Buildings)};
  Osm::OsmField vectors(14, layers);
  const std::array<Osm::OsmField::Declared, 1> declared{{{.Layer = layers[0],
                                                          .Area = true,
                                                          .LatLon = {a.LatitudeDeg,
                                                                     a.LongitudeDeg,
                                                                     b.LatitudeDeg,
                                                                     b.LongitudeDeg,
                                                                     c.LatitudeDeg,
                                                                     c.LongitudeDeg}}}};
  vectors.Declare(declared, {.X = region.X(), .Y = region.Y()});
  Osm::BuildingField footprints;
  WaterAsset water;
  Osm::StreetField streets;
  const Fields inputs{
      .Vectors = &vectors, .Footprints = &footprints, .WaterBodies = &water, .Ways = &streets};
  CHECK(vectors.Settled(region.X(), region.Y()), "vector input is fully available");
  CHECK(!FeaturesOver(region, inputs),
        "known buildings pending preparation cannot be classified as unoccupied land");
  const auto tile = static_cast<uint32_t>(vectors.TileIndex(region.X(), region.Y()));
  const std::array<outshine::Ground::BuildingFootprint, 1> prints{
      {{.FirstPoint = 0, .PointCount = 3, .HeightM = 12, .BaseM = 100}}};
  const Osm::BuildingField::Baked baked{.Prints = prints};
  footprints.PreparesAcceptances({.Prints = 1, .Tiles = 1});
  footprints.Take(tile);
  footprints.CommitAcceptance(footprints.PrepareAcceptance(tile, baked), vectors, baked);
  const auto ready = FeaturesOver(region, inputs);
  CHECK(ready && ready->Count() == 1 && ready->At(0).Kind == FeatureKind::Structure,
        "placement receives the building footprint after native publication");
  footprints.ResetDerived();
  const Osm::BuildingField::Baked empty;
  footprints.PreparesAcceptances({.Tiles = 1});
  footprints.Take(tile);
  footprints.CommitAcceptance(footprints.PrepareAcceptance(tile, empty), vectors, empty);
  const auto filtered = FeaturesOver(region, inputs);
  CHECK(filtered && filtered->Count() == 0,
        "an explicitly prepared empty building result is ready");
  vectors.Declare({}, {.X = region.X(), .Y = region.Y()});
  footprints.ResetDerived();
  const auto vacant = FeaturesOver(region, inputs);
  CHECK(vacant && vacant->Count() == 0, "source without buildings needs no building product");
  return Report();
}
