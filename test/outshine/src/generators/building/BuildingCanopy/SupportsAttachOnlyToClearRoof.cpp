#include "BuildingMesh.h"
#include "BuildingScratch.h"
#include "Check.h"
#include "HeightField.h"
#include "PreparedStructureCodec.h"
#include "PreparedStructureTile.h"
#include "TangentFrame.h"
#include <array>
#include <cstdint>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;

RawTile CanopySite(bool hasRoof, bool crossing) {
  RawTile raw;
  const auto frame = TangentFrame::At({.LongitudeDeg = 16, .LatitudeDeg = 48});
  raw.AnchorEcef = frame.OriginEcef();
  raw.TileSpanM = 1000;
  const auto add = [&](const auto &points) {
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    for (const auto point : points) {
      const auto at = frame.ApproximateGeographicAt(point);
      raw.LatLon.push_back(at.LatitudeDeg);
      raw.LatLon.push_back(at.LongitudeDeg);
    }
    return first;
  };
  if (hasRoof) {
    const auto first =
        add(std::array{EastNorth{0, 0}, EastNorth{35, 0}, EastNorth{35, 34}, EastNorth{0, 34}});
    raw.Structures.push_back({.LocalFirst = first,
                              .PointCount = 4,
                              .SourceFirst = first,
                              .Cell = {.Index = 1, .Footprint = {}},
                              .HeightM = 5,
                              .Pitched = 0,
                              .WallColour = {},
                              .Facade = {}});
  }
  const auto cap =
      add(std::array{EastNorth{10, 8}, EastNorth{24, 8}, EastNorth{24, 22}, EastNorth{10, 22}});
  raw.Structures.push_back({.LocalFirst = cap,
                            .PointCount = 4,
                            .SourceFirst = cap,
                            .Cell = {.Index = 1, .Footprint = {}},
                            .HeightM = 11,
                            .MinimumHeightM = 10,
                            .Pitched = 1,
                            .WallColour = {},
                            .Facade = {}});
  if (crossing) {
    const auto first = add(std::array{EastNorth{10.5, 0}, EastNorth{10.5, 30}});
    raw.Ways.push_back({.LocalFirst = first, .PointCount = 2, .HalfWidthM = 1});
  }
  return raw;
}
}

int main() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes = {100, 100, 100, 100};
  const auto ground = Ground::HeightField::Of(0, {block});
  CHECK(ground, "the roof has qualified terrain samples");
  if (!ground) { return Report(); }
  auto raw = CanopySite(true, false);
  auto supported = PrepareStructureTile(raw, *ground);
  CHECK(supported && supported->Surfaces.back().Shapes().size() == 5,
        "the elevated slab gains exactly four supports when each has roof contact");
  if (!supported || supported->Surfaces.back().Shapes().size() != 5) { return Report(); }
  const auto clearWay = PrepareStructureTile(CanopySite(true, true), *ground);
  const auto missingRoof = PrepareStructureTile(CanopySite(false, false), *ground);
  CHECK(clearWay && clearWay->Surfaces.back().Shapes().size() == 1,
        "a walkway through a post location remains clear");
  CHECK(missingRoof && missingRoof->Surfaces.back().Shapes().size() == 1,
        "an elevated surface never adds posts without a supporting roof");
  CHECK(!supported->Structures.back().Standing.TerrainCeiling,
        "rooftop supports do not become terrain foundations");
  const auto encoded = EncodePreparedStructureTile(*supported);
  const auto restored = encoded ? DecodePreparedStructureTile(*encoded, 1024 * 1024) : std::nullopt;
  CHECK(restored && restored->Surfaces.back().Shapes().size() == 5,
        "the building asset retains its procedural supports");
  BuildingScratch scratch;
  const BuildingMesh mesher;
  raw.RequestedDetail = LevelOfDetail::Shell;
  const auto baked = BakePreparedStructures(*supported, raw, mesher, scratch);
  CHECK(baked && !baked->Built.WallRun.empty() && baked->UnsupportedMeshes == 0,
        "supported parts use the regular building mesh and LOD path");
  return Report();
}
