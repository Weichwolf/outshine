#include "HockenheimCircuitFixture.h"
#include "HockenheimTransportFixture.h"
#include "OsmTransportBuilder.h"
#include "Check.h"
#include "EarthworkDeformation.h"
#include "HeightField.h"
#include "Json.h"
#include "OsmXmlReader.h"
#include "RoadSurfaceBuilder.h"
#include "RoadTerrainContact.h"
#include "Sha256.h"
#include "TerrainGrid.h"
#include "TerrainMesh.h"
#include "TerrainDeformation.h"
#include "TerrainRefinement.h"
#include "TileGeodesy.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <fstream>
#include <ios>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace Circuit = outshine::Test::HockenheimFixture;

namespace {
constexpr double kDegenerateTriangleAreaM2 = 1e-9;
constexpr double kBarycentricSlack = 1e-5;
constexpr double kPressedImpulseLimitM = 0.01;
constexpr double kTileProbeMaximum = 1.01;
constexpr double kTileProbeSlack = 0.01;
constexpr int kSourceSide = 33;
constexpr int kShoulderIntervals = 400;
constexpr int kShoulderSamples = 401;
constexpr double kRawImpulseLimitM = 0.005;
constexpr double kShoulderWidthM = 10.0;
constexpr double kShoulderStartM = 1780.0;
constexpr double kShoulderStepM = 0.25;
constexpr double kHeightSentinelM = 1e9;
constexpr double kCurvatureLimitPerM = 0.015;
constexpr double kCoarseCurvatureStepM = 5.0;
constexpr double kMinimumRawOffsetM = 0.2;
constexpr double kMinimumClearanceM = 0.02;
constexpr double kMaximumClearanceM = 0.15;
constexpr double kCorridorHalfWidthM = 26.0;

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
  if (std::abs(cross) < kDegenerateTriangleAreaM2) { return std::nullopt; }
  const double b = ((point.EastM - ax) * (cz - az) - (point.NorthM - az) * (cx - ax)) / cross;
  const double c = ((bx - ax) * (point.NorthM - az) - (bz - az) * (point.EastM - ax)) / cross;
  const double a = 1.0 - b - c;
  if (std::min({a, b, c}) < -kBarycentricSlack) { return std::nullopt; }
  return a * coordinate(vertices[0], 1) + b * coordinate(vertices[1], 1) +
         c * coordinate(vertices[2], 1);
}

[[nodiscard]] std::optional<double> AnyTriangleHeight(const outshine::Generators::TerrainMesh &mesh,
                                                      outshine::EastNorth point) {
  for (size_t index = 0; index + 2 < mesh.Indices.size(); index += 3) {
    if (auto height = TriangleHeight(
            mesh, {mesh.Indices[index], mesh.Indices[index + 1], mesh.Indices[index + 2]}, point)) {
      return height;
    }
  }
  return std::nullopt;
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
    if (x < -kTileProbeSlack || x > kTileProbeMaximum || y < -kTileProbeSlack ||
        y > kTileProbeMaximum) {
      continue;
    }
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
  return AnyTriangleHeight(mesh, point);
}

}

namespace {
using namespace outshine;
using namespace outshine::Data;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::World;

struct TerrainFixture {
  std::shared_ptr<const Ground::HeightField> Heights;
  std::vector<Ground::TerrainGrid> Grids;
  std::vector<Sheet> Sheets;
};

struct ContactSamples {
  std::vector<EastNorth> Points;
  std::vector<double> RoadHeights;
};

struct DrawnTerrain {
  Patchwork Pages;
  TerrainMesh Mesh;
};

std::optional<TerrainFixture> LoadTerrain() {
  constexpr auto fixture = "test/outshine/data/terrain/hockenheim/";
  const std::string tileSetText = ReadFile(std::string(fixture) + "tiles.json");
  Json tileSet;
  CHECK(tileSet.Parse(tileSetText.data(), tileSetText.size()),
        "terrain tile-set description parses");
  if (!tileSet.Ok()) { return std::nullopt; }
  const auto tiles = tileSet.Root()["tiles"];
  CHECK(tiles.Size() == 6 && tileSet.Root()["source_id"].Str() == "terrarium.s3",
        "six pinned Terrarium source tiles cover the route");
  if (tiles.Size() != 6) { return std::nullopt; }
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
    if (png.empty()) { return std::nullopt; }
    Ground::TerrainGrid grid = Ground::TerrainGrid::FromTerrariumPng(
        reinterpret_cast<const uint8_t *>(png.data()), png.size());
    Ground::TerrainField *field = grid.TryFieldMutable();
    CHECK(field && field->Rows() == 256 && field->Cols() == 256,
          "production Terrarium decoder reads the source raster");
    if (field == nullptr) { return std::nullopt; }
    const TileId address{
        .Zoom = zoom, .X = static_cast<uint32_t>(x), .Y = static_cast<uint32_t>(y)};
    field->AddSource({.From = TileSourceIdentity::Origin::Provider,
                      .Kind = DataKind::Elevation,
                      .Tile = address,
                      .SourceId = "terrarium.s3",
                      .Revision = {}});
    Ground::HeightField::Block block;
    CHECK(Ground::HeightField::CopiesField(*field, address, block),
          "decoded DEM retains its tile source identity");
    blocks.push_back(std::move(block));
    sourceSheets.push_back(
        {.Tile = address, .Nodes = {}, .Side = kSourceSide, .Postings = kSourceSide});
    sourceGrids.push_back(std::move(grid));
  }
  CHECK(seen.size() == 6, "every terrain fixture tile is distinct");
  const auto terrain = Ground::HeightField::Of(15, std::move(blocks));
  CHECK(terrain->Qualified() && terrain->Sources().size() == 6,
        "route terrain has six immutable provider identities");

  return TerrainFixture{
      .Heights = terrain, .Grids = std::move(sourceGrids), .Sheets = std::move(sourceSheets)};
}

bool CheckShoulders(const RoadAlignment &alignment,
                    const RoadSurface &surface,
                    const Ground::HeightField &terrain,
                    const TangentFrame &frame) {
  double largestShoulderImpulseM = 0.0;
  double largestShoulderOffsetM = 0.0;
  double largestShoulderStationM = 0.0;
  for (const double offsetM :
       {-17.0, -16.0, -15.0, -14.0, -13.0,           -12.0, -11.0, -kShoulderWidthM,
        -9.0,  -8.0,  8.0,   9.0,   kShoulderWidthM, 11.0,  12.0,  13.0,
        14.0,  15.0,  16.0,  17.0}) {
    std::vector<EastNorth> shoulder;
    std::vector<double> raw;
    for (size_t sample = 0; sample <= kShoulderIntervals; ++sample) {
      const double stationM = kShoulderStartM + kShoulderStepM * static_cast<double>(sample);
      const auto pose = alignment.AtStation(stationM);
      CHECK(pose.has_value(), "shoulder probe station resolves on the pinned circuit");
      if (!pose) { return false; }
      const double length = std::hypot(pose->TangentEnu[0], pose->TangentEnu[1]);
      const EastNorth at{.EastM = pose->PositionM.EastM - pose->TangentEnu[1] / length * offsetM,
                         .NorthM = pose->PositionM.NorthM + pose->TangentEnu[0] / length * offsetM};
      const LongitudeLatitude geo = frame.ApproximateGeographicAt(at);
      const auto aslM = terrain.At(geo).AslM();
      CHECK(aslM.has_value(), "pinned DEM covers each shoulder probe station");
      if (!aslM) { return false; }
      shoulder.push_back(at);
      raw.push_back(frame
                        .ToLocalPosition({.LongitudeDeg = geo.LongitudeDeg,
                                          .LatitudeDeg = geo.LatitudeDeg,
                                          .HeightM = *aslM})
                        .UpM);
    }
    std::vector<double> pressedM = raw;
    const auto press =
        ApplyEarthworkStamps(surface.Earthworks, shoulder, pressedM, kMostEarthworkM);
    double rawImpulseM = 0.0;
    double pressedImpulseM = 0.0;
    size_t strongest = 0;
    for (size_t index = 1; index + 1 < raw.size(); ++index) {
      rawImpulseM =
          std::max(rawImpulseM, std::abs(raw[index + 1] - 2.0 * raw[index] + raw[index - 1]));
      const double impulse =
          std::abs(pressedM[index + 1] - 2.0 * pressedM[index] + pressedM[index - 1]);
      if (impulse > pressedImpulseM) {
        pressedImpulseM = impulse;
        strongest = index;
      }
    }
    CHECK(raw.size() == kShoulderSamples && rawImpulseM < kRawImpulseLimitM,
          "smooth pinned DEM isolates the earthwork field");
    if (std::abs(offsetM) == kShoulderWidthM) {
      CHECK(press.Moved == raw.size(), "both ten-metre shoulders remain fully pressed");
    }
    if (pressedImpulseM > largestShoulderImpulseM) {
      largestShoulderImpulseM = pressedImpulseM;
      largestShoulderOffsetM = offsetM;
      largestShoulderStationM = kShoulderStartM + kShoulderStepM * static_cast<double>(strongest);
    }
    CHECK(pressedImpulseM < kPressedImpulseLimitM,
          "profiled road earthworks keep real DEM shoulders free of segment-end impulses");
  }
  Note("strongest shoulder impulse", largestShoulderImpulseM, "m/0.25m");
  Note("strongest shoulder offset", largestShoulderOffsetM, "m");
  Note("strongest shoulder station", largestShoulderStationM, "m");
  return true;
}

bool CheckVerticalProfile(const RoadAlignment &alignment, const RoadSurface &surface) {
  double lowestM = kHeightSentinelM;
  double highestM = -kHeightSentinelM;
  for (const RoadSurfaceSpan &span : surface.Spans) {
    const auto pose = alignment.AtStation((span.StartStationM + span.EndStationM) * 0.5);
    CHECK(pose.has_value(), "every rendered interval has a sourced route pose");
    if (!pose) { return false; }
    lowestM = std::min(lowestM, pose->PositionM.UpM);
    highestM = std::max(highestM, pose->PositionM.UpM);
  }
  Note("real DEM road height minimum", lowestM, "m");
  Note("real DEM road height maximum", highestM, "m");
  CHECK(highestM - lowestM > 1.0,
        "the real DEM changes road elevation rather than acting as a flat fixture");
  constexpr double kProfileStepM = 0.5;
  double strongestVerticalCurvaturePerM = 0.0;
  double strongestVerticalCurvatureStationM = 0.0;
  for (size_t sample = 1; kProfileStepM * static_cast<double>(sample + 1) < alignment.LengthM();
       ++sample) {
    const double stationM = kProfileStepM * static_cast<double>(sample);
    const auto behind = alignment.AtStation(stationM - kProfileStepM);
    const auto here = alignment.AtStation(stationM);
    const auto ahead = alignment.AtStation(stationM + kProfileStepM);
    if (!behind || !here || !ahead) { continue; }
    const double curvaturePerM =
        std::abs(ahead->PositionM.UpM - 2.0 * here->PositionM.UpM + behind->PositionM.UpM) /
        (kProfileStepM * kProfileStepM);
    if (curvaturePerM > strongestVerticalCurvaturePerM) {
      strongestVerticalCurvaturePerM = curvaturePerM;
      strongestVerticalCurvatureStationM = stationM;
    }
  }
  Note("real DEM highest vertical curvature", strongestVerticalCurvaturePerM, "1/m");
  Note("real DEM highest vertical curvature station", strongestVerticalCurvatureStationM, "m");
  CHECK(strongestVerticalCurvaturePerM < kCurvatureLimitPerM,
        "nonuniform source-node spacing does not kink the drivable road grade");
  const auto maximumCurvatureAt = [&alignment](double stepM) {
    double greatest = 0.0;
    for (size_t sample = 1; stepM * static_cast<double>(sample + 1) < alignment.LengthM();
         ++sample) {
      const double stationM = stepM * static_cast<double>(sample);
      const auto behind = alignment.AtStation(stationM - stepM);
      const auto here = alignment.AtStation(stationM);
      const auto ahead = alignment.AtStation(stationM + stepM);
      if (!behind || !here || !ahead) { continue; }
      greatest = std::max(
          greatest,
          std::abs(ahead->PositionM.UpM - 2.0 * here->PositionM.UpM + behind->PositionM.UpM) /
              (stepM * stepM));
    }
    return greatest;
  };
  Note("real DEM vertical curvature at 2 m", maximumCurvatureAt(2.0), "1/m");
  Note("real DEM vertical curvature at 5 m", maximumCurvatureAt(kCoarseCurvatureStepM), "1/m");

  return true;
}

std::optional<ContactSamples> CheckContact(const RoadSurface &surface,
                                           const Ground::HeightField &terrain,
                                           const TangentFrame &frame) {
  const auto positions = surface.SurfaceGeometry.positionsOf(0);
  constexpr std::array stationFractions{0.0, kShoulderStepM, 0.5, 0.75, 1.0};
  constexpr std::array widthFractions{0.0, 0.5, 1.0};
  std::vector<EastNorth> points;
  std::vector<double> roadHeights;
  std::vector<double> groundHeights;
  points.reserve(surface.Spans.size() * stationFractions.size() * widthFractions.size());
  roadHeights.reserve(points.capacity());
  groundHeights.reserve(points.capacity());
  double greatestRawDifferenceM = 0.0;
  for (size_t index = 0; index < surface.Spans.size(); ++index) {
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
        const auto aslM = terrain.At(geographic).AslM();
        if (!aslM) {
          CHECK(false, "every rendered road cross-section has real DEM coverage");
          return std::nullopt;
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
  CHECK(greatestRawDifferenceM > kMinimumRawOffsetM,
        "real DEM contact requires a measurable cut or fill rather than coincidental flatness");
  const auto pressed =
      ApplyEarthworkStamps(surface.Earthworks, points, groundHeights, kMostEarthworkM);
  double minimumClearanceM = kHeightSentinelM;
  double maximumClearanceM = -kHeightSentinelM;
  for (size_t index = 0; index < points.size(); ++index) {
    const double clearanceM = roadHeights[index] - groundHeights[index];
    minimumClearanceM = std::min(minimumClearanceM, clearanceM);
    maximumClearanceM = std::max(maximumClearanceM, clearanceM);
  }
  Note("real DEM minimum post-press clearance", minimumClearanceM, "m");
  Note("real DEM maximum post-press clearance", maximumClearanceM, "m");
  CHECK(pressed.Moved > 0 && minimumClearanceM >= kMinimumClearanceM &&
            maximumClearanceM <= kMaximumClearanceM,
        "real DEM road surface clears its cut and filled terrain at every sampled section");

  return ContactSamples{.Points = std::move(points), .RoadHeights = std::move(roadHeights)};
}

std::optional<DrawnTerrain>
RefineMesh(const RoadSurface &surface, const TerrainFixture &input, const TangentFrame &frame) {
  const auto &sourceSheets = input.Sheets;
  const auto &sourceGrids = input.Grids;
  const auto &terrain = input.Heights;
  const auto positions = surface.SurfaceGeometry.positionsOf(0);
  constexpr TerrainPageLayout layout{.Side = kSourceSide, .Halo = 1};
  std::vector<TerrainRefinementSource> sources;
  sources.reserve(sourceSheets.size());
  for (size_t index = 0; index < sourceSheets.size(); ++index) {
    sources.push_back({.Page = &sourceSheets[index], .Heights = sourceGrids[index].TryField()});
  }
  std::vector<TerrainRefinementCorridor> corridors;
  corridors.reserve(surface.Spans.size());
  for (size_t index = 0; index < surface.Spans.size(); ++index) {
    const size_t at = index * 12;
    const auto centre = [&](size_t offset) {
      return EastNorth{.EastM = 0.5 * (positions[at + offset] + positions[at + 3 + offset]),
                       .NorthM = -0.5 * (positions[at + offset + 2] + positions[at + 5 + offset])};
    };
    corridors.push_back({.Start = centre(0),
                         .End = centre(6),
                         .HalfWidthM = kCorridorHalfWidthM,
                         .MaximumPostingM = RoadTerrainContact::MaximumPostingM});
  }
  const auto refined = RefineTerrain(sources,
                                     frame,
                                     layout,
                                     {.EyeM = {}, .OrthographicPxPerM = 1.0, .ErrorPx = 1e6},
                                     512,
                                     corridors);
  CHECK(refined && refined->size() > sourceSheets.size(),
        "the production selector spends fine terrain patches along the sourced route");
  if (!refined) { return std::nullopt; }
  Patchwork candidate{.Sheets = *refined};
  for (Sheet &sheet : candidate.Sheets) {
    const auto sourceAt = std::ranges::find_if(sourceSheets, [&](const Sheet &sourceSheet) {
      const auto shift = static_cast<uint32_t>(sheet.Tile.Zoom - sourceSheet.Tile.Zoom);
      return sheet.Tile.X >> shift == sourceSheet.Tile.X &&
             sheet.Tile.Y >> shift == sourceSheet.Tile.Y;
    });
    CHECK(sourceAt != sourceSheets.end(), "every selected page has a pinned source ancestor");
    if (sourceAt == sourceSheets.end()) { return std::nullopt; }
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
      DeformTerrain(surface.Earthworks, candidate, frame, layout, kMostEarthworkM);
  auto mesh = BuildTerrainMesh(candidate, frame, layout);
  CHECK(meshPress.Nodes > 0 && mesh.PositionsM.size() == candidate.Sheets.size() * 33u * 33u * 3u,
        "production terrain press and triangulation cover each selected page");
  return DrawnTerrain{.Pages = std::move(candidate), .Mesh = std::move(mesh)};
}

bool CheckMeshContact(const DrawnTerrain &drawn,
                      const ContactSamples &samples,
                      const TangentFrame &frame) {
  constexpr TerrainPageLayout layout{.Side = kSourceSide, .Halo = 1};
  const auto &candidate = drawn.Pages;
  const auto &mesh = drawn.Mesh;
  const auto &points = samples.Points;
  const auto &roadHeights = samples.RoadHeights;
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
      return false;
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
  return true;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  const auto input = LoadTerrain();
  if (!input) { return Report(); }
  auto fixture = Circuit::LoadNetwork();
  CHECK(fixture,
        fixture ? "pinned circuit source and complete topology load" : fixture.error().c_str());
  if (!fixture) { return Report(); }
  CHECK(fixture->Route.EdgeIds.size() == Circuit::EdgeCount,
        "the 267-edge closed route resolves without pitlane");
  const auto constraints = RoadConstraintChain::Build(
      fixture->Graph, fixture->Route.SourceIdentity, fixture->Route.EdgeIds, *input->Heights);
  CHECK(constraints, "every source edge samples pinned real DEM");
  if (!constraints) { return Report(); }
  const auto alignment = RoadAlignmentBuilder::Build(*constraints);
  CHECK(alignment && alignment->Closed() && alignment->Edges().size() == Circuit::EdgeCount,
        "real DEM yields one complete periodic native alignment");
  if (!alignment) { return Report(); }
  const TangentFrame frame = TangentFrame::At(alignment->Anchor());
  const auto surface = RoadSurfaceBuilder::Build(*alignment, frame);
  CHECK(surface && surface->SurfaceGeometry.wellFormed() &&
            surface->Spans.size() == surface->Earthworks.size(),
        "real DEM alignment yields complete native road and earthwork products");
  if (!surface) { return Report(); }
  if (!CheckShoulders(*alignment, *surface, *input->Heights, frame) ||
      !CheckVerticalProfile(*alignment, *surface)) {
    return Report();
  }
  const auto contacts = CheckContact(*surface, *input->Heights, frame);
  if (!contacts) { return Report(); }
  const auto drawn = RefineMesh(*surface, *input, frame);
  if (!drawn) { return Report(); }
  (void)CheckMeshContact(*drawn, *contacts, frame);
  return Report();
}
