#include "Check.h"
#include "EarthworkPress.h"
#include "HeightField.h"
#include "Json.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"
#include "Sha256.h"
#include "TerrainGrid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace {

[[nodiscard]] std::string ReadFile(const std::string &path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) { return {}; }
  return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Data;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  using namespace outshine::World;

  constexpr auto fixture = "test/outshine/data/terrain/hockenheim/";
  const std::string tileSetText = ReadFile(std::string(fixture) + "tiles.json");
  Json tileSet;
  CHECK(tileSet.Parse(tileSetText.data(), tileSetText.size()),
        "terrain tile-set description parses");
  if (!tileSet.Ok()) { return Report(); }
  const auto tiles = tileSet.Root()["tiles"];
  CHECK(tiles.Size() == 6 && tileSet.Root()["source_id"].Str() == "terrarium.s3",
        "six pinned Terrarium source tiles cover the route");
  if (tiles.Size() != 6) { return Report(); }
  std::vector<Ground::HeightField::Block> blocks;
  std::set<std::pair<int, int>> seen;
  for (size_t index = 0; index < tiles.Size(); ++index) {
    const auto tile = tiles[index];
    const int zoom = tile["z"].Int(-1);
    const int x = tile["x"].Int(-1);
    const int y = tile["y"].Int(-1);
    CHECK(zoom == 15 && x >= 17163 && x <= 17165 && y >= 11206 && y <= 11207 &&
              seen.insert({x, y}).second,
          "fixture tile addresses are unique and inside the declared route coverage");
    const auto file = std::string(fixture) + std::to_string(x) + "-" + std::to_string(y) + ".png";
    const std::string png = ReadFile(file);
    CHECK(!png.empty() && png.size() == static_cast<size_t>(tile["bytes"].Int(-1)) &&
              Sha256Hex(png) == tile["sha256"].Str(),
          "each raw DEM tile matches its immutable tile-set pin");
    if (png.empty()) { return Report(); }
    Ground::TerrainGrid grid = Ground::TerrainGrid::FromTerrariumPng(
        reinterpret_cast<const uint8_t *>(png.data()), png.size());
    Ground::TerrainField *field = grid.TryFieldMutable();
    CHECK(field && field->Rows() == 256 && field->Cols() == 256,
          "production Terrarium decoder reads the source raster");
    if (!field) { return Report(); }
    const TileId address{
        .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
    field->AddSource({.From = TileSourceIdentity::Origin::Provider,
                      .Kind = DataKind::Elevation,
                      .Tile = address,
                      .SourceId = "terrarium.s3"});
    Ground::HeightField::Block block;
    CHECK(Ground::HeightField::CopiesField(*field, address, block),
          "decoded DEM retains its tile source identity");
    blocks.push_back(std::move(block));
  }
  CHECK(seen.size() == 6, "every terrain fixture tile is distinct");
  const auto terrain = Ground::HeightField::Of(15, std::move(blocks));
  CHECK(terrain->Qualified() && terrain->Sources().size() == 6,
        "route terrain has six immutable provider identities");

  const std::string xml = ReadFile("src/assets/world/osm/HockenheimringGrandPrix.osm");
  CHECK(!xml.empty(), "pinned OSM circuit is present");
  if (xml.empty()) { return Report(); }
  const auto source = OsmXmlReader::Read(xml, {.DatasetId = "openstreetmap", .Revision = "pin-r1"});
  CHECK(source.has_value(), "pinned OSM circuit parses");
  if (!source) { return Report(); }
  const auto topology = TransportTopology::Build(*source);
  CHECK(topology.has_value(), "pinned transport topology builds");
  if (!topology) { return Report(); }
  const auto route = topology->ResolveCircuit(*source, 284588);
  CHECK(route.has_value() && route->EdgeIds.size() == 267,
        "the 267-edge closed route resolves without pitlane");
  if (!route) { return Report(); }
  const auto constraints =
      RoadConstraintChain::Build(*topology, route->SourceIdentity, route->EdgeIds, *terrain);
  CHECK(constraints.has_value(), "every source edge samples pinned real DEM");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment && alignment->Closed() && alignment->Edges().size() == 267,
        "real DEM yields one complete periodic native alignment");
  if (!alignment) { return Report(); }
  const TangentFrame frame = TangentFrame::At(alignment->Anchor());
  const auto surface = RoadSurfaceBuilder::Build(*alignment, frame);
  CHECK(surface && surface->SurfaceGeometry.wellFormed() &&
            surface->Spans.size() == surface->Earthworks.size(),
        "real DEM alignment yields complete native road and earthwork products");
  if (!surface) { return Report(); }
  double lowestM = 1e9;
  double highestM = -1e9;
  for (const RoadSurfaceSpan &span : surface->Spans) {
    const auto pose = alignment->AtStation((span.StartStationM + span.EndStationM) * 0.5);
    CHECK(pose.has_value(), "every rendered interval has a sourced route pose");
    if (!pose) { return Report(); }
    lowestM = std::min(lowestM, pose->PositionM.UpM);
    highestM = std::max(highestM, pose->PositionM.UpM);
  }
  Note("real DEM road height minimum", lowestM, "m");
  Note("real DEM road height maximum", highestM, "m");
  CHECK(highestM - lowestM > 1.0,
        "the real DEM changes road elevation rather than acting as a flat fixture");

  const auto positions = surface->SurfaceGeometry.positionsOf(0);
  constexpr std::array stationFractions{0.0, 0.25, 0.5, 0.75, 1.0};
  constexpr std::array widthFractions{0.0, 0.5, 1.0};
  std::vector<EastNorth> points;
  std::vector<double> roadHeights;
  std::vector<double> groundHeights;
  points.reserve(surface->Spans.size() * stationFractions.size() * widthFractions.size());
  roadHeights.reserve(points.capacity());
  groundHeights.reserve(points.capacity());
  double greatestRawDifferenceM = 0.0;
  for (size_t index = 0; index < surface->Spans.size(); ++index) {
    const size_t at = index * 12;
    for (const double station : stationFractions) {
      for (const double width : widthFractions) {
        const auto coordinate = [&](size_t component) {
          const double left = std::lerp(static_cast<double>(positions[at + component]),
                                        static_cast<double>(positions[at + 6 + component]),
                                        station);
          const double right = std::lerp(static_cast<double>(positions[at + 3 + component]),
                                         static_cast<double>(positions[at + 9 + component]),
                                         station);
          return std::lerp(left, right, width);
        };
        const EastNorth point{.EastM = coordinate(0), .NorthM = -coordinate(2)};
        const LongitudeLatitude geographic = frame.ApproximateGeographicAt(point);
        const auto aslM = terrain->At(geographic).AslM();
        if (!aslM) {
          CHECK(false, "every rendered road cross-section has real DEM coverage");
          return Report();
        }
        const double groundM = frame
                                   .ToLocalPosition({.LongitudeDeg = geographic.LongitudeDeg,
                                                     .LatitudeDeg = geographic.LatitudeDeg,
                                                     .HeightM = *aslM})
                                   .UpM;
        const double roadM = coordinate(1);
        points.push_back(point);
        roadHeights.push_back(roadM);
        groundHeights.push_back(groundM);
        greatestRawDifferenceM = std::max(greatestRawDifferenceM, std::abs(roadM - groundM));
      }
    }
  }
  Note("real DEM largest pre-press road offset", greatestRawDifferenceM, "m");
  CHECK(greatestRawDifferenceM > 0.2,
        "real DEM contact requires a measurable cut or fill rather than coincidental flatness");
  const auto pressed =
      ApplyEarthworkStamps(surface->Earthworks, points, groundHeights, kMostEarthworkM);
  double minimumClearanceM = 1e9;
  double maximumClearanceM = -1e9;
  for (size_t index = 0; index < points.size(); ++index) {
    const double clearanceM = roadHeights[index] - groundHeights[index];
    minimumClearanceM = std::min(minimumClearanceM, clearanceM);
    maximumClearanceM = std::max(maximumClearanceM, clearanceM);
  }
  Note("real DEM minimum post-press clearance", minimumClearanceM, "m");
  Note("real DEM maximum post-press clearance", maximumClearanceM, "m");
  CHECK(pressed.Moved > 0 && minimumClearanceM >= 0.02 && maximumClearanceM <= 0.15,
        "real DEM road surface clears its cut and filled terrain at every sampled section");
  return Report();
}
