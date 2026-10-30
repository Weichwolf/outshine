#include "BuildingInputs.h"
#include "OsmField.h"
#include "Sha256.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <ios>
#include <iterator>
#include <numbers>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Generators::Osm;

using GridPoint = std::array<int64_t, 2>;
constexpr int kZoom = 14;
constexpr int kWrongInput = 5;
constexpr int kChangedFixture = 6;
constexpr uint64_t kContainer = 79947880;
constexpr size_t kWholeExteriors = 1194;
constexpr size_t kCutExteriors = 876;
constexpr double kHeightM = 5.0;
constexpr double kHalfTurnDeg = 180.0;
constexpr double kFullTurnDeg = 360.0;
constexpr int64_t kExtent = 4096;
constexpr int64_t kTileX = 8744;
constexpr int64_t kWholeTileY = 5705;
constexpr double kWorldGrid =
    static_cast<double>(kExtent) * (uint32_t{1} << static_cast<unsigned>(kZoom));
constexpr std::array<GridPoint, 4> kCut{{{587, 42}, {649, 58}, {648, 64}, {581, 64}}};
constexpr std::array<GridPoint, 4> kWhole{{{587, 42}, {649, 58}, {641, 89}, {579, 73}}};
constexpr std::array<std::string_view, 2> kInputDigests{
    "f8a80310f189e94d5f3f5f0b8f9a8167b083dfd503c2a45a74bd26d8c8911d80",
    "fc433eb129d174ac96964832d6c0a81e2ad377671d4b8eb6a971a8567200d373"};

enum class Footprint : uint8_t { Other, Cut, Whole };

GridPoint Grid(double lat, double lon) {
  return {std::llround((lon + kHalfTurnDeg) / kFullTurnDeg * kWorldGrid) - kTileX * kExtent,
          std::llround((1.0 - std::asinh(std::tan(lat * std::numbers::pi / kHalfTurnDeg)) /
                                  std::numbers::pi) *
                       0.5 * kWorldGrid) -
              kWholeTileY * kExtent};
}

bool SameRing(const std::array<GridPoint, 4> &actual, const std::array<GridPoint, 4> &expected) {
  for (size_t shift = 0; shift < actual.size(); ++shift) {
    bool forward = true;
    bool reverse = true;
    for (size_t at = 0; at < actual.size(); ++at) {
      forward = forward && actual[at] == expected[(shift + at) % expected.size()];
      reverse = reverse && actual[at] == expected[(shift + expected.size() - at) % expected.size()];
    }
    if (forward || reverse) { return true; }
  }
  return false;
}

Footprint ShapeOf(std::span<const double> points, uint32_t first, uint32_t count) {
  if (count != kWhole.size() || first > points.size() / 2 || count > points.size() / 2 - first) {
    return Footprint::Other;
  }
  std::array<GridPoint, 4> shape;
  for (size_t i = 0; i < shape.size(); ++i) {
    const size_t at = (static_cast<size_t>(first) + i) * 2;
    shape[i] = Grid(points[at], points[at + 1]);
  }
  if (SameRing(shape, kWhole)) { return Footprint::Whole; }
  return SameRing(shape, kCut) ? Footprint::Cut : Footprint::Other;
}

int ReadOriginals(OsmField &vectors, char *const *paths) {
  for (size_t i = 0; i < kInputDigests.size(); ++i) {
    std::ifstream input(paths[i], std::ios::binary);
    if (!input) { return 3; }
    const std::vector<uint8_t> bytes(std::istreambuf_iterator<char>(input), {});
    if (Sha256Hex(bytes.data(), bytes.size()) != kInputDigests[i]) { return kWrongInput; }
    if (!vectors.Accept(static_cast<int>(kTileX),
                        static_cast<int>(kWholeTileY - 1) + static_cast<int>(i),
                        bytes)) {
      return 4;
    }
  }
  return 0;
}

struct Counts {
  size_t Observations = 0;
  size_t OriginalControls = 0;
  size_t Prepared = 0;
  size_t Whole = 0;
  size_t Cut = 0;
  size_t Qualified = 0;
};

bool OriginalOf(const OsmField &vectors, const OsmField::Feature &feature, Counts &counts) {
  if (feature.ProviderFeatureId != kContainer) { return false; }
  const auto &tile = vectors.Tiles()[feature.Tile];
  size_t exteriors = 0;
  size_t matches = 0;
  for (const auto &ring : vectors.Rings().subspan(feature.FirstRing, feature.RingCount)) {
    if (!ring.Exterior) { continue; }
    ++exteriors;
    const auto shape = ShapeOf(vectors.Points(), ring.First, ring.Count);
    if ((tile.Y == kWholeTileY && shape == Footprint::Whole) ||
        (tile.Y == kWholeTileY - 1 && shape == Footprint::Cut)) {
      ++matches;
    }
  }
  counts.Observations += matches;
  if (exteriors == (tile.Y == kWholeTileY ? kWholeExteriors : kCutExteriors) &&
      vectors.Num(feature, "height", 0) == kHeightM && vectors.Num(feature, "min_height", 0) == 0) {
    counts.OriginalControls += matches;
  }
  return matches != 0;
}

void Inspect(const OsmField &vectors, size_t at, Counts &counts) {
  const auto &feature = vectors.Features()[at];
  if (!OriginalOf(vectors, feature, counts)) { return; }
  const BuildingField prints;
  const StreetField streets;
  RawTile raw;
  PrepareBuildingInputs(vectors,
                        prints,
                        streets,
                        {.From = at, .To = at + 1, .Tile = feature.Tile, .Found = true},
                        {},
                        std::nullopt,
                        std::nullopt,
                        raw);
  for (const auto &structure : raw.Structures) {
    const auto shape = ShapeOf(raw.LatLon, structure.LocalFirst, structure.PointCount);
    if (shape == Footprint::Other) { continue; }
    std::print(R"({}{{"source_tile_y":{},"shape":"{}","source_id":{}}})",
               counts.Prepared != 0 ? "," : "",
               vectors.Tiles()[feature.Tile].Y,
               shape == Footprint::Whole ? "whole" : "cut",
               structure.SourceId.Id);
    ++counts.Prepared;
    counts.Whole += shape == Footprint::Whole ? 1u : 0u;
    counts.Cut += shape == Footprint::Cut ? 1u : 0u;
    counts.Qualified +=
        structure.HeightM == kHeightM && structure.MinimumHeightM == 0 && structure.SourceId.Id == 0
            ? 1u
            : 0u;
  }
}
}

int main(int argc, char *const *argv) {
  if (argc != 3) { return 2; }
  const std::array<std::string, 1> layers{"buildings"};
  outshine::Generators::Osm::OsmField vectors(
      kZoom, layers, outshine::Generators::Osm::MvtSchema::OpenMapTiles);
  if (const auto error = ReadOriginals(vectors, argv + 1)) { return error; }
  Counts counts;
  std::print("{{\"prepared\":[");
  for (size_t at = 0; at < vectors.Features().size(); ++at) { Inspect(vectors, at, counts); }
  const bool canonical = counts.Prepared == 1 && counts.Whole == 1 && counts.Cut == 0;
  std::println("],\"original_observations\":{},\"original_controls\":{},"
               "\"prepared_structures\":{},\"prepared_whole\":{},\"prepared_cut\":{},"
               "\"expected_whole\":1,\"canonical_requirement_met\":{}}}",
               counts.Observations,
               counts.OriginalControls,
               counts.Prepared,
               counts.Whole,
               counts.Cut,
               canonical);
  if (vectors.Tiles().size() != 2 || counts.Observations != 2 || counts.OriginalControls != 2 ||
      counts.Qualified != counts.Prepared) {
    return kChangedFixture;
  }
  return canonical ? 0 : 1;
}
