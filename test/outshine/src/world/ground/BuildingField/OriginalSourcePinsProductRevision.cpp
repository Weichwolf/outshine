#include "BuildingField.h"
#include "Check.h"
#include "OsmXmlReader.h"

#include <memory>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Field = Ground::BuildingField;
  auto parsed = Data::OsmXmlReader::Read(
      R"(<osm version="0.6"><node id="1" lat="0" lon="0"><tag k="name" v="original"/></node></osm>)",
      {.DatasetId = "original-osm", .Revision = "one"});
  CHECK(parsed.has_value(), "original source fixture parses");
  if (!parsed) { return Report(); }
  auto source = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*parsed)});
  const std::weak_ptr<const Data::OsmSourceSnapshot> held = source;
  auto geometry = std::make_shared<Field::Geometry>();
  geometry->Original = {.Snapshot = source,
                        .Bounds = {.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}};
  Field field;
  Field::Baked product{.Coordinates = geometry};
  field.PreparesAcceptances({.Tiles = 1});
  field.Take(0);
  auto pending = field.PrepareAcceptance(0, product, {}, true);
  field.CommitAcceptance(std::move(pending), product);
  CHECK(field.AcceptedTiles().size() == 1,
        "native product publication retains its reservation without a vector-tile field");
  const uint64_t firstKey = field.InputOfTile(0)->SourceKey;
  const uint64_t firstRevision = field.Revision();
  Field snapshot = field.SnapshotAccepted();
  source.reset();
  geometry = std::make_shared<Field::Geometry>(*geometry);
  geometry->Original.Bounds.EastDeg = 2;
  product.Coordinates = geometry;
  auto changed = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(changed), product);
  CHECK(field.InputOfTile(0)->SourceKey != firstKey && field.Revision() != firstRevision,
        "different original source coverage invalidates an otherwise equal product");
  auto next = Data::OsmXmlReader::Read(R"(<osm version="0.6"><node id="1" lat="0" lon="0"/></osm>)",
                                       {.DatasetId = "original-osm", .Revision = "two"});
  CHECK(next.has_value(), "replacement original source fixture parses");
  if (!next) { return Report(); }
  geometry = std::make_shared<Field::Geometry>(*geometry);
  geometry->Original.Snapshot = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*next)});
  geometry->Original.Bounds.EastDeg = 1;
  product.Coordinates = geometry;
  auto replacement = field.PrepareAcceptance(0, product, {}, true);
  field.ReplaceAcceptance(std::move(replacement), product);
  CHECK(field.InputOfTile(0)->SourceKey != firstKey,
        "original dataset revision participates without a vector tile surrogate");
  const auto *old = snapshot.InputOfTile(0);
  CHECK(!held.expired() && old && old->SourceKey == firstKey && old->Coordinates &&
            old->Coordinates->Original.Snapshot->Elements.FindNode(1)->Tags.front().Value ==
                "original",
        "published snapshot retains original elements and tags after producer replacement");
  snapshot.ResetDerived();
  CHECK(held.expired(), "retiring the last product releases its original snapshot");
  return Report();
}
