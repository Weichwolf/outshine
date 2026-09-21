#include "src/generators/terrain/GroundPatchwork.h"
#include "TileGeodesy.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <utility>

using namespace outshine;

namespace {
enum class Pattern { Ready, Mixed, Pending };

struct Source : TileMeshes {
  Pattern Mode = Pattern::Ready;
  std::vector<Data::TileId> Calls;
  size_t Queries = 0;
  size_t Waits = 0;

  int Code(Data::TileId tile) const {
    if (Mode == Pattern::Ready) { return 0; }
    if (Mode == Pattern::Pending) { return 1; }
    return static_cast<int>((tile.X + 3 * tile.Y + static_cast<uint32_t>(tile.Zoom)) % 6);
  }

  Reply Status(Data::TileId tile) const {
    constexpr std::array states{Reply::Ready,
                                Reply::Pending,
                                Reply::Absent,
                                Reply::Refused,
                                Reply::Undeclared,
                                Reply::Ready};
    return states[static_cast<size_t>(Code(tile))];
  }

  Reply Mesh(Data::TileId tile, int, TileBuild *out) override {
    Calls.push_back(tile);
    if (Status(tile) == Reply::Ready) {
      out->Side = Code(tile) == 5 ? 1 : 2;
      out->Nodes.assign(12, 1.0f);
    }
    return Status(tile);
  }

  Reply MeshAwaited(Data::TileId, int, TileBuild *) override {
    ++Waits;
    return Reply::Refused;
  }

  Reply Wants(Data::TileId tile, int) override {
    ++Queries;
    Calls.push_back(tile);
    return Status(tile);
  }
};

struct Expected {
  std::vector<Data::TileId> Calls;
  size_t Skipped = 0;
  size_t Overlapped = 0;
};

Expected CellOracle(const Around &over, const Source &source) {
  Expected result;
  std::set<std::pair<long, long>> covered;
  for (int level = 0; level < over.Levels; ++level) {
    const int zoom = over.Zoom - level;
    const auto at = Ground::ToTileFracClamped(
        {.LongitudeDeg = over.LongitudeDeg, .LatitudeDeg = over.LatitudeDeg}, zoom);
    const long firstX = 2 * static_cast<long>(std::floor((std::floor(at.X) - 1) / 2));
    const long firstY = 2 * static_cast<long>(std::floor((std::floor(at.Y) - 1) / 2));
    const long span = 1L << level;
    const long world = 1L << zoom;
    std::set<std::pair<long, long>> completed;
    for (long y = firstY; y < firstY + 4; ++y) {
      for (long x = firstX; x < firstX + 4; ++x) {
        std::set<std::pair<long, long>> footprint;
        size_t present = 0;
        for (long row = y * span; row < (y + 1) * span; ++row) {
          for (long col = x * span; col < (x + 1) * span; ++col) {
            footprint.emplace(col, row);
            present += covered.contains({col, row});
          }
        }
        if (present == footprint.size()) {
          ++result.Skipped;
          continue;
        }
        result.Overlapped += present != 0;
        if (y < 0 || y >= world) { continue; }
        const Data::TileId tile{.Zoom = zoom,
                                .X = static_cast<uint32_t>((x % world + world) % world),
                                .Y = static_cast<uint32_t>(y)};
        result.Calls.push_back(tile);
        const bool ready =
            over.Asking ? source.Status(tile) == TileMeshes::Reply::Ready : source.Code(tile) == 0;
        if (ready) { completed.insert(footprint.begin(), footprint.end()); }
      }
    }
    covered.insert(completed.begin(), completed.end());
  }
  return result;
}
}

int main() {
  using namespace outshine::Test;
  for (const auto position :
       {std::pair{0.0, 0.0}, {179.9, 0.0}, {-179.9, 0.0}, {0.0, 90.0}, {0.0, -90.0}}) {
    for (const auto pattern : {Pattern::Ready, Pattern::Mixed, Pattern::Pending}) {
      for (const bool asking : {false, true}) {
        const Around over{.LatitudeDeg = position.second,
                          .LongitudeDeg = position.first,
                          .Zoom = 6,
                          .Levels = 4,
                          .Grid = 2,
                          .Asking = asking};
        Source source;
        source.Mode = pattern;
        const auto expected = CellOracle(over, source);
        const auto actual = LayPatchwork(source, over);
        CHECK(actual.has_value(), "valid cascade completes");
        CHECK(source.Calls == expected.Calls,
              "tile order and fallback match independent cell union");
        CHECK(source.Waits == 0, "cascade never waits for a tile");
        if (!actual) { continue; }
        CHECK(actual->Skipped == expected.Skipped && actual->Overlapped == expected.Overlapped,
              "full and partial coverage match independent raster");
        const size_t ready =
            static_cast<size_t>(std::ranges::count_if(expected.Calls, [&source](Data::TileId tile) {
              return source.Status(tile) == TileMeshes::Reply::Ready && source.Code(tile) == 0;
            }));
        CHECK(actual->Tiles == source.Calls.size() && actual->Sheets.size() == (asking ? 0 : ready),
              "mesh products contain ready coverage and never pending placeholder geometry");
        size_t activeBytes = actual->Sheets.size() * sizeof(Sheet);
        for (const Sheet &sheet : actual->Sheets) {
          activeBytes += sheet.Nodes.size() * sizeof(float);
        }
        CHECK(actual->HeapBytes() >= activeBytes,
              "patchwork reports sheet slots and every owned node capacity");
        size_t pending = 0, absent = 0, refused = 0, bare = 0;
        for (const auto tile : expected.Calls) {
          pending += source.Status(tile) == TileMeshes::Reply::Pending;
          absent += source.Status(tile) == TileMeshes::Reply::Absent ||
                    source.Status(tile) == TileMeshes::Reply::Undeclared;
          refused += source.Status(tile) == TileMeshes::Reply::Refused;
          bare += source.Code(tile) != 0;
        }
        CHECK(actual->Pending == pending && actual->Absent == absent &&
                  actual->Refused == refused && actual->Bare == (asking ? 0 : bare),
              "all reply states retain their diagnostic meaning");
      }
    }
  }
  Source source;
  source.Mode = Pattern::Pending;
  const auto playable = LayPatchwork(
      source, {.Zoom = 6, .Levels = 4, .Grid = 2, .Asking = true, .PlayableOnly = true});
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
  }
  return Report();
}
