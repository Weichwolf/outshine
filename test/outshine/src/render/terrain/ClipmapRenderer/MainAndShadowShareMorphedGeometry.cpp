#include "Check.h"
#include "ClipmapRenderer.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "ClipmapHeightCache.h"
#include "ClipmapHeightUpload.h"
#include "ClipmapRing.h"
#include "ClipmapVisibility.h"
#include "test/harness/render/NativeTerrainRendererProbe.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Test::GroundProbe;
constexpr size_t kBytes = size_t{4} * 1024 * 1024;
constexpr float kEastSlope = 0.01f;
constexpr float kNorthSlope = 0.02f;

void Populate(ClipmapHeightCache &cache, Vec2 camera, bool warped) {
  auto update = cache.Plan(camera, kBytes);
  CHECK(update.has_value(), "native height work is admitted");
  if (!update) { Failed(); }
  const auto samples = update->Samples();
  const auto points = update->Positions();
  for (size_t i = 0; i < points.size(); ++i) {
    samples[i] = static_cast<float>(0.2 + points[i][0] * kEastSlope + points[i][1] * kNorthSlope +
                                    (warped ? 0.1 * std::sin(points[i][0] * 0.8) : 0));
  }
  Require(cache.Validate(*update).has_value()
              ? Result<void>{}
              : std::unexpected(
                    Error{.Kind = ErrorKind::Invalid, .Message = "invalid synthetic height work"}));
  CHECK(cache.Commit(*update).has_value(), "native fields publish");
}

std::unique_ptr<Texture> HeightImage(Device &device, const ClipmapHeightCache &cache) {
  auto image =
      Require(device.CreateTexture({.Width = cache.Side(),
                                    .Height = cache.Side(),
                                    .Layers = cache.Grid().Levels,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::R32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  std::vector<uint32_t> words;
  for (uint32_t i = 0; i < cache.Grid().Levels; ++i) {
    for (const float value : cache.Values(i)) { words.push_back(std::bit_cast<uint32_t>(value)); }
  }
  auto commands = Require(device.Begin());
  const auto source = Upload(device, *commands, words);
  for (uint32_t i = 0; i < cache.Grid().Levels; ++i) {
    Require(
        commands->Copy(*source,
                       *image,
                       {.BufferOffset = uint64_t{i} * cache.Values(i).size_bytes(),
                        .RowBytes = uint64_t{cache.Side()} * sizeof(float),
                        .Region = {.Layer = i, .Width = cache.Side(), .Height = cache.Side()}}));
  }
  Require(device.Submit(std::move(commands)));
  return image;
}

std::unique_ptr<Texture> NormalImage(Device &device, const ClipmapHeightCache &cache) {
  auto image =
      Require(device.CreateTexture({.Width = cache.Side(),
                                    .Height = cache.Side(),
                                    .Layers = cache.Grid().Levels,
                                    .Type = TextureType::Image2DArray,
                                    .Texels = Format::Rgba32Float,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto normal = Vec3{{-kEastSlope, -kNorthSlope, 1}};
  CHECK(Normalise(normal), "normal source has a finite direction");
  std::vector<uint32_t> words(size_t{cache.Side()} * cache.Side() * 4);
  for (size_t i = 0; i < words.size(); i += 4) {
    for (size_t c = 0; c < 3; ++c) {
      words[i + c] = std::bit_cast<uint32_t>(static_cast<float>(normal[c]));
    }
    words[i + 3] = std::bit_cast<uint32_t>(1.0f);
  }
  auto commands = Require(device.Begin());
  const auto source = Upload(device, *commands, words);
  for (uint32_t i = 0; i < cache.Grid().Levels; ++i) {
    Require(
        commands->Copy(*source,
                       *image,
                       {.RowBytes = uint64_t{cache.Side()} * 4 * sizeof(float),
                        .Region = {.Layer = i, .Width = cache.Side(), .Height = cache.Side()}}));
  }
  Require(device.Submit(std::move(commands)));
  return image;
}

struct OutputImages {
  Texture &Depth;
  Texture &Shadow;
  Texture &Normal;
};

void CheckOutputs(Device &device, OutputImages images) {
  const auto pixels = Read(device, images.Depth, 1);
  const auto shadowPixels = Read(device, images.Shadow, 1);
  CHECK(std::ranges::all_of(pixels, [](float value) { return value > 0; }),
        "all ring phases cover the view without cracks");
  for (size_t i = 0; i < pixels.size(); ++i) {
    CHECK(std::abs(pixels[i] - shadowPixels[i]) < 1e-6f,
          "main and shadow use the same morphed surface");
  }
  auto normal = Vec3{{-kEastSlope, 1, kNorthSlope}};
  CHECK(Normalise(normal), "expected normal has a finite direction");
  const std::array<float, 4> expected{static_cast<float>(normal[0]),
                                      static_cast<float>(normal[1]),
                                      static_cast<float>(normal[2]),
                                      1};
  CheckTarget({.Colour = Read(device, images.Normal, 4), .Depth = pixels},
              expected,
              "finer supplied normals remain independent of the geometry resolution and morph");
}

void Draw(Device &device, const ClipmapRenderer &renderer, Vec2 camera, bool warped) {
  auto heights = ClipmapHeightCache::Make({.HalfCells = 8, .Levels = 3}, kBytes);
  auto normals = ClipmapHeightCache::Make({.HalfCells = 16, .Levels = 3, .CellM = 0.5}, kBytes);
  CHECK(heights && normals, "two resolutions are admitted independently");
  if (!heights || !normals) { Failed(); }
  Populate(*heights, camera, warped);
  Populate(*normals, camera, false);
  auto records = MakeClipmapRings(*heights, *normals, {.Camera = camera, .PreviousCamera = camera});
  CHECK(records.has_value(), "prepared fields bind to their ring records");
  if (!records) { Failed(); }
  const auto heightImage = HeightImage(device, *heights);
  const auto normalImage = NormalImage(device, *normals);
  std::vector<uint32_t> recordWords(records->size() * sizeof(ClipmapRing) / sizeof(uint32_t));
  std::memcpy(recordWords.data(), records->data(), recordWords.size() * sizeof(uint32_t));
  std::array<std::unique_ptr<Texture>, 5> targets;
  std::array<ColourAttachment, 5> attachments;
  for (size_t i = 0; i < targets.size(); ++i) {
    targets[i] = Target(device, kTargetFormats[i], false);
    attachments[i] = {.Image = {.Image = targets[i].get()}, .Begin = Load::Clear};
  }
  auto depth = Target(device, Format::Depth32Float, true);
  auto shadow = Target(device, Format::Depth32Float, true);
  auto commands = Require(device.Begin());
  const auto rings = Upload(device, *commands, recordWords);
  const std::array<uint32_t, 3> classWords{0, 0, ~0u};
  const auto classes = Upload(device, *commands, classWords);
  std::array<float, kPaletteFloats> palette{};
  palette[0] = std::bit_cast<float>(1u);
  palette[2] = 1;
  for (size_t row = 0; row < 2; ++row) {
    for (size_t c = 0; c < 4; ++c) { palette[4 + row * 4 + c] = kSurface[c]; }
  }
  palette[12] = palette[13] = kNoRockSlope;
  const auto paletteBuffer =
      Upload(device,
             *commands,
             std::span(reinterpret_cast<const uint32_t *>(palette.data()), palette.size()));
  TerrainViewUniform view;
  view.Current.Column.fill(0);
  view.Current[0] = 1.0f / 20;
  view.Current[9] = -1.0f / 20;
  view.Current[6] = 0.1f;
  view.Current[14] = 0.5f;
  view.Current[15] = 1;
  view.Previous = view.Current;
  LightingUniform lighting{};
  lighting.Up = {{0, 1, 0, 0}};
  const auto material = PackMaterial(MaterialBinding{}, 1);
  const std::array bindings{
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}},
      Storage(2, 0, *classes),
      Storage(2, 1, *paletteBuffer),
      Binding{.Set = 3, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(material))}},
      Binding{
          .Set = 3, .Slot = 1, .Value = Uniform{.Bytes = std::as_bytes(std::span(&lighting, 1))}}};
  std::vector<std::array<float, 2>> heightRanges(records->size());
  for (uint32_t level = 0; level < heightRanges.size(); ++level) {
    heightRanges[level] = ClipmapHeightRange(*heights, level);
  }
  const ClipmapFields fields{.Heights = heightImage.get(),
                             .Normals = normalImage.get(),
                             .RingBuffer = rings.get(),
                             .Rings = *records,
                             .HeightRanges = heightRanges};
  ClipmapDrawList selection;
  Require(renderer.Select(fields, view.Current, selection));
  CHECK(selection.Triangles > 0 && selection.Triangles < 1344,
        "native culling retains visible coverage and removes some reference geometry");
  StageSubmission submission;
  const PassRecording into{.Commands = commands.get(),
                           .Submission = submission,
                           .Area = {.Width = kWidth, .Height = kHeight},
                           .Clip = {.Width = kWidth, .Height = kHeight}};
  Require(commands->BeginRenderPass(
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  Require(renderer.Encode(into, fields, bindings, ClipmapPass::Surface, &selection));
  Require(commands->EndRenderPass());
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{.Image = {.Image = shadow.get()}, .Begin = Load::Clear}}));
  Require(
      renderer.Encode(into, fields, std::span(bindings).first(1), ClipmapPass::Depth, &selection));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "both native terrain passes finish");
  CheckOutputs(device, {.Depth = *depth, .Shadow = *shadow, .Normal = *targets[1]});
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  ClipmapRenderer renderer;
  std::array<ColourTarget, kTargetFormats.size()> targets;
  for (size_t i = 0; i < targets.size(); ++i) { targets[i].Texels = kTargetFormats[i]; }
  Require(renderer.Configure(
      *device,
      {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
      targets,
      8,
      kBytes));
  const auto refused = renderer.Configure(
      *device,
      {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
      targets,
      8,
      1);
  CHECK(!refused && refused.error().Kind == ErrorKind::Busy,
        "failed geometry admission keeps the previous drawable renderer");
  for (const auto camera : {Vec2{{0.5, 0.5}}, Vec2{{2.5, 3.5}}, Vec2{{-1.5, -2.5}}}) {
    Draw(*device, renderer, camera, false);
    Draw(*device, renderer, camera, true);
  }
  return Report();
}
