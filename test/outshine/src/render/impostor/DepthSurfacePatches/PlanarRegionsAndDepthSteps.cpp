#include "DepthSurfacePatches.h"
#include "Check.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

int main() {
  using namespace outshine::Render;
  using namespace outshine::Test;
  constexpr uint32_t width = 8, height = 6;
  std::vector<float> depths(width * height);
  std::vector<uint32_t> surfaces(width * height, 1);
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) { depths[y * width + x] = 0.3f + 0.01f * x + 0.02f * y; }
  }
  DepthSurfaceSource source{
      .Width = width, .Height = height, .Depth = depths, .Surface = surfaces, .AllowedError = 1e-6};
  const auto plane = BuildDepthSurfacePatches(source);
  CHECK(plane && plane->size() == 1, "an entire slanted depth plane needs one quad");
  if (plane && plane->size() == 1) {
    for (uint32_t y = 0; y < height; ++y) {
      for (uint32_t x = 0; x < width; ++x) {
        CHECK(std::abs(plane->front().At(x + 0.5, y + 0.5) - depths[y * width + x]) <= 1e-6,
              "the retained plane agrees with every captured sample");
      }
    }
  }
  for (uint32_t y = 0; y < height; ++y) {
    for (uint32_t x = 0; x < width; ++x) { depths[y * width + x] = x < width / 2 ? 0.25f : 0.75f; }
  }
  const auto stepped = BuildDepthSurfacePatches(source);
  CHECK(stepped && stepped->size() == 4, "a depth discontinuity splits before vertex creation");
  if (stepped) {
    std::vector<uint32_t> visits(width * height);
    for (const auto &patch : *stepped) {
      for (uint32_t y = patch.Top; y < patch.Bottom; ++y) {
        for (uint32_t x = patch.Left; x < patch.Right; ++x) {
          ++visits[y * width + x];
          CHECK(std::abs(patch.At(x + 0.5, y + 0.5) - depths[y * width + x]) <= 1e-6,
                "discontinuous surfaces never merge across the allowance");
        }
      }
    }
    for (uint32_t count : visits) {
      CHECK(count == 1, "every captured pixel belongs to one patch");
    }
  }
  std::fill(surfaces.begin(), surfaces.end(), 0);
  const auto empty = BuildDepthSurfacePatches(source);
  CHECK(empty && empty->empty(), "empty coverage generates no geometry");
  surfaces[7] = 1;
  depths[7] = 0.5;
  const auto isolated = BuildDepthSurfacePatches(source);
  CHECK(isolated && isolated->size() == 1, "a masked isolated sample retains its depth");
  depths[7] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!BuildDepthSurfacePatches(source), "covered nonfinite depth is rejected");
  depths[7] = 0.5;
  source.AllowedError = -1;
  CHECK(!BuildDepthSurfacePatches(source), "negative error allowances are rejected");
  source.AllowedError = 0;
  source.Width = 0;
  CHECK(!BuildDepthSurfacePatches(source), "empty dimensions are rejected");
  return Report();
}
