#include "Check.h"
#include "ClipmapHeightCache.h"
#include "ClipmapHeightUpload.h"
#include "ReadbackTransfer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr size_t kWorkBytes = size_t{1} * 1024 * 1024;

void CheckGpu(Device &device, const Texture &image, const ClipmapHeightCache &cache) {
  for (uint32_t level = 0; level < cache.Grid().Levels; ++level) {
    auto transfer = Require(ReadbackTransfer::Start(
        device, image, {.Layer = level, .Width = cache.Side(), .Height = cache.Side()}));
    const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
    CHECK(bytes && bytes->size() == cache.Values(level).size_bytes(),
          "complete native layer returns");
    if (!bytes || bytes->size() != cache.Values(level).size_bytes()) { continue; }
    const auto expected = cache.Values(level);
    for (size_t i = 0; i < expected.size(); ++i) {
      float actual = 0;
      std::memcpy(&actual, bytes->data() + i * sizeof(float), sizeof(float));
      CHECK(actual == expected[i] || (std::isnan(actual) && std::isnan(expected[i])),
            "GPU slots equal retained CPU slots, including unknown coverage and untouched overlap");
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  auto made = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 3}, kWorkBytes);
  CHECK(made.has_value(), "CPU clipmap allocation is admitted");
  if (!made) { return Report(); }
  auto &cache = *made;
  auto texture = Require(device->CreateTexture(
      {.Width = cache.Side(),
       .Height = cache.Side(),
       .Layers = cache.Grid().Levels,
       .Type = TextureType::Image2DArray,
       .Texels = Format::R32Float,
       .Use = TextureUse::Sampled | TextureUse::CopyDestination | TextureUse::CopySource}));
  auto mismatched = Require(device->CreateTexture({.Width = cache.Side() + 1,
                                                   .Height = cache.Side(),
                                                   .Layers = cache.Grid().Levels,
                                                   .Type = TextureType::Image2DArray,
                                                   .Texels = Format::R32Float,
                                                   .Use = TextureUse::CopyDestination}));
  const std::array<Vec2, 7> cameras{
      {{{0, 0}}, {{0.5, 0.5}}, {{2, 2}}, {{-4, 6}}, {{24, -30}}, {{-100, -100}}, {{200, 200}}}};
  for (const auto camera : cameras) {
    auto update = cache.Plan(camera, kWorkBytes);
    CHECK(update.has_value(), "owned source query is admitted");
    if (!update) { continue; }
    const auto samples = update->Samples();
    const auto points = update->Positions();
    for (size_t i = 0; i < points.size(); ++i) {
      samples[i] = points[i][0] > 150 ? std::numeric_limits<float>::quiet_NaN()
                                      : static_cast<float>(points[i][0] / 8 + points[i][1] / 16);
    }
    auto commands = Require(device->Begin());
    if (!samples.empty()) {
      const auto mismatch =
          UploadClipmapHeights(*device, *commands, *mismatched, cache, *update, kWorkBytes);
      CHECK(!mismatch && mismatch.error().Kind == ErrorKind::Invalid,
            "an incompatible height target is refused before staging");
      const auto refused = UploadClipmapHeights(*device, *commands, *texture, cache, *update, 1);
      CHECK(!refused && refused.error().Kind == ErrorKind::Busy,
            "staging admission is checked before recording writes");
    }
    Require(UploadClipmapHeights(*device, *commands, *texture, cache, *update, kWorkBytes));
    const auto completion = Require(device->Submit(std::move(commands)));
    CHECK(cache.Commit(*update).has_value(), "CPU publication follows accepted GPU submission");
    CHECK(Require(device->Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "recorded staging survives its local owner until GPU completion");
    CheckGpu(*device, *texture, cache);
    if (!samples.empty()) {
      auto staleCommands = Require(device->Begin());
      const auto stale =
          UploadClipmapHeights(*device, *staleCommands, *texture, cache, *update, kWorkBytes);
      CHECK(!stale && stale.error().Kind == ErrorKind::Invalid,
            "late worker data cannot overwrite a newer GPU window");
    }
  }
  return Report();
}
