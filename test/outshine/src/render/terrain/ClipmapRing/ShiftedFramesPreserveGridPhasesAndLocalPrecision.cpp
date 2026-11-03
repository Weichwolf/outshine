#include "Check.h"
#include "ClipmapHeightCache.h"
#include "ClipmapRing.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr size_t kBytes = size_t{4} * 1024 * 1024;

void Populate(ClipmapHeightCache &cache, Vec2 camera, float height = 0) {
  auto update = cache.Plan(camera, kBytes);
  CHECK(update.has_value(), "height query fits admission");
  if (!update) { return; }
  std::ranges::fill(update->Samples(), height);
  CHECK(cache.Commit(*update).has_value(), "height source publishes");
}

void Phases() {
  for (const auto camera : {Vec2{{0.5, 0.5}}, Vec2{{2.5, 3.5}}, Vec2{{-1.5, -2.5}}}) {
    auto heights = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 3}, kBytes);
    auto normals = ClipmapHeightCache::Make({.HalfCells = 16, .Levels = 3, .CellM = 0.5}, kBytes);
    CHECK(heights && normals, "compatible geometry and finer normal grids build");
    if (!heights || !normals) { continue; }
    Populate(*heights, camera);
    Populate(*normals, camera);
    const auto rings =
        MakeClipmapRings(*heights, *normals, {.Camera = camera, .PreviousCamera = camera});
    CHECK(rings && rings->size() == 3, "one bound record per prepared level");
    if (!rings) { continue; }
    for (size_t i = 0; i < rings->size(); ++i) {
      const auto &ring = (*rings)[i];
      CHECK(ring.Morph[0] >= 0 && ring.Morph[0] < 2 && ring.Morph[1] >= 0 && ring.Morph[1] < 2,
            "continuous camera offset lies inside the retained snap cell");
      CHECK(ring.HeightAddress[2] == 19 && ring.NormalAddress[2] == 35,
            "geometry and normal fields retain their own toroidal widths");
      CHECK(ring.CentreStep[3] == (i + 1 < rings->size() ? 1.0f : 0.0f),
            "outermost ring never reads an absent parent");
      if (i + 1 == rings->size()) { continue; }
      const auto &parent = (*rings)[i + 1];
      for (size_t axis = 0; axis < 2; ++axis) {
        const float position = ring.CentreStep[axis] / parent.CentreStep[2] -
                               parent.CentreStep[axis] / parent.CentreStep[2];
        CHECK(position == static_cast<float>(ring.Offsets[axis]),
              "parent phase matches the actual shared edge");
      }
    }
  }
}

void PrecisionAndMissingCoverage() {
  constexpr Vec2 camera{{1000000000.5, -999999999.5}};
  auto heights = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 2}, kBytes);
  auto normals = ClipmapHeightCache::Make({.HalfCells = 16, .Levels = 2, .CellM = 0.5}, kBytes);
  CHECK(heights && normals, "shifted fields build");
  if (!heights || !normals) { return; }
  const ClipmapView view{.Camera = camera,
                         .PreviousCamera = camera,
                         .Shift = {{-1000000000.0, 0, -1000000000.0}},
                         .PreviousShift = {{-999999999.875, 0, -1000000000.25}}};
  CHECK(!MakeClipmapRings(*heights, *normals, view), "unpublished fields cannot draw");
  Populate(*heights, camera);
  Populate(*normals, camera);
  const auto rings = MakeClipmapRings(*heights, *normals, view);
  CHECK(rings.has_value(), "large world coordinates still produce small local drawing coordinates");
  if (!rings) { return; }
  CHECK((rings->front().RelativeCentres == std::array<float, 4>{0, 0, 0.125f, 0.25f}),
        "subtracting frame shifts before Float32 conversion preserves sub-metre motion");
  CHECK(!MakeClipmapRings(*heights, *normals, {.Camera = {{0, 0}}}),
        "an unprepared camera position cannot morph an unrelated grid");
  Populate(*normals, {{1000000200.0, -999999800.0}}, std::numeric_limits<float>::quiet_NaN());
  CHECK(!MakeClipmapRings(*heights, *normals, view),
        "missing fine-normal coverage cannot impersonate a prepared field");
}
}

int main() {
  Phases();
  PrecisionAndMissingCoverage();
  return Report();
}
