#include "src/generators/terrain/GroundPatchwork.h"
#include "TileGeodesy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

using namespace outshine;

namespace {
enum class Pattern { Ready, Mixed, Pending };

struct Source : TileMeshes {
  Pattern Mode = Pattern::Ready;
  mutable std::vector<Data::TileId> Calls;
  size_t Queries = 0;
  size_t Waits = 0;
  mutable size_t Inspections = 0;

  int Code(Data::TileId tile) const {
    if (Mode == Pattern::Ready) { return 0; }
    if (Mode == Pattern::Pending) { return 1; }
    return static_cast<int>((tile.X + 3 * tile.Y + static_cast<uint32_t>(tile.Zoom)) % 7);
  }

  Reply Status(Data::TileId tile) const {
    constexpr std::array states{Reply::Ready,
                                Reply::Pending,
                                Reply::Absent,
                                Reply::Refused,
                                Reply::Undeclared,
                                Reply::Deferred,
                                Reply::Ready};
    return states[static_cast<size_t>(Code(tile))];
  }

  Reply Mesh(Data::TileId tile, [[maybe_unused]] int grid, TileBuild *out) override {
    Calls.push_back(tile);
    if (Status(tile) == Reply::Ready) {
      out->Side = Code(tile) == 6 ? 1 : 2;
      out->Nodes.assign(12, 1.0f);
    }
    return Status(tile);
  }

  Reply MeshAwaited([[maybe_unused]] Data::TileId tile,
                    [[maybe_unused]] int grid,
                    [[maybe_unused]] TileBuild *out) override {
    ++Waits;
    return Reply::Refused;
  }

  Reply Inspect(Data::TileId tile, [[maybe_unused]] int grid) const override {
    ++Inspections;
    Calls.push_back(tile);
    return Status(tile);
  }

  Reply Wants(Data::TileId tile, [[maybe_unused]] int grid) override {
    ++Queries;
    Calls.push_back(tile);
    return Status(tile);
  }
};

struct Expected {
  std::vector<Data::TileId> Calls;
  size_t Skipped = 0;
  size_t Overlapped = 0;
  size_t ContactPending = 0;
};

using Cells = std::set<std::pair<long, long>>;

Cells Footprint(std::pair<long, long> tile, long span) {
  const auto &[tileX, tileY] = tile;
  Cells footprint;
  for (long row = tileY * span; row < (tileY + 1) * span; ++row) {
    for (long column = tileX * span; column < (tileX + 1) * span; ++column) {
      footprint.emplace(column, row);
    }
  }
  return footprint;
}

void OracleLevel(
    const Around &over, const Source &source, int level, Cells &covered, Expected &result) {
  const int zoom = over.Zoom - level;
  const auto at = Ground::ToTileFracClamped(
      {.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, zoom);
  const long firstX = 2 * static_cast<long>(std::floor((std::floor(at.X) - 1) / 2));
  const long firstY = 2 * static_cast<long>(std::floor((std::floor(at.Y) - 1) / 2));
  const auto span = static_cast<long>(1UL << static_cast<unsigned>(level));
  const auto world = static_cast<long>(1UL << static_cast<unsigned>(zoom));
  Cells completed;
  for (long y = firstY; y < firstY + 4; ++y) {
    for (long x = firstX; x < firstX + 4; ++x) {
      const bool contact = level == 0 && x == static_cast<long>(std::floor(at.X)) &&
                           y == static_cast<long>(std::floor(at.Y));
      if (over.Access != TerrainTileAccess::Mesh && over.PlayableOnly && level != over.Levels - 1 &&
          !contact) {
        continue;
      }
      const auto footprint = Footprint({x, y}, span);
      const auto present = static_cast<size_t>(std::ranges::count_if(
          footprint, [&covered](const auto &cell) { return covered.contains(cell); }));
      if (present == footprint.size()) {
        ++result.Skipped;
        continue;
      }
      result.Overlapped += static_cast<size_t>(present != 0);
      if (y < 0 || y >= world) { continue; }
      const Data::TileId tile{.Zoom = zoom,
                              .X = static_cast<uint32_t>((x % world + world) % world),
                              .Y = static_cast<uint32_t>(y)};
      result.Calls.push_back(tile);
      const bool ready = over.Access != TerrainTileAccess::Mesh
                             ? source.Status(tile) == TileMeshes::Reply::Ready
                             : source.Code(tile) == 0;
      result.ContactPending += static_cast<size_t>(contact && !ready);
      if (ready) { completed.insert(footprint.begin(), footprint.end()); }
    }
  }
  covered.insert(completed.begin(), completed.end());
}

Expected CellOracle(const Around &over, const Source &source) {
  Expected result;
  Cells covered;
  for (int level = 0; level < over.Levels; ++level) {
    OracleLevel(over, source, level, covered, result);
  }
  return result;
}

std::vector<std::pair<double, double>> Positions() {
  std::vector<std::pair<double, double>> positions{{0.0, 0.0},
                                                   {179.9, 0.0},
                                                   {-179.9, 0.0},
                                                   {180.0, 0.0},
                                                   {-180.0, 0.0},
                                                   {0.0, 90.0},
                                                   {0.0, -90.0}};
  for (int x = 0; x < 16; ++x) {
    for (int y = 0; y < 16; ++y) {
      const double longitude = (16.5 + x) / 64.0 * 360.0 - 180.0;
      const double latitude =
          std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * (16.5 + y) / 64.0))) * 180.0 /
          std::numbers::pi;
      positions.emplace_back(longitude, latitude);
    }
  }
  return positions;
}

void CheckCoverage(const Around &over, Pattern pattern) {
  using namespace outshine::Test;
  const bool asking = over.Access != TerrainTileAccess::Mesh;
  Source source;
  source.Mode = pattern;
  const auto expected = CellOracle(over, source);
  const auto actual = LayPatchwork(source, over);
  const auto planned = PlanPatchworkTiles(over);
  CHECK(actual.has_value(), "valid cascade completes");
  CHECK(planned.has_value(), "valid coverage has a data plan");
  CHECK(source.Calls == expected.Calls, "tile order and fallback match independent cell union");
  if (planned) {
    const auto key = [](Data::TileId tile) { return std::tuple(tile.Zoom, tile.X, tile.Y); };
    CHECK(std::ranges::all_of(expected.Calls,
                              [&](Data::TileId tile) {
                                return std::ranges::binary_search(*planned, key(tile), {}, key);
                              }),
          "planned coverage contains every tile the independent cascade can request");
    if (pattern == Pattern::Pending && (!over.PlayableOnly || !asking)) {
      auto all = expected.Calls;
      std::ranges::sort(all, {}, key);
      all.erase(std::ranges::unique(all).begin(), all.end());
      CHECK(*planned == all,
            "no resident tiles makes the plan equal the independent full-grid oracle");
    }
  }
  CHECK(source.Waits == 0, "cascade never waits for a tile");
  CHECK(over.Access != TerrainTileAccess::Inspect ||
            (source.Queries == 0 && source.Inspections == expected.Calls.size()),
        "inspection observes the complete cascade without requesting any product");
  if (!actual) { return; }
  CHECK(actual->Skipped == expected.Skipped && actual->Overlapped == expected.Overlapped &&
            actual->ContactPending == expected.ContactPending,
        "full and partial coverage match independent raster");
  const auto ready =
      static_cast<size_t>(std::ranges::count_if(expected.Calls, [&source](Data::TileId tile) {
        return source.Status(tile) == TileMeshes::Reply::Ready && source.Code(tile) == 0;
      }));
  CHECK(actual->Tiles == source.Calls.size() && actual->Sheets.size() == (asking ? 0 : ready),
        "mesh products contain ready coverage and never pending placeholder geometry");
  size_t activeBytes = actual->Sheets.size() * sizeof(Sheet);
  for (const Sheet &sheet : actual->Sheets) { activeBytes += sheet.Nodes.size() * sizeof(float); }
  CHECK(actual->HeapBytes() >= activeBytes,
        "patchwork reports sheet slots and every owned node capacity");
  size_t pending = 0;
  size_t absent = 0;
  size_t refused = 0;
  size_t bare = 0;
  for (const auto tile : expected.Calls) {
    pending += static_cast<size_t>(source.Status(tile) == TileMeshes::Reply::Pending ||
                                   source.Status(tile) == TileMeshes::Reply::Deferred);
    absent += static_cast<size_t>(source.Status(tile) == TileMeshes::Reply::Absent ||
                                  source.Status(tile) == TileMeshes::Reply::Undeclared);
    refused += static_cast<size_t>(source.Status(tile) == TileMeshes::Reply::Refused);
    bare += static_cast<size_t>(source.Code(tile) != 0);
  }
  CHECK(actual->Pending == pending && actual->Absent == absent && actual->Refused == refused &&
            actual->Bare == (asking ? 0 : bare),
        "all reply states retain their diagnostic meaning");
}

}

int main() {
  using namespace outshine::Test;
  for (const auto position : Positions()) {
    for (const auto pattern : {Pattern::Ready, Pattern::Mixed, Pattern::Pending}) {
      for (const auto [access, playableOnly] : {std::pair{TerrainTileAccess::Mesh, false},
                                                {TerrainTileAccess::Request, false},
                                                {TerrainTileAccess::Inspect, false},
                                                {TerrainTileAccess::Mesh, true},
                                                {TerrainTileAccess::Request, true},
                                                {TerrainTileAccess::Inspect, true}}) {
        const Around over{.LatitudeDeg = position.second,
                          .LongitudeDeg = position.first,
                          .Zoom = 6,
                          .Levels = 4,
                          .Grid = 2,
                          .Access = access,
                          .PlayableOnly = playableOnly};
        CheckCoverage(over, pattern);
      }
    }
  }
  Source source;
  source.Mode = Pattern::Pending;
  const auto playable = LayPatchwork(source,
                                     {.Zoom = 6,
                                      .Levels = 4,
                                      .Grid = 2,
                                      .Access = TerrainTileAccess::Request,
                                      .PlayableOnly = true});
  CHECK(playable && playable->Tiles == 17 && playable->Pending == 17 &&
            playable->ContactPending == 1 && playable->PendingAtZoom[3] == 16,
        "first publication requests one exact contact tile and a complete coarse baseline");
  source.Calls.clear();
  const auto largest = LayPatchwork(source,
                                    {.Zoom = static_cast<int>(kZoomLevels) - 1,
                                     .Levels = std::numeric_limits<int>::max(),
                                     .Grid = 2});
  CHECK(largest && largest->CoarsestZoom == 1 && source.Calls.size() <= 16 * (kZoomLevels - 1),
        "largest cascade has a bounded request count and reaches zoom one");
  for (const auto over :
       {Around{.Zoom = 0},
        Around{.Zoom = 24},
        Around{.Zoom = 6, .Levels = 0},
        Around{.Zoom = 6, .Grid = 1},
        Around{.LatitudeDeg = std::numeric_limits<double>::quiet_NaN(), .Zoom = 6},
        Around{.LongitudeDeg = std::numeric_limits<double>::infinity(), .Zoom = 6}}) {
    Source untouched;
    CHECK(!LayPatchwork(untouched, over) && untouched.Calls.empty(),
          "invalid input is rejected before provider work");
    CHECK(!PlanPatchworkTiles(over), "invalid coverage is rejected before planning");
  }
  return Report();
}
