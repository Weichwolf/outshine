#include "test/harness/render/NativeGroundLatticeProbe.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

int main() {
  using namespace outshine::Test::GroundProbe;
  auto device = Require(CreateDevice({.Validation = true}));
  GroundLattice ground;
  CHECK(Require(ground.ReadDrawArguments()).SkirtInstances == 0,
        "empty terrain needs no readback or device");
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
  pattern.Nodes.fill(0.5f);
  const auto page = Require(
      RetryOffline([&] { return ground.PlacePage(pattern.Nodes); }, std::chrono::seconds(5)));
  pattern.Tiles = {Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  Require(RetryOffline([&] { return ground.SetGrid(pattern.Fractions); }, std::chrono::seconds(5)));
  Require(RetryOffline([&] { return ground.SetInstances(pattern.Tiles, {}); },
                       std::chrono::seconds(5)));
  Lit(*device, ground, "", &pattern);
  const auto direct = Require(ground.ReadDrawArguments());
  CHECK(direct.SkirtInstances == 2, "both visible pages draw skirts");
  CHECK(direct.Patches[0].InstanceCount == 32 && direct.Patches[0].IndexCount == 384,
        "direct fallback submits sixteen full-resolution patches per page");
  for (size_t i = 1; i < direct.Patches.size(); ++i) {
    CHECK(direct.Patches[i].InstanceCount == 0, "direct fallback has no coarse patch draws");
  }
  Lit(*device, ground, "", &pattern, true);
  const auto selected = Require(ground.ReadDrawArguments());
  CHECK(selected.SkirtInstances == 2, "GPU patch selection keeps both skirt instances");
  uint64_t references = 0;
  for (size_t i = 0; i < selected.Patches.size(); ++i) {
    const auto &row = selected.Patches[i];
    CHECK(row.InstanceCount == (i == 3 ? 32u : 0u),
          "flat pages independently require all thirty-two patches at the coarsest LOD");
    references += uint64_t{row.IndexCount} * row.InstanceCount;
  }
  CHECK(references == uint64_t{32} * 96u,
        "adaptive draw work includes every instance, with thirty-two boundary triangles per patch");
  ground.SetDrawEnabled(false);
  const auto disabled = Require(ground.ReadDrawArguments());
  CHECK(disabled.SkirtInstances == 0, "disabled ground reports no skirt work");
  for (const auto &row : disabled.Patches) {
    CHECK(row.IndexCount == 0 && row.InstanceCount == 0,
          "disabled ground never reports stale GPU arguments");
  }
  return Report();
}
