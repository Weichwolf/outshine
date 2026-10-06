#include "PlaneSurfacePatches.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr uint32_t width = 8, height = 6;
  std::vector<PlaneSurfaceSample> samples(width * height);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) {
      samples[y * width + x] = {.Surface = 1,
                                .Point = {{static_cast<double>(x), static_cast<double>(y), 10}},
                                .Normal = {{0, 0, -1}}};
    }
  }
  auto patches = BuildPlaneSurfacePatches(samples, width, height);
  CHECK(patches && patches->size() == 1, "one planar surface becomes one patch");
  samples[2 * width + 3].Surface = 0;
  samples[3 * width + 4].Surface = 2;
  samples[4 * width + 2].Point[2] = 11;
  samples[5 * width + 5].Normal = {{0.6, 0, -0.8}};
  patches = BuildPlaneSurfacePatches(samples, width, height);
  CHECK(patches, "empty coverage, material seams, depth steps and normal changes remain valid");
  if (!patches) { return Report(); }
  std::vector<unsigned> visits(samples.size());
  for (const auto &patch : *patches) {
    const auto &first = samples[patch.Sample];
    for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
      for (uint32_t x = patch.Left; x < patch.Right; ++x) {
        const auto &at = samples[y * width + x];
        ++visits[y * width + x];
        CHECK(at.Surface == first.Surface && at.Normal == first.Normal,
              "a patch never merges independent appearance or normals");
        CHECK(std::abs(Dot(at.Point - first.Point, first.Normal)) < 1.0e-6,
              "every represented sample lies on its source plane");
      }
    }
  }
  for (size_t at = 0; at < samples.size(); ++at) {
    CHECK(visits[at] == (samples[at].Surface != 0 ? 1 : 0),
          "every covered sample is represented exactly once and holes stay empty");
  }
  const std::array<Vec3, 4> rays{{{{-1, -1, 1}}, {{1, -1, 1}}, {{1, 1, 1}}, {{-1, 1, 1}}}};
  const auto corners = IntersectPlaneSurface(samples.front(), {}, rays);
  CHECK(corners, "a visible plane reconstructs all four projected corners");
  if (corners) {
    for (const auto &corner : *corners) {
      CHECK(corner[2] == 10 && std::abs(corner[0]) == 10 && std::abs(corner[1]) == 10,
            "the reconstructed patch has the original depth and angular footprint");
    }
  }
  auto invalidRays = rays;
  invalidRays[0][2] = 0;
  CHECK(!IntersectPlaneSurface(samples.front(), {}, invalidRays),
        "a projection singularity cannot produce geometry");
  invalidRays[0][2] = -1;
  CHECK(!IntersectPlaneSurface(samples.front(), {}, invalidRays),
        "a patch crossing the eye plane cannot produce geometry");
  CHECK(!BuildPlaneSurfacePatches(samples, 0, height), "empty dimensions are rejected");
  CHECK(!BuildPlaneSurfacePatches(samples, width, height + 1), "sample count is checked");
  samples.front().Normal[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BuildPlaneSurfacePatches(samples, width, height), "nonfinite source normals are rejected");
  return Report();
}
