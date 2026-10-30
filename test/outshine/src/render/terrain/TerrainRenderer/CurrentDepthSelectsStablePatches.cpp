#include "Check.h"
#include "test/harness/render/NativeBufferReadback.h"
#include <utility>
#include "TerrainPatchBounds.h"
#include "TerrainLattice.h"
#include "TerrainPatchSelection.h"
#include "TerrainPatchMesh.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <vector>

namespace {
constexpr auto kDegenerateScale = 1.0e-30f;
constexpr auto kOutsideShiftM = 100;

using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kImageSide = 64;
constexpr float kBehind = 0.25f, kInFront = 0.9f, kWall = 0.75f;
constexpr Mat4f kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

struct SelectionParameters {
  size_t Tiles = 1;
  float ErrorPx = 0;
  uint32_t ExpectedLevel = 0;
};

void Select(Device &device,
            TerrainPatchSelection &selection,
            float wall,
            std::span<const uint32_t> expected,
            Mat4f clip = kIdentity,
            Vec3 anchor = {},
            SelectionParameters parameters = {}) {
  const auto [tiles, errorPx, expectedLevel] = parameters;
  const auto shape = PyramidOver({.WidthPx = kImageSide, .HeightPx = kImageSide});
  std::vector<uint32_t> depth(shape.Texels, std::bit_cast<uint32_t>(wall));
  std::vector<uint32_t> codes(tiles * TerrainLattice::kPatchCount);
  std::ranges::iota(codes, uint32_t{0});
  selection.BeginFrame();
  for (size_t tile = 0; tile < tiles; ++tile) { selection.Include(tile); }
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  auto pyramid = Upload(device, *commands, depth);
  const auto candidates = Upload(device, *commands, codes);
  Require(selection.Upload(device, *commands));
  FrameContext frame;
  frame.Mvp = clip;
  Require(selection.Cull(*commands,
                         frame,
                         anchor,
                         {.Width = kImageSide, .Height = kImageSide},
                         *pyramid,
                         shape,
                         *candidates,
                         errorPx));
  CHECK(selection.Ready(), "only complete native selection publishes indirect arguments");
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "current-depth cull, reused scan and stable compaction finish");
  const auto args = ReadBufferWords(device,
                                    *selection.Arguments(),
                                    selection.ArgumentOffset(),
                                    4u * sizeof(IndexedIndirectArguments));
  const uint64_t room = selection.Codes()->Description().Bytes / (4u * sizeof(uint32_t));
  uint32_t firstIndex = 0;
  for (uint32_t level = 0; level < 4; ++level) {
    const size_t at = size_t{5} * level;
    CHECK(args[at] == TerrainPatchIndexCounts()[level] &&
              args[at + 1] == (level == expectedLevel ? expected.size() : 0u) &&
              args[at + 2] == firstIndex && args[at + 3] == 0 && args[at + 4] == level * room,
          "each LOD owns its count, exact index range and independent compacted stream");
    firstIndex += TerrainPatchIndexCounts()[level];
  }
  if (!expected.empty()) {
    const auto selected =
        ReadBufferWords(device,
                        *selection.Codes(),
                        uint64_t{args[5u * expectedLevel + 4u]} * sizeof(uint32_t),
                        expected.size_bytes());
    std::vector<uint32_t> wanted(expected.begin(), expected.end());
    for (auto &code : wanted) { code |= expectedLevel << TerrainLattice::kGroundLodShift; }
    CHECK(std::ranges::equal(selected, wanted),
          "surviving patch codes preserve source order within their selected LOD");
  }
}

void ProjectiveScaleUsesTheLargestScreenDisplacement(Device &device,
                                                     TerrainPatchSelection &selection,
                                                     TerrainPatchBounds &selectionData,
                                                     const TerrainInstance &tile,
                                                     std::span<const uint32_t> expected) {
  std::vector<float> heights(TerrainLattice::kPageNodes, 0);
  auto clip = kIdentity;
  for (const float peak : {0.012f, 0.025f}) {
    for (size_t y = 0; y < 4; ++y) {
      for (size_t x = 0; x < 4; ++x) {
        heights[(y * 8u + 4u) * TerrainLattice::kPageSide + x * 8u + 4u] = peak;
      }
    }
    selectionData.Page(0, heights);
    selectionData.Sources(std::span(&tile, 1), 1);
    Select(device, selection, 0, expected, clip, {}, {.ErrorPx = 0.5f, .ExpectedLevel = 3});
    clip[11] = 1;
    clip[15] = 2;
  }
  clip = kIdentity;
  clip[0] = kDegenerateScale;
  clip[5] = kDegenerateScale;
  Select(device, selection, 0, expected, clip, {}, {.ErrorPx = 0.5f});
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  TerrainPatchSelection::Pipelines pipelines;
  TerrainPatchBounds selectionData;
  TerrainPatchSelection selection{selectionData};
  Require(TerrainPatchSelection::Configure(*device, pipelines));
  selection.UsePipelines(pipelines);
  std::vector<float> heights(TerrainLattice::kPageNodes, kBehind);
  selectionData.Page(0, heights);
  TerrainInstance tile;
  tile.Corners = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
  const std::array tiles{tile};
  selectionData.Sources(tiles, 1);
  std::array<uint32_t, TerrainLattice::kPatchCount> all{};
  std::ranges::iota(all, uint32_t{0});
  Select(*device, selection, kWall, {});
  Select(*device, selection, 0, all);
  Select(*device, selection, 0, all, kIdentity, {}, {.ErrorPx = 0.5f, .ExpectedLevel = 3});
  auto behindEye = kIdentity;
  behindEye[15] = -1;
  Select(*device, selection, kWall, all, behindEye);
  Select(*device, selection, kWall, all, kIdentity, Vec3{{0, 0, 1}});
  Select(*device, selection, kWall, {});

  for (size_t y = TerrainLattice::kSide / 2 + 2u; y < TerrainLattice::kPageSide; ++y) {
    std::fill_n(heights.begin() + static_cast<ptrdiff_t>(y * TerrainLattice::kPageSide),
                TerrainLattice::kPageSide,
                kInFront);
  }
  selectionData.Page(0, heights);
  Select(*device, selection, kWall, all);
  selectionData.Sources(tiles, 1);
  Select(*device, selection, kWall, std::span(all).subspan(all.size() / 2));
  for (const size_t count : {size_t{17}, size_t{4097}}) {
    const std::vector<TerrainInstance> many(count, tile);
    selectionData.Sources(many, static_cast<uint32_t>(count));
    std::vector<uint32_t> expected;
    for (uint32_t code = 0; code < count * TerrainLattice::kPatchCount; ++code) {
      if (code % TerrainLattice::kPatchCount >= TerrainLattice::kPatchCount / 2u) {
        expected.push_back(code);
      }
    }
    Select(*device, selection, kWall, expected, kIdentity, {}, {.Tiles = count});
    Select(*device,
           selection,
           0,
           std::span<const uint32_t>{},
           kIdentity,
           {{kOutsideShiftM, 0, 0}},
           {.Tiles = count});
  }
  ProjectiveScaleUsesTheLargestScreenDisplacement(*device, selection, selectionData, tile, all);
  selectionData.Sources(tiles, 1);
  CHECK(!selection.Ready(), "changing canonical bounds invalidates a completed selection");
  selection.BeginFrame();
  selection.Include(0);
  selectionData.Sources(tiles, 1);
  auto commands = Require(RetryOffline([&] { return device->Begin(); }, std::chrono::seconds(5)));
  const auto changed = selection.Upload(*device, *commands);
  CHECK(!changed && changed.error().Kind == ErrorKind::Busy && !selection.Ready(),
        "canonical changes during gathering refuse stale bounds before any GPU upload");
  return Report();
}
