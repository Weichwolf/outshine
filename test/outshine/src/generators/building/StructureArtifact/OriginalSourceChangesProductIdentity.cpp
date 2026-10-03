#include "OriginalBuildingInput.h"
#include "OsmSourceProvenance.h"
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
  auto parsed = outshine::Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6"><node id="1" lat="0" lon="0"><tag k="name" v="retained"/></node></osm>)",
      {.DatasetId = "native-buildings", .Revision = "one"});
  CHECK(parsed.has_value(), "original fixture parses");
  if (!parsed) { return Report(); }
  auto source = std::make_shared<const outshine::Generators::Osm::SourceSnapshot>(
      outshine::Generators::Osm::SourceSnapshot{.Elements = std::move(*parsed), .Coverage = {}});
  const std::weak_ptr<const outshine::Generators::Osm::SourceSnapshot> held = source;
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "DEM fixture is available");
  if (!heights) { return Report(); }
  RawTile raw;
  raw.SourceInputs = {
      .Objects = std::make_shared<const outshine::Generators::Osm::SourceCapture>(source),
      .Origin = {.Provenance = outshine::Generators::Osm::DescribeOsmSource(*source),
                 .Bounds = {.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}}};
  raw.Structures.push_back(
      {.SourceId = {.Id = 1,
                    .Kind = static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Node)}});
  const auto key = StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test");
  raw.Structures.front().SourceId.Kind =
      static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Way);
  CHECK(key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "equal numeric IDs of different OSM element kinds cannot alias a product");
  raw.Structures.front().SourceId.Kind =
      static_cast<uint8_t>(outshine::Generators::Osm::ElementKind::Node);
  raw.Structures.front().SourceId.Id = 2;
  CHECK(key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "different original elements cannot share product identity");
  raw.Structures.front().SourceId.Id = 1;
  raw.Projection.AllowedErrorPx = 0.25;
  CHECK(key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "implicit view products include their pixel allowance in the input identity");
  raw.RequestedDetail = LevelOfDetail::Fine;
  const auto explicitKey = StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test");
  raw.Projection.AllowedErrorPx = 2.0;
  CHECK(explicitKey == StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "an explicit geometric variant remains independent of the view selection allowance");
  raw.RequestedDetail.reset();
  raw.Projection.AllowedErrorPx = 1.0;
  RawTile copy = raw;
  source.reset();
  raw.SourceInputs = {};
  CHECK(!held.expired() &&
            CapturedOsmSource(copy).Elements.FindNode(1)->Tags.front().Value == "retained",
        "copied worker input retains original tags after producer release");
  CHECK(key && key != StructureArtifactKey(raw, *heights, std::nullopt, "native-input-test"),
        "original and unqualified inputs cannot share product identity");
  copy.SourceInputs.Origin.Bounds.EastDeg = 2;
  copy.SourceInputs.Origin.Provenance =
      outshine::Generators::Osm::DescribeOsmSource(CapturedOsmSource(copy));
  CHECK(key != StructureArtifactKey(copy, *heights, std::nullopt, "native-input-test"),
        "native source coverage participates in product identity");
  auto next = outshine::Generators::Osm::XmlReader::Read(
      R"(<osm version="0.6"/>)", {.DatasetId = "native-buildings", .Revision = "two"});
  CHECK(next.has_value(), "replacement fixture parses");
  if (!next) { return Report(); }
  copy.SourceInputs.Origin.Bounds.EastDeg = 1;
  copy.SourceInputs.Objects = std::make_shared<const outshine::Generators::Osm::SourceCapture>(
      std::make_shared<const outshine::Generators::Osm::SourceSnapshot>(
          outshine::Generators::Osm::SourceSnapshot{.Elements = std::move(*next), .Coverage = {}}));
  copy.SourceInputs.Origin.Provenance =
      outshine::Generators::Osm::DescribeOsmSource(CapturedOsmSource(copy));
  CHECK(key != StructureArtifactKey(copy, *heights, std::nullopt, "native-input-test"),
        "original revision changes identity even when mesh parameters are equal");
  CHECK(held.expired(), "last worker release retires the original source owner");
  return Report();
}
