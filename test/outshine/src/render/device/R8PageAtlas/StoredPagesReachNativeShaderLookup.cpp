#include "AtlasChecks.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct alignas(16) AtlasVector {
  std::array<float, 4> Values;
};

static_assert(sizeof(AtlasVector) == 16 && alignof(AtlasVector) == 16 &&
              offsetof(AtlasVector, Values) == 0);
using Queries = std::array<AtlasVector, 4>;

ReadbackTransfer RecordSamples(Device &device,
                               const R8PageAtlas &atlas,
                               const GraphicsPipeline &pipeline,
                               const Queries &queries) {
  auto commands = Require(device.Begin());
  const auto words = std::bit_cast<std::array<uint32_t, 16>>(queries);
  const auto inputs = Upload(device, *commands, words);
  const auto target =
      Require(device.CreateTexture({.Width = 4,
                                    .Height = 1,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  const std::array bindings{Storage(0, 0, *inputs),
                            Binding{.Set = 0,
                                    .Slot = 1,
                                    .Value = SampledTexture{.Image = {.Image = &atlas.Image()},
                                                            .Sampling = &atlas.Sampling()}},
                            Storage(0, 2, atlas.Lookup())};
  const std::array colours{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = 4, .Height = 1},
                                    .Clip = {.Width = 4, .Height = 1}},
                          .Range = {.Count = 3}}));
  Require(commands->EndRenderPass());
  const auto submitted = Require(device.Submit(std::move(commands)));
  CHECK(submitted.Serial != 0, "the consumer frame submits without waiting for the atlas upload");
  return Require(ReadbackTransfer::Start(device, *target, {.Width = 4, .Height = 1}));
}

void ShaderLookup(Device &device, uint32_t core) {
  auto options = core == 128 ? R8PageAtlasOptions{} : AtlasOptions();
  options.Layout.CoreTexels = core;
  auto atlas =
      Require(R8PageAtlas::Create(device, options, Require(R8PageAtlasStorageFor(options))));
  DrainAtlas(device, atlas);
  const size_t side = size_t{core} + 2;
  std::vector<uint8_t> first(side * side, 17);
  std::vector<uint8_t> second(side * side, 83);
  std::vector<uint8_t> third(side * side, 211);
  const std::array keys{FieldPageAddress{.X = -65, .Y = -64, .Mip = 5},
                        FieldPageAddress{},
                        FieldPageAddress{.X = 12345, .Y = -23456, .Mip = 2}};
  const std::array edits{R8PageEdit{.Slot = 0, .Address = keys[0], .Pixels = first},
                         R8PageEdit{.Slot = 1, .Address = keys[1], .Pixels = second},
                         R8PageEdit{.Slot = 2, .Address = keys[2], .Pixels = third}};
  CHECK(Require(atlas.Update(edits)), "one native upload publishes a complete three-page snapshot");
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  const auto vertex = Shader("fullscreen.vert.spv");
  const auto fragment = Spirv(OUTSHINE_R8_PAGE_ATLAS_SHADER);
  const auto pipeline =
      Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                             .Fragment = ShaderCode{.Spirv = fragment},
                                             .Streams = {},
                                             .Attributes = {},
                                             .Colours = targets,
                                             .Depth = std::nullopt,
                                             .Culling = Cull::None,
                                             .Label = "R8 atlas lookup"}));
  Queries queries{};
  for (size_t i = 0; i < keys.size(); ++i) {
    const float step = std::ldexp(1.0f / 16, static_cast<int>(keys[i].Mip));
    queries[i].Values = {(static_cast<float>(keys[i].X) * static_cast<float>(core) + 1.5f) * step,
                         (static_cast<float>(keys[i].Y) * static_cast<float>(core) + 1.5f) * step,
                         static_cast<float>(keys[i].Mip),
                         0};
  }
  queries[3].Values = {1024, -1024, 5, 0};
  auto before = RecordSamples(device, atlas, *pipeline, queries);
  DrainAtlas(device, atlas);
  const std::array changed{R8PageEdit{.Slot = 1, .Address = keys[1], .Pixels = third}};
  CHECK(Require(atlas.Update(changed)),
        "a later upload replaces one body under a stable spatial key");
  auto after = RecordSamples(device, atlas, *pipeline, queries);
  Queries previous{};
  Queries current{};
  const auto oldBytes = AtlasBytes(device, before);
  const auto newBytes = AtlasBytes(device, after);
  std::memcpy(previous.data(), oldBytes.data(), sizeof(previous));
  std::memcpy(current.data(), newBytes.data(), sizeof(current));
  constexpr std::array<float, 3> oldValues{17.0f / 255, 83.0f / 255, 211.0f / 255};
  constexpr std::array<float, 3> newValues{17.0f / 255, 211.0f / 255, 211.0f / 255};
  for (size_t i = 0; i < keys.size(); ++i) {
    CHECK(previous[i].Values[1] == 1 && current[i].Values[1] == 1 &&
              std::abs(previous[i].Values[0] - oldValues[i]) < 1e-6f &&
              std::abs(current[i].Values[0] - newValues[i]) < 1e-6f,
          "the production shader samples the native owner's exact prior/current atlas and lookup");
  }
  CHECK(previous[3].Values[0] == 0 && previous[3].Values[1] == 0 && current[3].Values[1] == 0,
        "missing native lookup keys cannot sample stale physical pages");
  DrainAtlas(device, atlas);
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  ShaderLookup(*device, 64);
  ShaderLookup(*device, 128);
  return outshine::Test::Report();
}
