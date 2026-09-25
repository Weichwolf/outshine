#include "PublishedRegion.h"
#include "Check.h"

#include <array>
#include <memory>
#include <span>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;

  std::shared_ptr<const PublishedRegion> old;
  std::shared_ptr<const PublishedRegion> next;
  {
    const std::array<std::string, 1> layers{"building"};
    OsmField ingest(14, layers);
    StreetField ways;
    WaterField water;
    BuildingField footprints;
    footprints.AnchorAt({{1, 2, 3}});
    const std::array<OsmField::Declared, 1> structure{
        {{.Layer = "building",
          .Key = "kind",
          .Value = "hall",
          .HeightM = 9,
          .Area = true,
          .LatLon = {47.0, 9.0, 47.0, 9.001, 47.001, 9.001, 47.001, 9.0}}}};
    ingest.Declare(structure, TileAt{.X = 8581, .Y = 5603});
    const uint64_t oldGeneration = ingest.Generation();
    const Data::TileSourceIdentity heightSource{.Kind = Data::DataKind::Elevation,
                                                .Tile = {.Zoom = 14, .X = 8581, .Y = 5603},
                                                .SourceId = "dem",
                                                .Revision = "height-a"};
    const BuildingField::Baked emptyBake;
    footprints.PreparesAcceptances({.Tiles = 1});
    footprints.Take(0);
    auto acceptance = footprints.PrepareAcceptance(
        0,
        emptyBake,
        std::span(&heightSource, 1),
        true,
        ingest.Tiles().front().Source,
        {.HeightRasterDigest = 17, .StreetDigest = 19, .FocalPx = 90, .TileSpanM = 100});
    footprints.CommitAcceptance(std::move(acceptance), ingest, emptyBake);
    old = std::make_shared<const PublishedRegion>(RegionSources::Snapshot(&ingest, ways, water),
                                                  footprints.SnapshotAccepted());
    CHECK(old->Vectors() && old->Vectors()->Generation() == oldGeneration &&
              old->Vectors()->Tiles().front().Source.From ==
                  Data::TileSourceIdentity::Origin::Declared &&
              old->Vectors()->Str(old->Vectors()->Features().front(), "kind") == "hall" &&
              old->Footprints().InputOfTile(0) &&
              old->Footprints().InputOfTile(0)->Sources.front().Revision == "height-a",
          "the published region pins native features, tags and accepted source identity");

    ingest.Declare(std::span<const OsmField::Declared>{}, TileAt{.X = 8582, .Y = 5603});
    footprints.ResetDerived();
    next = std::make_shared<const PublishedRegion>(RegionSources::Snapshot(&ingest, ways, water),
                                                   footprints.SnapshotAccepted());
    CHECK(next->Vectors() && next->Vectors()->Features().empty() &&
              next->Vectors()->Tiles().size() == 1 &&
              next->Vectors()->Tiles().front().FeatureCount == 0 &&
              next->Vectors()->Tiles().front().X == 8582,
          "an empty replacement tile retains its identity without inheriting old features");
    CHECK(old->Vectors()->Tiles().front().X == 8581 &&
              old->Vectors()->Str(old->Vectors()->Features().front(), "kind") == "hall" &&
              old->Footprints().InputOfTile(0) && !next->Footprints().InputOfTile(0),
          "ingest replacement cannot mutate the previous published region");
  }
  CHECK(old && next && old->Vectors()->Features().size() == 1 &&
            next->Vectors()->Features().empty() && old->HeapBytes() > next->HeapBytes(),
        "both generations remain independently queryable after ingest destruction");
  return Report();
}
