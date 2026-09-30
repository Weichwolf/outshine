#include "StructureArtifact.h"
#include "OsmXmlReader.h"
#include "Check.h"

#include <memory>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  auto parsed = Data::OsmXmlReader::Read(
      R"(<osm version="0.6"><node id="1" lat="0" lon="0"><tag k="name" v="retained"/></node></osm>)",
      {.DatasetId = "native-buildings", .Revision = "one"});
  CHECK(parsed.has_value(), "original fixture parses");
  if (!parsed) { return Report(); }
  auto source = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*parsed), .Coverage = {}});
  const std::weak_ptr<const Data::OsmSourceSnapshot> held = source;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "DEM fixture is available");
  if (!heights) { return Report(); }
  RawTile raw;
  raw.Original = {.Snapshot = source,
                  .Bounds = {.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}};
  raw.Structures.push_back({.OriginalId = {.Kind = Data::OsmElementKind::Node, .Id = 1}});
  const auto key = StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test");
  raw.Structures.front().OriginalId.Kind = Data::OsmElementKind::Way;
  CHECK(key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "equal numeric IDs of different OSM element kinds cannot alias a product");
  raw.Structures.front().OriginalId.Kind = Data::OsmElementKind::Node;
  raw.Structures.front().OriginalId.Id = 2;
  CHECK(key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "different original elements cannot share product identity");
  raw.Structures.front().OriginalId.Id = 1;
  RawTile copy = raw;
  source.reset();
  raw.Original = {};
  CHECK(!held.expired() &&
            copy.Original.Snapshot->Elements.FindNode(1)->Tags.front().Value == "retained",
        "copied worker input retains original tags after producer release");
  CHECK(key && key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "original and unqualified inputs cannot share product identity");
  copy.Original.Bounds.EastDeg = 2;
  CHECK(key != StructureArtifactKey(copy, *heights, std::nullopt, "native-input-test"),
        "native source coverage participates in product identity");
  auto next = Data::OsmXmlReader::Read(R"(<osm version="0.6"/>)",
                                       {.DatasetId = "native-buildings", .Revision = "two"});
  CHECK(next.has_value(), "replacement fixture parses");
  if (!next) { return Report(); }
  copy.Original.Bounds.EastDeg = 1;
  copy.Original.Snapshot = std::make_shared<const Data::OsmSourceSnapshot>(
      Data::OsmSourceSnapshot{.Elements = std::move(*next), .Coverage = {}});
  CHECK(key != StructureArtifactKey(copy, *heights, std::nullopt, "native-input-test"),
        "original revision changes identity even when mesh parameters are equal");
  CHECK(held.expired(), "last worker release retires the original source owner");
  return Report();
}
