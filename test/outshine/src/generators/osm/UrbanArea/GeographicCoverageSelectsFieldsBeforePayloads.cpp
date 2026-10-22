#include "UrbanArea.h"
#include "UrbanAreaAsset.h"
#include "OsmField.h"
#include "Check.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "TileGeodesy.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = outshine::Generators::Osm;
constexpr int kZoom = 16;

std::expected<Osm::UrbanArea, std::string> AreaAt(LongitudeLatitude focus) {
  const auto located = Osm::OsmField::Locate(focus, kZoom);
  if (!located) { return std::unexpected(std::string(located.error())); }
  const auto tile = *located;
  const auto box = Ground::TileBounds(
      {.Zoom = kZoom, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)});
  constexpr double marginDeg = .002;
  const double west = box.MinLonDeg - marginDeg, east = box.MaxLonDeg + marginDeg;
  const double south = box.MinLatDeg - marginDeg, north = box.MaxLatDeg + marginDeg;
  const std::array<std::string, 1> layers{"sites"};
  Osm::OsmField field(kZoom, layers);
  const Osm::OsmField::Declared urban{
      .Layer = "sites",
      .Key = "kind",
      .Value = "residential",
      .Area = true,
      .LatLon = {south, west, south, east, north, east, north, west}};
  field.Declare(std::span(&urban, 1), tile);
  const std::array<const Osm::OsmField *, 1> inputs{&field};
  return Osm::GenerateUrbanArea(inputs);
}

AssetQuery QueryAt(const Vec3 &point) {
  Box box;
  constexpr double radiusM = 1;
  for (size_t axis = 0; axis < 3; ++axis) {
    box.Min[axis] = point[axis] - radiusM;
    box.Max[axis] = point[axis] + radiusM;
  }
  return {.Bounds = box,
          .Kind = "urban-area",
          .Level = kZoom,
          .Radius = AssetRadius{.Centre = point, .Metres = radiusM},
          .Planes = {}};
}

void TileCoverage(const Osm::UrbanArea &area, const Box &coverage) {
  const auto &tile = area.Inputs.front().Identity.Tile;
  const auto box = Ground::TileBounds(tile);
  for (const double latitude : {box.MinLatDeg, box.MaxLatDeg}) {
    for (const double longitude : {box.MinLonDeg, box.MaxLonDeg}) {
      Vec3 point;
      GeoToEcef({.LongitudeDeg = longitude, .LatitudeDeg = latitude, .HeightM = 0}, point);
      for (size_t axis = 0; axis < 3; ++axis) {
        CHECK(coverage.Min[axis] <= point[axis] && coverage.Max[axis] >= point[axis],
              "geographic coverage encloses every owner tile corner");
      }
    }
  }
}

void CoverageQuery() {
  const auto path = PlantedPath("urban-ecef-coverage.sqlite");
  std::error_code error;
  std::filesystem::remove(path, error);
  auto cache = AssetCache::Open(path);
  CHECK(cache, "asset index opens");
  if (!cache) { return; }
  const std::array<LongitudeLatitude, 3> sites{{{.LongitudeDeg = 0, .LatitudeDeg = 0},
                                                {.LongitudeDeg = 179.999, .LatitudeDeg = 48},
                                                {.LongitudeDeg = -179.999, .LatitudeDeg = 84}}};
  for (const auto site : sites) {
    const auto area = AreaAt(site);
    CHECK(area, "source field cooks near equator, antimeridian and polar Mercator band");
    if (!area) { continue; }
    const auto request =
        Sha256Hex(std::to_string(site.LongitudeDeg) + "/" + std::to_string(site.LatitudeDeg));
    const auto package = Osm::PackageUrbanArea(*area, request);
    CHECK(package && package->Records.size() == 1, "native field gets one spatial coverage record");
    if (!package) { continue; }
    const auto &record = package->Records.front();
    TileCoverage(*area, record.Bounds);
    const auto stored = (*cache)->Publish(package->Records, package->Bytes);
    CHECK(stored, "geographic product publishes through the shared package service");
    Vec3 point;
    GeoToEcef({.LongitudeDeg = site.LongitudeDeg, .LatitudeDeg = site.LatitudeDeg, .HeightM = 0},
              point);
    for (size_t axis = 0; axis < 3; ++axis) {
      CHECK(record.Bounds.Min[axis] <= point[axis] && record.Bounds.Max[axis] >= point[axis],
            "coverage encloses its geographic surface point in ECEF");
    }
    const auto selected = (*cache)->Select(QueryAt(point));
    CHECK(selected && std::ranges::any_of(*selected,
                                          [&](const auto &hit) { return hit.Key == record.Key; }),
          "metadata-only radius query finds the requested geographical field");
    CHECK(record.Level == kZoom && record.RequestKey == request,
          "request and source hierarchy level remain indexed");
    CHECK(!Osm::PackageUrbanArea(*area, "invalid"),
          "noncanonical request identities never reach publication");
  }
  const auto central = (*cache)->Select(QueryAt({{0, 0, 0}}));
  CHECK(central && central->empty(),
        "surface fields are not accidentally indexed around model-space zero");
  cache->reset();
  std::filesystem::remove(path, error);
}
}

int main() {
  CoverageQuery();
  return Report();
}
