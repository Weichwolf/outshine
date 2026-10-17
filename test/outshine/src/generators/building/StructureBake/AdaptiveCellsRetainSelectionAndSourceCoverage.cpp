#include "BuildingMesh.h"
#include "StructureBake.h"
#include "PreparedStructureTile.h"
#include "StructureArtifact.h"
#include "Geodesy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <set>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
constexpr std::string_view kProductKey =
    "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";

RawTile Inputs() {
  constexpr double kTileSpanM = 16000;
  constexpr double kLongitude = 9;
  constexpr double kLatitude = 47;
  constexpr double kTileDegrees = 0.2;
  constexpr double kFocalPx = 720;
  constexpr double kCameraHeightM = 40;
  RawTile raw;
  raw.TileSpanM = kTileSpanM;
  raw.Eye = {.LongitudeDeg = kLongitude, .LatitudeDeg = kLatitude};
  raw.Projection = {.FocalPx = kFocalPx, .AllowedErrorPx = 1};
  GeoToEcef({.LongitudeDeg = raw.Eye.LongitudeDeg, .LatitudeDeg = raw.Eye.LatitudeDeg},
            raw.AnchorEcef);
  Vec3 eye;
  GeoToEcef({.LongitudeDeg = kLongitude, .LatitudeDeg = kLatitude, .HeightM = kCameraHeightM}, eye);
  raw.EyeEcef = eye;
  const Ground::GeoBounds bounds{.MinLonDeg = kLongitude,
                                 .MinLatDeg = kLatitude,
                                 .MaxLonDeg = kLongitude + kTileDegrees,
                                 .MaxLatDeg = kLatitude + kTileDegrees};
  constexpr double kNearStep = 0.0002;
  constexpr double kFarLongitude = 9.08;
  constexpr double kFarLatitude = 47.08;
  constexpr double kFarStep = 0.004;
  constexpr double kBuildingSpan = 0.0001;
  constexpr uint32_t kNearCount = 4;
  constexpr uint32_t kSourceCount = 20;
  for (uint32_t at = 0; at < kSourceCount; ++at) {
    const uint32_t row = (at < kNearCount ? 0 : at - kNearCount) / 4;
    const double lon =
        at < kNearCount ? kLongitude + kNearStep : kFarLongitude + (at - kNearCount) % 4 * kFarStep;
    const double lat =
        at < kNearCount ? kLatitude + (at + 1) * kNearStep : kFarLatitude + row * kFarStep;
    const std::array ring{lat,
                          lon,
                          lat,
                          lon + kBuildingSpan,
                          lat + kBuildingSpan,
                          lon + kBuildingSpan,
                          lat + kBuildingSpan,
                          lon};
    const auto cell = StructureCellOf(bounds, ring);
    CHECK(cell, "each full source footprint has a spatial address");
    if (!cell) { return {}; }
    const auto first = static_cast<uint32_t>(raw.LatLon.size() / 2);
    raw.LatLon.insert(raw.LatLon.end(), ring.begin(), ring.end());
    raw.Structures.push_back(
        {.LocalFirst = first, .PointCount = 4, .Cell = *cell, .HeightM = 9, .Pitched = 0});
  }
  return raw;
}

void CellProducts(const PreparedStructureTile &base,
                  RawTile view,
                  const BakedTile &whole,
                  const StructureMesher &mesher) {
  std::set<uint32_t> cells;
  for (const auto &source : base.Structures) { cells.insert(source.Layout.Cell.Index); }
  size_t sources = 0;
  size_t coarse = 0;
  size_t triangles = 0;
  size_t largestBytes = 0;
  uint64_t covered = 0;
  for (const uint32_t cell : cells) {
    view.RequestedCell = cell;
    auto scratch = mesher.Scratch();
    const auto part = BakePreparedStructures(base, view, mesher, *scratch);
    CHECK(part, "adaptive generation accepts a spatial cell without forcing a uniform LOD");
    if (!part) { continue; }
    const auto count = std::ranges::count(
        base.Structures, cell, [](const auto &source) { return source.Layout.Cell.Index; });
    const uint64_t bit = uint64_t{1} << (cell - 1u);
    CHECK(!part->RequestedDetail && part->RequestedCell == cell && part->SelectedView &&
              part->OccupiedCells == bit && part->Prints.size() == static_cast<size_t>(count),
          "one adaptive product retains its view, exact cell and every assigned source");
    CHECK((covered & bit) == 0, "initial products do not duplicate source cells");
    covered |= bit;
    sources += part->Prints.size();
    coarse += std::ranges::count_if(part->FootprintDetails,
                                    [](auto detail) { return detail != LevelOfDetail::Fine; });
    triangles += (part->Built.WallRun.size() + part->Built.RoofRun.size()) / 3;
    for (const auto &print : part->Prints) {
      const auto expected =
          std::ranges::find(whole.Prints, print.FirstPoint, &Ground::BuildingFootprint::FirstPoint);
      CHECK(expected != whole.Prints.end() && *expected == print,
            "cell products retain every enriched source footprint field");
    }
    if (std::ranges::all_of(part->FootprintDetails,
                            [](auto detail) { return detail == LevelOfDetail::Fine; })) {
      CHECK(std::ranges::all_of(part->Built.WallCorners,
                                [&](const StoredVertex &vertex) {
                                  return std::ranges::any_of(
                                      whole.Built.WallCorners, [&](const StoredVertex &expected) {
                                        return expected.pos == vertex.pos &&
                                               expected.texture == vertex.texture &&
                                               expected.normWord == vertex.normWord;
                                      });
                                }),
            "near walls preserve exact positions, facade coordinates and normals");
    }
    const auto encoded = EncodeStructureArtifact(*part, kProductKey);
    CHECK(encoded, "the existing asset schema encodes an adaptive cell product");
    if (!encoded) { continue; }
    largestBytes = std::max(largestBytes, encoded->size());
    const auto decoded = DecodeStructureArtifact(*encoded, kProductKey, 0);
    CHECK(decoded && !decoded->RequestedDetail && decoded->RequestedCell == cell &&
              decoded->Digest == part->Digest && decoded->OccupiedCells == bit,
          "cache replay retains the adaptive spatial selection and geometry digest");
  }
  const auto encodedWhole = EncodeStructureArtifact(whole, kProductKey);
  CHECK(sources == base.Structures.size() && covered == whole.OccupiedCells,
        "all bounded child products cover the complete parent source set");
  CHECK(coarse != 0, "far sources keep adaptive coarse representations instead of forced Fine");
  CHECK(encodedWhole, "the unchanged whole-tile product remains a valid encoded reference");
  std::println("adaptive cells={} sources={} triangles={} whole_triangles={} largest_bytes={} "
               "whole_bytes={}",
               cells.size(),
               sources,
               triangles,
               (whole.Built.WallRun.size() + whole.Built.RoofRun.size()) / 3,
               largestBytes,
               encodedWhole ? encodedWhole->size() : 0);
}
}

int main() {
  const auto raw = Inputs();
  Ground::HeightField::Block block;
  block.At = {.Zoom = 0, .X = 0, .Y = 0};
  block.Raster = {.Side = 2, .Postings = 2};
  block.Nodes.assign(4, 0.0f);
  const auto ground = Ground::HeightField::Of(0, {block});
  CHECK(ground, "the source fixture has a complete common height field");
  if (!ground) { return Report(); }
  const auto base = PrepareStructureTile(raw, *ground);
  CHECK(base, "source enrichment runs once before spatial generation");
  if (!base) { return Report(); }
  const BuildingMesh mesher;
  auto scratch = mesher.Scratch();
  const auto whole = BakePreparedStructures(*base, raw, mesher, *scratch);
  CHECK(whole, "the unchanged whole-tile path supplies coverage and cost comparison");
  if (whole) { CellProducts(*base, raw, *whole, mesher); }
  return Report();
}
