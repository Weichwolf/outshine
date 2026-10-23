#include "PublishedRegion.h"
#include "WaterField.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  constexpr TileAt firstTile{.X = 8581, .Y = 5603};
  constexpr TileAt replacementTile{.X = 8582, .Y = 5603};

  std::shared_ptr<const ::outshine::Generators::Osm::PublishedRegion> old;
  std::shared_ptr<const ::outshine::Generators::Osm::PublishedRegion> next;
  {
    const std::array<std::string, 1> layers{"building"};
    ::outshine::Generators::Osm::OsmField ingest(14, layers);
    const ::outshine::Generators::Osm::StreetField ways;
    const ::outshine::Generators::Osm::WaterField water;
    ::outshine::Generators::Osm::BuildingField footprints;
    footprints.AnchorAt({{1, 2, 3}});
    const std::array<::outshine::Generators::Osm::OsmField::Declared, 1> structure{
        {{.Layer = "building",
          .Key = "kind",
          .Value = "hall",
          .HeightM = 9,
          .Area = true,
          .LatLon = {47.0, 9.0, 47.0, 9.001, 47.001, 9.001, 47.001, 9.0}}}};
    ingest.Declare(structure, firstTile);
    const uint64_t oldGeneration = ingest.Generation();
    const Data::TileSourceIdentity heightSource{.Kind = Data::DataKind::Elevation,
                                                .Tile = {.Zoom = 14, .X = 8581, .Y = 5603},
                                                .SourceId = "dem",
                                                .Revision = "height-a"};
    const ::outshine::Generators::Osm::BuildingField::Baked emptyBake;
    footprints.PreparesAcceptances(1);
    footprints.Take(0);
    const ::outshine::Generators::Osm::BuildingField::BakeInputs bakeInputs{
        .HeightRasterDigest = 17,
        .StreetDigest = 19,
        .Projection = {.FocalPx = 90},
        .TileSpanM = 100,
        .Eye = {},
        .EyeEcef = {}};
    auto acceptance = footprints.PrepareAcceptance(
        0, emptyBake, std::span(&heightSource, 1), true, ingest.Tiles().front().Source, bakeInputs);
    footprints.CommitAcceptance(std::move(acceptance), emptyBake);
    old = std::make_shared<const ::outshine::Generators::Osm::PublishedRegion>(
        ::outshine::Generators::Osm::RegionSources::Snapshot(&ingest, ways, water.Asset(ingest)),
        footprints.SnapshotAccepted());
    const auto reused = ::outshine::Generators::Osm::RegionSources::Snapshot(
        &ingest, ways, water.Asset(ingest), old->VectorSnapshot());
    CHECK(reused.Vectors.get() == old->Vectors(),
          "a new region reuses the published immutable vector owner");
    const auto absent = ::outshine::Generators::Osm::RegionSources::Snapshot(
        nullptr, ways, water.Asset(ingest), old->VectorSnapshot());
    CHECK(!absent.Vectors, "an absent vector source cannot inherit the previous publication");
    CHECK(old->MatchesLiveSources(&ingest, footprints),
          "a published region accepts live detail from its unchanged semantic sources");
    ::outshine::Generators::Osm::BuildingField changedFootprints = footprints.SnapshotAccepted();
    changedFootprints.ResetDerived();
    CHECK(!old->MatchesLiveSources(&ingest, changedFootprints),
          "changed footprint semantics cannot publish live detail into the older region");
    ::outshine::Generators::Osm::OsmField otherIngest(14, layers);
    auto changedStructure = structure;
    changedStructure[0].HeightM = 15;
    otherIngest.Declare(changedStructure, firstTile);
    CHECK(otherIngest.Generation() == ingest.Generation() &&
              otherIngest.Tiles().front().Source == ingest.Tiles().front().Source &&
              otherIngest.Tiles().front().FeatureCount == ingest.Tiles().front().FeatureCount &&
              !old->MatchesLiveSources(&otherIngest, footprints),
          "equal counters and tile identities from another source do not authorize publication");
    CHECK(old->Vectors() && old->Vectors()->Generation() == oldGeneration &&
              old->Vectors()->Tiles().front().Source.From ==
                  Data::TileSourceIdentity::Origin::Declared &&
              old->Vectors()->Str(old->Vectors()->Features().front(), "kind") == "hall" &&
              old->Footprints().InputOfTile(0) &&
              old->Footprints().InputOfTile(0)->Sources.front().Revision == "height-a",
          "the published region pins native features, tags and accepted source identity");

    ingest.Declare(std::span<const ::outshine::Generators::Osm::OsmField::Declared>{},
                   replacementTile);
    CHECK(!old->MatchesLiveSources(&ingest, footprints),
          "changed vector generation cannot publish live detail into the older region");
    footprints.ResetDerived();
    next = std::make_shared<const ::outshine::Generators::Osm::PublishedRegion>(
        ::outshine::Generators::Osm::RegionSources::Snapshot(&ingest, ways, water.Asset(ingest)),
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
