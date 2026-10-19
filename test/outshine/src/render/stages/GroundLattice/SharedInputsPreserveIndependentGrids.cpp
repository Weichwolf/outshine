#include "test/harness/render/NativeGroundLatticeProbe.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <span>

namespace {
using namespace outshine::Test::GroundProbe;

void Configure(Device &device, GroundLattice &ground) {
  std::array<ColourTarget, kTargetFormats.size()> targets{};
  for (size_t i = 0; i < targets.size(); ++i) { targets[i].Texels = kTargetFormats[i]; }
  Require(RetryOffline(
      [&] {
        return ground.Configure(
            device,
            {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
            targets);
      },
      std::chrono::seconds(5)));
}

void Place(GroundLattice &ground, HeightPattern &pattern) {
  const auto page = Require(
      RetryOffline([&] { return ground.PlacePage(pattern.Nodes); }, std::chrono::seconds(5)));
  pattern.Tiles = {Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  Require(RetryOffline(
      [&] {
        return ground.SetInstances(std::span(pattern.Tiles).first(1),
                                   std::span(pattern.Tiles).subspan(1));
      },
      std::chrono::seconds(5)));
}
}

int main() {
  using namespace outshine::Test::GroundProbe;
  auto device = Require(CreateDevice({.Validation = true}));
  HeightPattern original;
  for (size_t i = 0; i < original.Fractions.size(); ++i) {
    original.Fractions[i] =
        std::pow(static_cast<float>(i) / (GroundLattice::kSide - 1), kFractionPower);
  }
  for (size_t y = 0; y < GroundLattice::kPageSide; ++y) {
    for (size_t x = 0; x < GroundLattice::kPageSide; ++x) {
      original.Nodes[y * GroundLattice::kPageSide + x] =
          kHeightBase + kHeightAmpE * std::sin(static_cast<float>(x) * kHeightRateE) +
          kHeightAmpN * std::cos(static_cast<float>(y) * kHeightRateN);
    }
  }
  GroundLattice kept;
  {
    GroundLattice published;
    Configure(*device, published);
    Require(RetryOffline([&] { return published.SetGrid(original.Fractions); },
                         std::chrono::seconds(5)));
    Require(kept.ReuseInputsFrom(published, *device));
    CHECK(kept.HeldBytes() == published.HeldBytes(),
          "the replacement retains the complete immutable ground input payload");
  }
  Configure(*device, kept);
  Place(kept, original);
  Lit(*device, kept, "", &original, true);
  GroundLattice changed;
  Require(changed.ReuseInputsFrom(kept, *device));
  Configure(*device, changed);
  HeightPattern replacement = original;
  for (size_t i = 0; i < replacement.Fractions.size(); ++i) {
    replacement.Fractions[i] = static_cast<float>(i) / (GroundLattice::kSide - 1);
  }
  Require(RetryOffline([&] { return changed.SetGrid(replacement.Fractions); },
                       std::chrono::seconds(5)));
  Place(changed, replacement);
  Lit(*device, changed, "", &replacement, true);
  Lit(*device, kept, "", &original, true);
  return Report();
}
