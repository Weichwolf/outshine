#include "PreparedBuildingAssets.h"
#include "PreparedBuildingBasis.h"
#include "PreparedStructureTile.h"
#include "PreparedStructurePlan.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include "AssetCache.h"
#include "Geodesy.h"
#include "Check.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr double kLongitude = 9, kLatitude = 47, kSpanM = 1000;
constexpr double kRegionExtentDeg = 0.02, kInsetDeg = 0.001, kFootprintExtentDeg = 0.0002;
constexpr double kHeightM = 12;

RawTile Inputs() {
  RawTile raw;
  raw.TileSpanM = kSpanM;
  GeoToEcef({.LongitudeDeg = kLongitude, .LatitudeDeg = kLatitude}, raw.AnchorEcef);
  const Ground::GeoBounds region{.MinLonDeg = kLongitude,
                                 .MinLatDeg = kLatitude,
                                 .MaxLonDeg = kLongitude + kRegionExtentDeg,
                                 .MaxLatDeg = kLatitude + kRegionExtentDeg};
  const double west = kLongitude + kInsetDeg;
  const double south = kLatitude + kInsetDeg;
  const std::array ring{south,
                        west,
                        south,
                        west + kFootprintExtentDeg,
                        south + kFootprintExtentDeg,
                        west + kFootprintExtentDeg,
                        south + kFootprintExtentDeg,
                        west};
  const auto cell = StructureCellOf(region, ring);
  CHECK(cell, "the single real building footprint has a valid native cell");
  if (!cell) { return {}; }
  raw.LatLon.assign(ring.begin(), ring.end());
  raw.Structures.push_back({.LocalFirst = 0,
                            .PointCount = 4,
                            .SourceFirst = 0,
                            .Cell = *cell,
                            .HeightM = kHeightM,
                            .MinimumHeightM = 0,
                            .Pitched = 0,
                            .WallColour = {},
                            .Facade = FacadeStyle::Commercial});
  return raw;
}

std::shared_ptr<const Ground::HeightField> Heights() {
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  constexpr std::array elevations{125.125F, 131.25F, 142.5F, 149.75F};
  block.Nodes.assign(elevations.begin(), elevations.end());
  return Ground::HeightField::Of(0, {block});
}

void Exercise(const std::filesystem::path &directory) {
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  const Data::SourceSet sources(store);
  auto cache = PreparedBuildingAssets::Open(directory.string(), sources);
  CHECK(cache, "the native building cache opens without network sources");
  if (!cache) { return; }
  const Data::TileId tile{.Zoom = 0, .X = 0, .Y = 0};
  const Ground::ShapedGround shape;
  constexpr uint64_t kInputDigest = 17;
  const auto request = (*cache)->RequestKey(tile, shape, kSpanM);
  const auto key = (*cache)->Key(tile, kInputDigest, shape, kSpanM);
  const std::atomic_bool stopping{false};
  const auto heights = Heights();
  CHECK(heights, "the bounded native elevation fixture is valid");
  if (!heights) { return; }
  const auto generated = (*cache)->Generate(key, Inputs(), *heights, stopping, request);
  CHECK(generated && *generated, "the native generator publishes actual building inputs");
  const auto first = (*cache)->LoadBasisRequest(request);
  CHECK(first && *first && (**first).Product->Sources.size() == 1,
        "the native cache decodes a complete nonempty building basis");
  if (!first || !*first) { return; }
  auto records = AssetCache::Open((directory / "assets.sqlite").string());
  CHECK(records, "an independent connection opens request metadata");
  if (!records) { return; }
  const auto record = (*records)->FindRequest(request);
  CHECK(record && *record, "the original request owns a persistent product binding");
  if (!record || !*record) { return; }
  const std::string alias(64, 'f');
  CHECK(alias != request, "the two logical addresses are distinct");
  const auto bound = (*records)->BindRequest((**record).Key, alias);
  CHECK(bound && *bound, "a second resolved address binds the same building basis");
  const auto before = (*cache)->Costs();
  for (const auto *address : {&request, &alias, &request, &alias}) {
    const auto hit = (*cache)->LoadBasisRequest(*address);
    CHECK(hit && *hit && (**hit).BaseKey == key && (**hit).Product.get() == (**first).Product.get(),
          "both addresses retain the same immutable live basis without displacement");
  }
  const auto after = (*cache)->Costs();
  CHECK(after.BasisResidentHits == before.BasisResidentHits + 4 &&
            after.BasisHits == before.BasisHits && after.BasisReadBytes == before.BasisReadBytes &&
            after.ReadBytes == before.ReadBytes,
        "alternating addresses cause no native payload reads, plan reads or decoding");
  CHECK((*records)->FindRequest(request)->has_value() &&
            (*records)->FindRequest(alias)->has_value(),
        "both logical bindings remain durable after resident reuse");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-basis-alias-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  Exercise(directory);
  std::filesystem::remove_all(directory);
  return Report();
}
