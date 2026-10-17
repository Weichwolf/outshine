#include "test/harness/render/NativeGroundLatticeProbe.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <span>

int main() {
  using namespace outshine::Test::GroundProbe;
  auto device = Require(CreateDevice({.Validation = true}));
  GroundLattice ground;
  std::array<ColourTarget, kTargetFormats.size()> targets;
  for (size_t i = 0; i < targets.size(); ++i) { targets[i].Texels = kTargetFormats[i]; }
  Require(RetryOffline(
      [&] {
        return ground.Configure(
            *device,
            {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
            targets);
      },
      std::chrono::seconds(5)));
  HeightPattern pattern;
  for (size_t i = 0; i < pattern.Fractions.size(); ++i) {
    pattern.Fractions[i] = static_cast<float>(i) / (pattern.Fractions.size() - 1);
  }
  for (size_t i = 0; i < pattern.Nodes.size(); ++i) {
    pattern.Nodes[i] = 0.5f + 0.005f * static_cast<float>(std::sin(static_cast<double>(i) * 0.7));
  }
  const auto page = Require(
      RetryOffline([&] { return ground.PlacePage(pattern.Nodes); }, std::chrono::seconds(5)));
  pattern.Tiles = {Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  Require(RetryOffline([&] { return ground.SetGrid(pattern.Fractions); }, std::chrono::seconds(5)));
  Require(RetryOffline([&] { return ground.SetInstances(pattern.Tiles, {}); },
                       std::chrono::seconds(5)));
  Lit(*device, ground, "", &pattern, true);
  return Report();
}
