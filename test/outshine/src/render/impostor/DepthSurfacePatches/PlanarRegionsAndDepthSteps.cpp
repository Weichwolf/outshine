#include "DepthSurfacePatches.h"
#include "Check.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Test;
constexpr uint32_t kWidth = 8;
constexpr uint32_t kHeight = 6;
constexpr size_t kPixels = size_t{kWidth} * kHeight;

struct Fixture {
  std::vector<float> Depths = std::vector<float>(kPixels);
  std::vector<uint32_t> Surfaces = std::vector<uint32_t>(kPixels, 1);
  DepthSurfaceSource Source{.Width = kWidth,
                            .Height = kHeight,
                            .Depth = Depths,
                            .Surface = Surfaces,
                            .AllowedError = 1e-6};
};

void CheckPlane() {
  Fixture fixture;
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      fixture.Depths[y * kWidth + x] =
          0.3f + 0.01f * static_cast<float>(x) + 0.02f * static_cast<float>(y);
    }
  }
  const auto plane = BuildDepthSurfacePatches(fixture.Source);
  CHECK(plane && plane->size() == 1, "an entire slanted depth plane needs one quad");
  if (!plane || plane->size() != 1) { return; }
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      CHECK(std::abs(plane->front().At(x + 0.5, y + 0.5) -
                     fixture.Depths[size_t{y} * kWidth + x]) <= 1e-6,
            "the retained plane agrees with every captured sample");
    }
  }
}

void CheckSteps() {
  Fixture fixture;
  for (size_t y = 0; y < kHeight; ++y) {
    for (size_t x = 0; x < kWidth; ++x) {
      fixture.Depths[y * kWidth + x] = x < kWidth / 2 ? 0.25f : 0.75f;
    }
  }
  const auto stepped = BuildDepthSurfacePatches(fixture.Source);
  CHECK(stepped && stepped->size() == 4, "a depth discontinuity splits before vertex creation");
  if (!stepped) { return; }
  std::vector<uint32_t> visits(kPixels);
  for (const auto &patch : *stepped) {
    for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
      for (uint32_t x = patch.Left; x < patch.Right; ++x) {
        ++visits[size_t{y} * kWidth + x];
        CHECK(std::abs(patch.At(x + 0.5, y + 0.5) - fixture.Depths[size_t{y} * kWidth + x]) <= 1e-6,
              "discontinuous surfaces never merge across the allowance");
      }
    }
  }
  for (const uint32_t count : visits) {
    CHECK(count == 1, "every captured pixel belongs to one patch");
  }
}

void CheckAdmission() {
  Fixture fixture;
  std::ranges::fill(fixture.Surfaces, 0);
  const auto empty = BuildDepthSurfacePatches(fixture.Source);
  CHECK(empty && empty->empty(), "empty coverage generates no geometry");
  fixture.Surfaces[7] = 1;
  fixture.Depths[7] = 0.5f;
  const auto isolated = BuildDepthSurfacePatches(fixture.Source);
  CHECK(isolated && isolated->size() == 1, "a masked isolated sample retains its depth");
  fixture.Depths[7] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!BuildDepthSurfacePatches(fixture.Source), "covered nonfinite depth is rejected");
  fixture.Depths[7] = 0.5f;
  fixture.Source.AllowedError = -1;
  CHECK(!BuildDepthSurfacePatches(fixture.Source), "negative error allowances are rejected");
  fixture.Source.AllowedError = 0;
  fixture.Source.Width = 0;
  CHECK(!BuildDepthSurfacePatches(fixture.Source), "empty dimensions are rejected");
}
}

int main() {
  CheckPlane();
  CheckSteps();
  CheckAdmission();
  return Report();
}
