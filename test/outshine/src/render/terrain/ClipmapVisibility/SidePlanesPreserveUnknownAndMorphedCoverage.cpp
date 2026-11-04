#include "Check.h"
#include "ClipmapVisibility.h"
#include "ClipmapHeightCache.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

void CheckHeightRanges() {
  constexpr size_t bytes = size_t{1024} * 1024;
  auto cache = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 2}, bytes);
  CHECK(cache.has_value(), "bounded height cache is available");
  if (!cache) { return; }
  CHECK(std::isnan(ClipmapHeightRange(*cache, 0)[0]), "unpublished bounds remain unknown");
  auto update = cache->Plan({}, bytes);
  CHECK(update.has_value(), "both levels fit the admitted update");
  if (!update) { return; }
  size_t offset = 0;
  for (const auto &region : update->Regions()) {
    const size_t count = size_t{region.Width} * region.Height;
    std::ranges::fill(update->Samples().subspan(offset, count), region.Level == 0 ? 0.0f : 10.0f);
    offset += count;
  }
  CHECK(cache->Commit(*update).has_value(), "source levels are published");
  CHECK((ClipmapHeightRange(*cache, 0) == std::array{0.0f, 10.0f}),
        "bounds include parent samples used by geometric morphing");
  CHECK((ClipmapHeightRange(*cache, 1) == std::array{10.0f, 10.0f}),
        "the outermost ring has no invented parent");
}

void CheckSides() {
  const ClipmapFrustum frustum(Mat4f{});
  ClipmapRing ring;
  ring.CentreStep[2] = 1;
  const ClipmapBlock centre{.Low = {-1, -1}, .High = {1, 1}};
  CHECK(frustum.Intersects(centre, ring, {0, 0}, 0), "the central surface remains visible");
  for (const float sign : {-1.0f, 1.0f}) {
    ring.RelativeCentres[0] = sign * 4;
    CHECK(!frustum.Intersects(centre, ring, {0, 0}, 0), "side planes reject disjoint terrain");
  }
  ring.RelativeCentres[0] = 0;
  CHECK(!frustum.Intersects(centre, ring, {2, 3}, 0), "known heights above the view are rejected");
  CHECK(frustum.Intersects(centre, ring, {0, 3}, 0), "a visible morph parent preserves the block");
  CHECK(frustum.Intersects(centre, ring, {2, 3}, -2), "the exact render height shift is included");
  ring.RelativeCentres[1] = 100;
  CHECK(frustum.Intersects(centre, ring, {0, 0}, 0),
        "depth-clamped geometry is not near/far culled");
  const float nan = std::numeric_limits<float>::quiet_NaN();
  ring.RelativeCentres[0] = 100;
  CHECK(frustum.Intersects(centre, ring, {nan, nan}, 0),
        "unknown terrain is never an occlusion proof");
  Mat4f invalid;
  invalid.At(0, 0) = nan;
  CHECK(ClipmapFrustum(invalid).Intersects(centre, ring, {0, 0}, 0),
        "unknown camera transforms cannot discard terrain");
}

void CheckPerspectiveCrossings() {
  Mat4f projection;
  projection.Column.fill(0);
  projection.At(0, 0) = projection.At(1, 1) = projection.At(3, 2) = 1;
  const ClipmapFrustum frustum(projection);
  ClipmapRing ring;
  ring.CentreStep[2] = 1;
  const ClipmapBlock centre{.Low = {-1, -1}, .High = {1, 1}};
  ring.RelativeCentres[1] = -10;
  CHECK(frustum.Intersects(centre, ring, {-1, 1}, 0), "forward terrain survives perspective");
  ring.RelativeCentres[1] = 10;
  CHECK(!frustum.Intersects(centre, ring, {-1, 1}, 0), "terrain wholly behind the eye is rejected");
  ring.RelativeCentres[1] = 0;
  CHECK(frustum.Intersects(centre, ring, {-1, 1}, 0), "eye-crossing bounds remain conservative");
}

void CheckBoundaryRounding() {
  Mat4f projection;
  for (const float scale : {1.0f, 1000000.0f}) {
    projection.At(0, 0) = 1 / scale;
    const ClipmapFrustum frustum(projection);
    ClipmapRing ring;
    ring.CentreStep[2] = scale;
    ring.RelativeCentres[0] = 2 * scale;
    const ClipmapBlock edge{.Low = {-1, -1}, .High = {1, 1}};
    CHECK(frustum.Intersects(edge, ring, {0, 0}, 0), "touching float boundaries remain visible");
    ring.RelativeCentres[0] = 4 * scale;
    CHECK(!frustum.Intersects(edge, ring, {0, 0}, 0),
          "the numerical guard does not disable culling");
  }
}
}

int main() {
  CheckHeightRanges();
  CheckSides();
  CheckPerspectiveCrossings();
  CheckBoundaryRounding();
  return Report();
}
