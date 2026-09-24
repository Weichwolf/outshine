#include "Check.h"
#include "EarthworkPress.h"
#include "HeightField.h"
#include "Json.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"
#include "RoadTerrainContact.h"
#include "Sha256.h"
#include "TerrainGrid.h"
#include "TerrainMesh.h"
#include "TerrainPress.h"
#include "TerrainRefinement.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <optional>
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

[[nodiscard]] std::optional<double> TriangleHeight(const outshine::Generators::TerrainMesh &mesh,
                                                   std::array<uint32_t, 3> vertices,
                                                   outshine::EastNorth point) {
  const auto coordinate = [&](uint32_t vertex, size_t component) {
    return static_cast<double>(mesh.PositionsM[static_cast<size_t>(vertex) * 3u + component]);
  };
  const double ax = coordinate(vertices[0], 0);
  const double az = -coordinate(vertices[0], 2);
  const double bx = coordinate(vertices[1], 0);
  const double bz = -coordinate(vertices[1], 2);
  const double cx = coordinate(vertices[2], 0);
  const double cz = -coordinate(vertices[2], 2);
  const double cross = (bx - ax) * (cz - az) - (bz - az) * (cx - ax);
  if (std::abs(cross) < 1e-9) { return std::nullopt; }
  const double b = ((point.EastM - ax) * (cz - az) - (point.NorthM - az) * (cx - ax)) / cross;
  const double c = ((bx - ax) * (point.NorthM - az) - (bz - az) * (point.EastM - ax)) / cross;
  const double a = 1.0 - b - c;
  if (std::min({a, b, c}) < -1e-5) { return std::nullopt; }
  return a * coordinate(vertices[0], 1) + b * coordinate(vertices[1], 1) +
         c * coordinate(vertices[2], 1);
}

[[nodiscard]] std::optional<double> MeshHeightAt(const outshine::Generators::TerrainMesh &mesh,
                                                 const outshine::Patchwork &candidate,
                                                 const outshine::TangentFrame &frame,
                                                 outshine::EastNorth point,
                                                 int side) {
  const outshine::LongitudeLatitude geo = frame.ApproximateGeographicAt(point);
  for (size_t page = 0; page < candidate.Sheets.size(); ++page) {
    const outshine::Sheet &sheet = candidate.Sheets[page];
    const outshine::Ground::TileFrac tile = outshine::Ground::ToTileFracClamped(
        {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}, sheet.Tile.Zoom);
    const double x = tile.X - static_cast<double>(sheet.Tile.X);
    const double y = tile.Y - static_cast<double>(sheet.Tile.Y);
    if (x < -0.01 || x > 1.01 || y < -0.01 || y > 1.01) { continue; }
    const int column =
        std::clamp(static_cast<int>(std::floor(x * static_cast<double>(side - 1))), 0, side - 2);
    const int row =
        std::clamp(static_cast<int>(std::floor(y * static_cast<double>(side - 1))), 0, side - 2);
    for (int dy = -1; dy <= 1; ++dy) {
      for (int dx = -1; dx <= 1; ++dx) {
        const int cellX = column + dx;
        const int cellY = row + dy;
        if (cellX < 0 || cellX >= side - 1 || cellY < 0 || cellY >= side - 1) { continue; }
        const size_t first = (page * static_cast<size_t>(side - 1) * static_cast<size_t>(side - 1) +
                              static_cast<size_t>(cellY) * static_cast<size_t>(side - 1) +
                              static_cast<size_t>(cellX)) *
                             6u;
        for (size_t triangle = 0; triangle < 2; ++triangle) {
          const size_t at = first + triangle * 3u;
          if (auto height = TriangleHeight(
                  mesh, {mesh.Indices[at], mesh.Indices[at + 1], mesh.Indices[at + 2]}, point)) {
            return height;
          }
        }
      }
    }
  }
  for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3) {
    if (auto height = TriangleHeight(
            mesh, {mesh.Indices[index], mesh.Indices[index + 1], mesh.Indices[index + 2]}, point)) {
      return height;
    }
  }
  return std::nullopt;
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
  std::vector<Ground::TerrainGrid> sourceGrids;
  std::vector<Sheet> sourceSheets;
  sourceGrids.reserve(tiles.Size());
  sourceSheets.reserve(tiles.Size());
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
    sourceSheets.push_back({.Tile = address, .Side = 33, .Postings = 33});
    sourceGrids.push_back(std::move(grid));
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

  constexpr TerrainPageLayout layout{.Side = 33, .Halo = 1};
  std::vector<TerrainRefinementSource> sources;
  sources.reserve(sourceSheets.size());
  for (size_t index = 0; index < sourceSheets.size(); ++index) {
    sources.push_back({.Page = &sourceSheets[index], .Heights = sourceGrids[index].TryField()});
  }
  std::vector<TerrainRefinementCorridor> corridors;
  corridors.reserve(surface->Spans.size());
  for (size_t index = 0; index < surface->Spans.size(); ++index) {
    const size_t at = index * 12;
    const auto centre = [&](size_t offset) {
      return EastNorth{.EastM = 0.5 * (positions[at + offset] + positions[at + 3 + offset]),
                       .NorthM = -0.5 * (positions[at + offset + 2] + positions[at + 5 + offset])};
    };
    corridors.push_back({.Start = centre(0),
                         .End = centre(6),
                         .HalfWidthM = 26.0,
                         .MaximumPostingM = RoadTerrainContact::MaximumPostingM});
  }
  const auto refined = RefineTerrain(
      sources, frame, layout, {.OrthographicPxPerM = 1.0, .ErrorPx = 1e6}, 512, corridors);
  CHECK(refined && refined->size() > sourceSheets.size(),
        "the production selector spends fine terrain patches along the sourced route");
  if (!refined) { return Report(); }
  Patchwork candidate{.Sheets = *refined};
  for (Sheet &sheet : candidate.Sheets) {
    const auto sourceAt = std::ranges::find_if(sourceSheets, [&](const Sheet &sourceSheet) {
      const auto shift = static_cast<uint32_t>(sheet.Tile.Zoom - sourceSheet.Tile.Zoom);
      return sheet.Tile.X >> shift == sourceSheet.Tile.X &&
             sheet.Tile.Y >> shift == sourceSheet.Tile.Y;
    });
    CHECK(sourceAt != sourceSheets.end(), "every selected page has a pinned source ancestor");
    if (sourceAt == sourceSheets.end()) { return Report(); }
    const size_t sourceIndex = static_cast<size_t>(sourceAt - sourceSheets.begin());
    const Ground::TerrainField *sourceField = sourceGrids[sourceIndex].TryField();
    sheet.Nodes.assign(layout.NodeCount(), std::numeric_limits<float>::quiet_NaN());
    for (int row = -layout.Halo; row < layout.Side + layout.Halo; ++row) {
      for (int column = -layout.Halo; column < layout.Side + layout.Halo; ++column) {
        const Ground::Geo geo = Ground::TileFracToGeo(
            {.X = static_cast<double>(sheet.Tile.X) + layout.FractionAt(sheet, column),
             .Y = static_cast<double>(sheet.Tile.Y) + layout.FractionAt(sheet, row)},
            sheet.Tile.Zoom);
        const auto aslM =
            terrain->At({.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}).AslM();
        const Ground::TileFrac fraction = Ground::ToTileFracClamped(geo, 15);
        const float ancestorM = sourceField->PostingM(
            {.Col = std::clamp(fraction.X - static_cast<double>(sourceAt->Tile.X), 0.0, 1.0),
             .Row = std::clamp(fraction.Y - static_cast<double>(sourceAt->Tile.Y), 0.0, 1.0)});
        sheet.Nodes[layout.NodeAt(column, row)] = static_cast<float>(aslM.value_or(ancestorM));
      }
    }
  }
  const auto meshPress =
      PressTerrain(surface->Earthworks, candidate, frame, layout, kMostEarthworkM);
  const auto mesh = BuildTerrainMesh(candidate, frame, layout);
  CHECK(meshPress.Nodes > 0 && mesh.PositionsM.size() == candidate.Sheets.size() * 33u * 33u * 3u,
        "production terrain press and triangulation cover each selected page");
  double minimumTriangleClearanceM = std::numeric_limits<double>::infinity();
  size_t worstTriangleSample = 0;
  for (size_t index = 0; index < points.size(); ++index) {
    const auto terrainM = MeshHeightAt(mesh, candidate, frame, points[index], layout.Side);
    if (!terrainM) {
      Note("missing mesh sample index", static_cast<double>(index), "sample");
      Note("missing mesh east", points[index].EastM, "m");
      Note("missing mesh north", points[index].NorthM, "m");
      const auto geo = frame.ApproximateGeographicAt(points[index]);
      const auto tile = Ground::ToTileFracClamped(
          {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg}, 15);
      Note("missing mesh tile x", tile.X, "tile");
      Note("missing mesh tile y", tile.Y, "tile");
      CHECK(false, "every road sample intersects a drawn terrain triangle");
      return Report();
    }
    const double clearanceM = roadHeights[index] - *terrainM;
    if (clearanceM < minimumTriangleClearanceM) {
      minimumTriangleClearanceM = clearanceM;
      worstTriangleSample = index;
    }
  }
  Note("real DEM minimum triangulated clearance", minimumTriangleClearanceM, "m");
  Note("real DEM worst triangle sample", static_cast<double>(worstTriangleSample), "sample");
  CHECK(minimumTriangleClearanceM >= 0.0,
        "the refined and pressed terrain triangles do not occlude the road surface");
  return Report();
}
