#include "TilePool.h"
#include "TerrainPathPreparation.h"
#include "RawSourceCache.h"
#include "SourceSet.h"
#include <SourceTransport.h>
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <set>
#include <span>
#include <string>
#include <tuple>
#include <vector>

using namespace outshine;

namespace {
class NoNetwork final : public Data::SourceTransport {
public:
  size_t Starts = 0;

  Data::FetchStart Begin(const std::string & /*url*/) override {
    ++Starts;
    return Data::Ticket::None;
  }

  Data::Wire Collect(Data::Ticket /*ticket*/) override { return Data::Wire::Never(); }

  void Cancel(Data::Ticket /*ticket*/) override {}
};

using Key = std::tuple<int, uint32_t, uint32_t>;
using Keys = std::set<Key>;

Keys KeysOf(std::span<const Data::TileId> tiles) {
  Keys result;
  for (const auto tile : tiles) { result.emplace(tile.Zoom, tile.X, tile.Y); }
  return result;
}

struct TileRectangle {
  int Zoom;
  uint32_t Left;
  uint32_t Right;
  uint32_t Top;
  uint32_t Bottom;
};

void Rectangle(Keys &keys, TileRectangle area) {
  for (uint32_t y = area.Top; y <= area.Bottom; ++y) {
    for (uint32_t x = area.Left; x <= area.Right; ++x) { keys.emplace(area.Zoom, x, y); }
  }
}

Around Point(double column) {
  // Independent inverse Mercator, at row 20.25 of a 64x64 grid.
  const double latitude = std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * 20.25 / 64.0))) *
                          180.0 / std::numbers::pi;
  return {.LatitudeDeg = latitude,
          .LongitudeDeg = column / 64.0 * 360.0 - 180.0,
          .Zoom = 6,
          .Levels = 1,
          .Grid = 4};
}
}

int main() {
  using namespace outshine::Test;
  Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
  Data::SourceSet sources(store);
  NoNetwork transport;
  Ground::TilePool pool({}, sources, transport);
  const Ground::GroundStream ground(pool, {.Z = 5, .Grid = 4});
  const std::array path{Point(20.25), Point(22.25)};
  const std::array classification{
      Ground::ClassificationPreparation::SourceWindow{.Zoom = 5, .Ring = 1}};
  const auto planned = PlanTerrainPath(
      path, ground, {.VectorZoom = 6, .Classification = classification, .RoadTiles = {}});
  CHECK(planned, "two camera positions produce an admitted data plan without IO");
  if (!planned) { return Report(); }
  // Mesh blocks are [18,21] and [20,23], both at rows [18,21].
  // A one-cell field halo yields 8x6 fine fields and 5x4 parents.
  Keys fields;
  Rectangle(fields, {.Zoom = 6, .Left = 17, .Right = 24, .Top = 17, .Bottom = 22});
  Rectangle(fields, {.Zoom = 5, .Left = 8, .Right = 12, .Top = 8, .Bottom = 11});
  fields.emplace(2, 1, 1);
  CHECK(KeysOf(planned->Fields) == fields && planned->Fields.size() == 48 + 20 + 1,
        "independent rectangles prove mesh halos, parent fields and sampler fallback coverage");
  Keys vectors;
  Rectangle(vectors, {.Zoom = 6, .Left = 17, .Right = 25, .Top = 17, .Bottom = 23});
  Rectangle(vectors, {.Zoom = 5, .Left = 9, .Right = 12, .Top = 9, .Bottom = 11});
  CHECK(KeysOf(planned->Vectors) == vectors && planned->Vectors.size() == 63 + 12,
        "geometry and classifier windows form independently derived source unions");
  const std::array repeated{path[1], path[0], path[1]};
  const auto shuffled = PlanTerrainPath(
      repeated, ground, {.VectorZoom = 6, .Classification = classification, .RoadTiles = {}});
  CHECK(shuffled && shuffled->Fields == planned->Fields && shuffled->Vectors == planned->Vectors,
        "path order and repeated positions cannot change canonical data demand");
  const std::array seam{Point(32.25)};
  const auto crossing = PlanTerrainPath(seam, ground, {});
  CHECK(crossing && KeysOf(crossing->Fields).contains({2, 1, 1}) &&
            KeysOf(crossing->Fields).contains({2, 2, 1}),
        "a mesh block crossing a fallback seam includes both parent fields, not only the camera "
        "cell");
  CHECK(!PlanTerrainPath({}, ground, {}), "empty paths are refused");
  const std::vector<Around> excessive(TerrainPathPlan::MaximumPoints + 1, path[0]);
  CHECK(!PlanTerrainPath(excessive, ground, {}),
        "oversized point counts fail before planning or IO");
  auto invalid = path;
  invalid[1].Zoom = 7;
  CHECK(!PlanTerrainPath(invalid, ground, {}), "one path cannot change its native source grid");
  invalid[1] = path[1];
  invalid[1].LongitudeDeg = std::numeric_limits<double>::infinity();
  CHECK(!PlanTerrainPath(invalid, ground, {}), "invalid geography is refused");
  const std::array hugeWindow{
      Ground::ClassificationPreparation::SourceWindow{.Zoom = 14, .Ring = 1000}};
  CHECK(!PlanTerrainPath(path, ground, {.Classification = hugeWindow, .RoadTiles = {}}),
        "oversized windows are refused before enumerating millions of cells");
  CHECK(pool.Counters().Posts == 0 && pool.Counters().Fetches == 0 && transport.Starts == 0,
        "path planning starts neither provider requests nor workers");
  return Report();
}
