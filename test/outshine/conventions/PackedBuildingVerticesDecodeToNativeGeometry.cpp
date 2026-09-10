#include "Meshed.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const std::array vertices{StoredVertex::Of(Vec3f{{0, 0, 0}}, Vec2f{{1, 2}}, Vec3f{{1, 0, 0}}),
                            StoredVertex::Of(Vec3f{{2, 0, 0}}, Vec2f{{0, 3}}, Vec3f{{0, 1, 0}}),
                            StoredVertex::Of(Vec3f{{0, 2, 0}}, Vec2f{{2, 1}}, Vec3f{{0, 0, 1}})};
  Generators::Meshed adapter;
  CHECK(adapter.Take("triangle", MaterialInstance(0), vertices), "typed packed vertices accepted");
  CHECK(!adapter.Take("incomplete", MaterialInstance(0), std::span(vertices).first(2)) &&
            adapter.Parts() == 1,
        "incomplete input preserves existing parts");
  const auto geometry = adapter.Handed();
  CHECK(geometry.parts() == 1, "one native part");
  const std::array<float, 9> positions{0, 0, 0, 2, 0, 0, 0, 2, 0};
  const std::array<float, 9> normals{1, 0, 0, 0, 1, 0, 0, 0, 1};
  const std::array<float, 6> texture{1, 2, 0, 3, 2, 1};
  const std::array<uint32_t, 3> indices{0, 1, 2};
  CHECK(std::ranges::equal(geometry.positionsOf(0), positions),
        "positions retain their numeric values");
  // Quantization of [-1,1] into 65536 values has half-step 1/65535.
  // UVs scale it by four; these axis normals have at most two half-steps of error.
  constexpr float halfStep = 1.0f / std::numeric_limits<uint16_t>::max();
  constexpr float rounding = 8.0f * std::numeric_limits<float>::epsilon();
  CHECK(std::ranges::equal(
            geometry.normalsOf(0),
            normals,
            [](float a, float b) { return std::abs(a - b) <= 2.0f * halfStep + rounding; }),
        "packed normals decoded through their representation");
  CHECK(std::ranges::equal(
            geometry.textureOf(0),
            texture,
            [](float a, float b) { return std::abs(a - b) <= 4.0f * halfStep + rounding; }),
        "texture coordinates respect the specified quantization bound");
  CHECK(std::ranges::equal(geometry.trianglesOf(0), indices), "triangle ordering retained");
  return Report();
}
