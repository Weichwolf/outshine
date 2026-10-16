#include "GroundPatchSelection.h"
#include "GroundLattice.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kImageSide = 64;
constexpr float kBehind = 0.25f, kInFront = 0.9f, kWall = 0.75f;
constexpr Mat4f kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};

std::vector<uint32_t> Read(Device &device, const Buffer &buffer, uint64_t offset, uint64_t bytes) {
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  auto read = Require(device.CreateBuffer(
      {.Bytes = bytes, .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  Require(commands->Copy(buffer, *read, {.SourceOffset = offset, .Bytes = bytes}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "native patch readback completes");
  const auto mapped = Require(device.MapRead(*read, {.Bytes = bytes}));
  std::vector<uint32_t> result(bytes / sizeof(uint32_t));
  std::memcpy(result.data(), mapped.data(), static_cast<size_t>(bytes));
  Require(device.Unmap(*read));
  return result;
}

void Select(Device &device,
            GroundPatchSelection &selection,
            float wall,
            std::span<const uint32_t> expected,
            Mat4f clip = kIdentity,
            Vec3 anchor = {},
            size_t tiles = 1) {
  const auto shape = PyramidOver({.WidthPx = kImageSide, .HeightPx = kImageSide});
  std::vector<uint32_t> depth(shape.Texels, std::bit_cast<uint32_t>(wall));
  std::vector<uint32_t> codes(tiles * GroundLattice::kPatchCount);
  std::iota(codes.begin(), codes.end(), uint32_t{0});
  selection.BeginFrame();
  for (size_t tile = 0; tile < tiles; ++tile) { selection.Include(tile); }
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  auto pyramid = Upload(device, *commands, depth);
  auto candidates = Upload(device, *commands, codes);
  Require(selection.Upload(device, *commands));
  FrameContext frame;
  frame.Mvp = clip;
  Require(selection.Cull(*commands,
                         frame,
                         anchor,
                         {.Width = kImageSide, .Height = kImageSide},
                         *pyramid,
                         shape,
                         *candidates));
  CHECK(selection.Ready(), "only complete native selection publishes indirect arguments");
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "current-depth cull, reused scan and stable compaction finish");
  const auto args = Read(
      device, *selection.Arguments(), selection.ArgumentOffset(), sizeof(IndexedIndirectArguments));
  CHECK(args[0] == GroundLattice::kPatchIndices && args[1] == expected.size() && args[2] == 0 &&
            args[3] == 0 && args[4] == 0,
        "the aligned scan counter updates instance count without corrupting indexed arguments");
  if (!expected.empty()) {
    const auto selected = Read(device, *selection.Codes(), 0, expected.size_bytes());
    CHECK(std::ranges::equal(selected, expected), "surviving patch codes preserve source order");
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  GroundPatchSelection selection;
  Require(selection.Configure(*device));
  std::vector<float> heights(GroundLattice::kPageNodes, kBehind);
  selection.Page(0, heights);
  GroundInstance tile;
  tile.Corners = {-0.5f, -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f};
  const std::array tiles{tile};
  selection.Sources(tiles, 1);
  std::array<uint32_t, GroundLattice::kPatchCount> all{};
  std::iota(all.begin(), all.end(), uint32_t{0});
  Select(*device, selection, kWall, {});
  Select(*device, selection, 0, all);
  auto behindEye = kIdentity;
  behindEye[15] = -1;
  Select(*device, selection, kWall, all, behindEye);
  Select(*device, selection, kWall, all, kIdentity, Vec3{{0, 0, 1}});
  Select(*device, selection, kWall, {});

  for (size_t y = GroundLattice::kSide / 2 + 2u; y < GroundLattice::kPageSide; ++y) {
    std::fill_n(heights.begin() + static_cast<ptrdiff_t>(y * GroundLattice::kPageSide),
                GroundLattice::kPageSide,
                kInFront);
  }
  selection.Page(0, heights);
  Select(*device, selection, kWall, all);
  selection.Sources(tiles, 1);
  Select(*device, selection, kWall, std::span(all).subspan(all.size() / 2));
  for (const size_t count : {size_t{17}, size_t{4097}}) {
    const std::vector<GroundInstance> many(count, tile);
    selection.Sources(many, static_cast<uint32_t>(count));
    std::vector<uint32_t> expected;
    for (uint32_t code = 0; code < count * GroundLattice::kPatchCount; ++code) {
      if (code % GroundLattice::kPatchCount >= GroundLattice::kPatchCount / 2u) {
        expected.push_back(code);
      }
    }
    Select(*device, selection, kWall, expected, kIdentity, {}, count);
    Select(*device, selection, 0, std::span<const uint32_t>{}, kIdentity, {{100, 0, 0}}, count);
  }
  return Report();
}
