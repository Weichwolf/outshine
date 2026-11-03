#include "Check.h"
#include "ClipmapHeightCache.h"
#include "ClipmapHeightUpload.h"
#include "ClipmapNormalPass.h"
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

float Half(uint16_t bits) {
  const int exponent = static_cast<int>((uint32_t{bits} >> 10u) & 31u);
  const int mantissa = bits & 1023;
  const float value = exponent == 0
                          ? std::ldexp(static_cast<float>(mantissa), -24)
                          : std::ldexp(static_cast<float>(1024 + mantissa), exponent - 25);
  return (bits & 32768) != 0 ? -value : value;
}

void CheckNormals(Device &device, const Texture &normals, const ClipmapHeightCache &cache) {
  for (uint32_t level = 0; level < cache.Grid().Levels; ++level) {
    auto transfer = Require(ReadbackTransfer::Start(
        device, normals, {.Layer = level, .Width = cache.Side(), .Height = cache.Side()}));
    const auto bytes = Require(transfer.Wait(device, std::chrono::seconds(5)));
    CHECK(bytes && bytes->size() == cache.Values(level).size() * 8, "normal layer returns");
    if (!bytes || bytes->size() != cache.Values(level).size() * 8) { continue; }
    const auto side = cache.Side();
    const auto field = cache.Values(level);
    const auto step = static_cast<float>(std::ldexp(cache.Grid().CellM, static_cast<int>(level)));
    for (uint32_t y = 0; y < side; ++y) {
      for (uint32_t x = 0; x < side; ++x) {
        const size_t index = size_t{y} * side + x;
        const std::array h{field[size_t{y} * side + (x + side - 1) % side],
                           field[size_t{y} * side + (x + 1) % side],
                           field[size_t{(y + side - 1) % side} * side + x],
                           field[size_t{(y + 1) % side} * side + x]};
        bool known = std::isfinite(field[index]);
        for (const float height : h) { known = known && std::isfinite(height); }
        std::array<float, 4> expected{};
        if (known) {
          const float east = -(h[1] - h[0]) / (2 * step);
          const float north = -(h[3] - h[2]) / (2 * step);
          const float scale = std::sqrt(east * east + north * north + 1);
          expected = {east / scale, north / scale, 1 / scale, 1};
        }
        std::array<uint16_t, 4> actual{};
        std::memcpy(actual.data(), bytes->data() + index * sizeof(actual), sizeof(actual));
        for (size_t c = 0; c < actual.size(); ++c) {
          CHECK(((uint32_t{actual[c]} >> 10u) & 31u) != 31u &&
                    std::abs(Half(actual[c]) - expected[c]) < 0.001f,
                "updated and retained torus texels match independent central differences");
        }
      }
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  auto made = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 3, .CellM = 0.5}, kWorkBytes);
  CHECK(made.has_value(), "fine normal-height storage is admitted independently of geometry");
  if (!made) { return Report(); }
  auto &cache = *made;
  auto heights =
      Require(device->CreateTexture({.Width = cache.Side(),
                                     .Height = cache.Side(),
                                     .Layers = cache.Grid().Levels,
                                     .Type = TextureType::Image2DArray,
                                     .Texels = Format::R32Float,
                                     .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto normals = Require(device->CreateTexture(
      {.Width = cache.Side(),
       .Height = cache.Side(),
       .Layers = cache.Grid().Levels,
       .Type = TextureType::Image2DArray,
       .Texels = Format::Rgba16Float,
       .Use = TextureUse::Sampled | TextureUse::Storage | TextureUse::CopySource}));
  ClipmapNormalPass pass;
  Require(pass.Configure(*device));
  const std::array<Vec2, 7> cameras{
      {{{0, 0}}, {{0.1, 0.1}}, {{1, 1}}, {{-2, 3}}, {{12, -15}}, {{-50, -50}}, {{100, 100}}}};
  for (const auto camera : cameras) {
    auto update = cache.Plan(camera, kWorkBytes);
    CHECK(update.has_value(), "normal field selects only missing samples");
    if (!update) { continue; }
    for (size_t i = 0; i < update->Positions().size(); ++i) {
      const auto point = update->Positions()[i];
      update->Samples()[i] = point[0] > 90
                                 ? std::numeric_limits<float>::quiet_NaN()
                                 : static_cast<float>(2 * std::sin(point[0] / 8) + point[1] / 20);
    }
    auto commands = Require(device->Begin());
    CHECK(!pass.Encode(*commands, *heights, *heights, cache, *update),
          "an incompatible target cannot record normal writes");
    Require(UploadClipmapHeights(*device, *commands, *heights, cache, *update, kWorkBytes));
    Require(pass.Encode(*commands, *heights, *normals, cache, *update));
    const auto completion = Require(device->Submit(std::move(commands)));
    CHECK(cache.Commit(*update).has_value(), "cache follows accepted copy and normal submission");
    CHECK(Require(device->Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "height copies reach the normal compute pass in the same submission");
    CheckNormals(*device, *normals, cache);
    auto stale = Require(device->Begin());
    if (update->Samples().empty()) {
      Require(pass.Encode(*stale, *heights, *normals, cache, *update));
    } else {
      CHECK(!pass.Encode(*stale, *heights, *normals, cache, *update),
            "old worker updates cannot rewrite current normal fields");
    }
  }
  return Report();
}
