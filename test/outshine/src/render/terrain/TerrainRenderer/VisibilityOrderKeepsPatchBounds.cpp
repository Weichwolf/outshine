#include "terrain/TerrainResidency.h"
#include "test/harness/render/NativeTerrainRendererProbe.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include "test/harness/render/NativeBufferReadback.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <utility>

namespace {
using namespace outshine::Test::GroundProbe;

GroundTile FlatPage(TerrainRenderer &ground, std::array<float, 2> east, float height) {
  std::array<float, TerrainLattice::kPageNodes> nodes{};
  nodes.fill(height);
  const auto page =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  const float centre = (east[0] + east[1]) * 0.5f;
  auto tile = Tile(page, {east[0] - centre, east[1] - centre});
  tile.Instance.Row[12] = centre;
  tile.LowM = height;
  tile.HighM = height;
  return tile;
}

struct SelectionCase {
  float DepthSlope;
  uint32_t VisibleTile;
  bool Uniform;
};

void SelectVisiblePage(Device &device, TerrainRenderer &ground, SelectionCase input) {
  const auto [depthSlope, expectedTile, uniform] = input;
  FrameContext frame;
  frame.Mvp = kIdentity;
  frame.Mvp[5] = -1;
  frame.Mvp[3] = depthSlope;
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  const auto shape = PyramidOver({.WidthPx = kWidth, .HeightPx = kHeight});
  const auto pyramid =
      Upload(device, *commands, std::vector<uint32_t>(shape.Texels, std::bit_cast<uint32_t>(0.5f)));
  Require(ground.Cull(frame, {}, *commands));
  CHECK(ground.Drawn() == 2, "CPU visibility retains both source pages before depth selection");
  StageSubmission submission;
  Require(ground.SelectPatches(frame,
                               {},
                               {.Commands = commands.get(),
                                .Submission = submission,
                                .Area = {.Width = kWidth, .Height = kHeight},
                                .Clip = {.Width = kWidth, .Height = kHeight}},
                               *pyramid,
                               shape));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "source permutation and native patch compaction complete before residency changes");
  const auto arguments = Require(ground.ReadDrawArguments());
  CHECK(arguments.SkirtInstances == 2, "depth selection keeps conservative source skirt residency");
  for (size_t level = 0; level < arguments.Patches.size(); ++level) {
    const auto &row = arguments.Patches[level];
    CHECK(row.InstanceCount == (level == 3 ? TerrainLattice::kPatchCount : 0),
          "only the independently known near page survives at its exact flat-page LOD");
    if (row.InstanceCount == 0) { continue; }
    const auto selected =
        outshine::Test::Gpu::ReadBufferWords(device,
                                             *ground.SelectedPatches(),
                                             uint64_t{row.FirstInstance} * sizeof(uint32_t),
                                             row.InstanceCount * sizeof(uint32_t));
    for (size_t patch = 0; patch < selected.size(); ++patch) {
      const uint32_t expected = static_cast<uint32_t>(patch) |
                                (expectedTile << TerrainLattice::kGroundPatchBits) |
                                (3u << TerrainLattice::kGroundLodShift) |
                                (uniform ? TerrainLattice::kGroundUniformBit : 0u);
      CHECK(selected[patch] == expected,
            "GPU patch bounds follow the visible source ordinal and retain its grid class");
    }
  }
}
}

int main() {
  using namespace outshine::Test::GroundProbe;
  auto device = Require(CreateDevice({.Validation = true}));
  outshine::Render::TerrainResidency resident;
  TerrainRenderer ground{resident};
  std::array<ColourTarget, kTargetFormats.size()> targets{};
  for (size_t i = 0; i < targets.size(); ++i) { targets[i].Texels = kTargetFormats[i]; }
  Require(RetryOffline(
      [&] {
        return ground.Configure(
            *device,
            {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
            targets);
      },
      std::chrono::seconds(5)));
  Require(ground.ConfigurePatchSelection());
  std::array<float, TerrainLattice::kSide> fractions{};
  for (size_t i = 0; i < fractions.size(); ++i) {
    fractions[i] = static_cast<float>(i) / (fractions.size() - 1);
  }
  Require(RetryOffline([&] { return ground.SetGrid(fractions); }, std::chrono::seconds(5)));
  const std::array tiles{FlatPage(ground, {kInner, -kLeft}, 0.25f),
                         FlatPage(ground, {kLeft, -kInner}, 0.75f)};
  for (const size_t realCount : {size_t{2}, size_t{0}, size_t{1}, size_t{2}}) {
    Require(RetryOffline(
        [&] {
          return ground.SetInstances(std::span(tiles).first(realCount),
                                     std::span(tiles).subspan(realCount));
        },
        std::chrono::seconds(5)));
    SelectVisiblePage(
        *device,
        ground,
        {.DepthSlope = 0.25f, .VisibleTile = realCount == 1 ? 1u : 0u, .Uniform = realCount != 2});
    SelectVisiblePage(
        *device, ground, {.DepthSlope = -0.25f, .VisibleTile = 1u, .Uniform = realCount != 2});
    Require(ground.SetInstances({}, {}));
    auto commands = Require(RetryOffline([&] { return device->Begin(); }, std::chrono::seconds(5)));
    Require(ground.Cull({}, {}, *commands));
    const auto done = Require(device->Submit(std::move(commands)));
    CHECK(Require(device->Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "empty residency completes without reusing old patch work");
    CHECK(ground.Drawn() == 0 && Require(ground.ReadDrawArguments()).SkirtInstances == 0,
          "clearing the current generation suppresses previous source draw arguments");
  }
  return Report();
}
