#include "OriginalBuildingInput.h"
#include "OsmStructureDescription.h"
#include "BuildingMesh.h"
#include "Geodesy.h"
#include "OsmXmlReader.h"
#include "StructureSourceKey.h"
#include "Check.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  std::weak_ptr<const Data::OsmSourceSnapshot> archive;
  size_t archiveBytes = 0;
  std::optional<RawTile> input;
  {
    std::string xml =
        "<osm version='0.6'><node id='1' lat='0' lon='0'><tag k='survey' v='original'/></node>"
        "<node id='2' lat='0' lon='0.001'/><node id='3' lat='0.001' lon='0.001'/>"
        "<node id='4' lat='0.001' lon='0'/>"
        "<way id='10'><nd ref='1'/><nd ref='2'/><nd ref='3'/><nd ref='4'/><nd ref='1'/>"
        "<tag k='building' v='yes'/><tag k='height' v='8'/>"
        "<tag k='unrecognized:property' v='preserved'/></way>";
    for (unsigned id = 100; id < 612; ++id) {
      xml += "<node id='" + std::to_string(id) + "' lat='1' lon='1'><tag k='description' v='" +
             std::string(256, 'x') + "'/></node>";
    }
    xml += "<relation id='900'><member type='way' ref='999' role='unrelated'/></relation></osm>";
    auto elements = Data::OsmXmlReader::Read(xml, {.DatasetId = "original", .Revision = "r1"});
    CHECK(elements.has_value(), "original archive includes unrelated tags and an incomplete route");
    if (!elements) { return Report(); }
    const auto source = std::make_shared<const Data::OsmSourceSnapshot>(Data::OsmSourceSnapshot{
        .Elements = std::move(*elements),
        .Coverage = {{.WestDeg = 0, .SouthDeg = 0, .EastDeg = 1, .NorthDeg = 1}},
        .SourceBytes = xml.size(),
        .Chunks = {{.Location = "official-original-cell", .PayloadSha256 = std::string(64, 'a')}},
        .Cell = Data::GeoCellId{.Level = 9, .X = 256, .Y = 256}});
    archive = source;
    archiveBytes = source->StorageChargeBytes();
    const auto footprints = outshine::Generators::Osm::BuildingFootprints::Build(source, 4);
    CHECK(footprints.has_value(),
          "building footprint is closed independently of the unrelated route");
    if (!footprints) { return Report(); }
    auto prepared = outshine::Test::OriginalBuildingInput(
        *footprints,
        {.Snapshot = source, .Origin = {.Bounds = source->Coverage.front()}},
        {.Heights = {.StoreyHeightM = 3, .BodyHeightM = 9}, .PointsMost = 4});
    CHECK(prepared.has_value(), "building product takes ownership of its required inputs");
    if (!prepared) { return Report(); }
    input = std::move(*prepared);
    CHECK(source->Elements.Nodes().size() == 516 && source->Elements.Relations().size() == 1,
          "product preparation preserves the complete original archive for other consumers");
  }
  CHECK(archive.expired() && input->Original.Archive.expired(),
        "resident building input does not extend the complete cell archive lifetime");
  const auto &native = *input->Original.Snapshot;
  CHECK(native.Elements.Nodes().size() == 4 && native.Elements.Ways().size() == 1 &&
            native.Elements.Relations().empty() && !native.Elements.FirstMissingReference(),
        "resident native input owns exactly the complete building dependency closure");
  CHECK(native.Elements.FindNode(1)->Tags.front().Value == "original" &&
            native.Elements.FindWay(10)->Tags.back().Value == "preserved" &&
            native.Chunks.front().PayloadSha256 == std::string(64, 'a') && native.Cell &&
            native.Cell->Level == 9 && native.Elements.SourceIdentity().Revision == "r1",
        "unknown consumed tags and original provenance survive archive retirement");
  CHECK(native.StorageChargeBytes() * 10 < archiveBytes,
        "the controlled fixture reduces retained storage by more than tenfold without input loss");
  input->TileSpanM = 1000;
  GeoToEcef({.LongitudeDeg = 0, .LatitudeDeg = 0, .HeightM = 100}, input->AnchorEcef);
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 100);
  const auto heights = Ground::HeightField::Of(0, {block});
  CHECK(heights != nullptr, "independent terrain remains available");
  if (!heights) { return Report(); }
  BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  BakedTile baked;
  CHECK(BakeStructures(*input, *heights, mesher, *scratch, baked) && baked.Prints.size() == 1 &&
            baked.Prints.front().HeightM == 8 && !baked.Built.WallCorners.empty(),
        "actual geometry generation succeeds after the complete original archive is released");
  return Report();
}
