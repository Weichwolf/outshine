#ifndef OUTSHINE_TEST_RENDER_NATIVEGROUNDLATTICEPROBE_H
#define OUTSHINE_TEST_RENDER_NATIVEGROUNDLATTICEPROBE_H

#include "GroundLattice.h"
#include "GroundRasterReference.h"
#include "SurfaceOutputs.h"
#include "StageSubmission.h"
#include "SubjectMaterialPacking.h"
#include "SubjectLightingUniform.h"
#include "ReadbackTransfer.h"
#include "NativeGpuCheck.h"
#include "NativeOffline.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <ios>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Test::GroundProbe {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 64;
constexpr uint32_t kHeight = 48;
constexpr size_t kPixels = size_t{kWidth} * kHeight;
constexpr Mat4f kIdentity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
constexpr float kLeft = -0.875f, kInner = 0.125f, kVertical = 0.75f;
constexpr float kDepthTolerance = 0.000001f;
constexpr size_t kPaletteFloats = 30;
constexpr float kNoRockSlope = 90;
constexpr std::array<float, 3> kDepthPerspective{{0.08f, -0.04f, 0.02f}};
constexpr float kFractionPower = 1.25f;
constexpr float kHeightBase = 0.5f;
constexpr float kHeightAmpE = 0.1f, kHeightRateE = 0.7f;
constexpr float kHeightAmpN = 0.08f, kHeightRateN = 0.4f;
constexpr std::array<float, 4> kSurface{0.25f, 0.5f, 0.75f, 1.0f};
constexpr std::array kTargetFormats{Format::Rgba32Float,
                                    Format::Rgba32Float,
                                    Format::Rgba32Float,
                                    Format::Rgba32Float,
                                    Format::Rg16Float};

struct HeightPattern {
  std::array<float, GroundLattice::kSide> Fractions{};
  std::array<float, GroundLattice::kPageNodes> Nodes{};
  std::array<GroundTile, 2> Tiles{};
};

struct alignas(16) SubjectView {
  Mat4f Current = kIdentity, Previous = kIdentity, Light = kIdentity;
  Vec4f Shift{}, PreviousShift{};
};

struct alignas(16) DepthView {
  Mat4f Light = kIdentity;
  Vec4f Shift{};
};

constexpr size_t kSubjectViewBytes = 224;
constexpr size_t kSubjectShiftOffset = 192;
static_assert(sizeof(SubjectView) == kSubjectViewBytes &&
              offsetof(SubjectView, Shift) == kSubjectShiftOffset);
constexpr size_t kDepthViewBytes = 80;
static_assert(sizeof(DepthView) == kDepthViewBytes && offsetof(DepthView, Shift) == sizeof(Mat4f));

inline GroundTile Tile(PageId page, std::array<float, 2> east) {
  GroundTile tile;
  tile.Instance.Corners = {
      east[0], -kVertical, east[1], -kVertical, east[0], kVertical, east[1], kVertical};
  tile.Instance.Page = static_cast<float>(page);
  tile.Instance.StepE = tile.Instance.StepN = 1;
  tile.LowM = 0;
  tile.HighM = 1;
  return tile;
}

inline std::unique_ptr<Texture> Target(Device &device, Format format, bool depth) {
  return Require(device.CreateTexture(
      {.Width = kWidth,
       .Height = kHeight,
       .Texels = format,
       .Use = (depth ? TextureUse::DepthAttachment : TextureUse::ColourAttachment) |
              TextureUse::CopySource}));
}

inline std::vector<float> Read(Device &device, Texture &texture, size_t channels) {
  auto read = Require(RetryOffline(
      [&] {
        return ReadbackTransfer::Start(device, texture, {.Width = kWidth, .Height = kHeight});
      },
      std::chrono::seconds(5)));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes, "actual terrain output completes within the explicit offline budget");
  if (!bytes) { Failed(); }
  CHECK(bytes->size() == kPixels * channels * sizeof(float),
        "native terrain readback keeps its format");
  std::vector<float> pixels(kPixels * channels);
  std::memcpy(pixels.data(), bytes->data(), bytes->size());
  return pixels;
}

inline void CheckDepth(std::span<const float> pixels, bool drawn) {
  size_t mismatch = 0;
  size_t covered = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const float cx = (static_cast<float>(x) + 0.5f) * 2 / kWidth - 1;
      const float cy = (static_cast<float>(y) + 0.5f) * 2 / kHeight - 1;
      const bool left = cx > kLeft && cx < -kInner && std::abs(cy) < kVertical;
      const bool right = cx > kInner && cx < -kLeft && std::abs(cy) < kVertical;
      float expected = 0;
      if (drawn && left) { expected = kSurface[0]; }
      if (drawn && right) { expected = kSurface[2]; }
      if (std::abs(pixels[size_t{y} * kWidth + x] - expected) > kDepthTolerance) { ++mismatch; }
      if (pixels[size_t{y} * kWidth + x] > 0) { ++covered; }
    }
  }
  CHECK(mismatch == 0, "every real and virtual terrain pixel has its independently expected depth");
  CHECK((covered > 0) == drawn, "disabled drawing leaves the cleared target empty");
}

inline void Save(std::span<const float> pixels, const std::string &prefix) {
  if (prefix.empty()) { return; }
  std::ofstream file(prefix + "-depth.bin", std::ios::binary);
  file.write(reinterpret_cast<const char *>(pixels.data()),
             static_cast<std::streamsize>(pixels.size_bytes()));
  file.flush();
  CHECK(file, "optional terrain diagnostic retains actual lossless readback");
}

inline void
Cast(Device &device, const GroundLattice &ground, const std::string &prefix, bool drawn) {
  auto depth = Target(device, Format::Depth32Float, true);
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  Require(commands->BeginRenderPass(
      {.Colours = {},
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  DepthView view;
  const std::array bindings{
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}}};
  StageSubmission submission;
  Require(ground.Cast({.Commands = commands.get(),
                       .Submission = submission,
                       .Area = {.Width = kWidth, .Height = kHeight},
                       .Clip = {.Width = kWidth, .Height = kHeight}},
                      view.Light,
                      view.Shift,
                      bindings));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the actual terrain shadow pass completes");
  auto pixels = Read(device, *depth, 1);
  CheckDepth(pixels, drawn);
  if (drawn) { Save(pixels, prefix); }
}

struct SurfacePixels {
  std::span<const float> Colour, Depth;
};

inline void CheckTarget(SurfacePixels pixels, std::span<const float> value, const char *message) {
  size_t errors = 0;
  for (size_t i = 0; i < kPixels; ++i) {
    for (size_t c = 0; c < value.size(); ++c) {
      const float expected = pixels.Depth[i] > 0 ? value[c] : 0;
      if (pixels.Colour[i * 4 + c] != expected) { ++errors; }
    }
  }
  CHECK(errors == 0, message);
}

inline void CheckMetalRough(Device &device, Texture &texture, std::span<const float> depths) {
  auto read = Require(RetryOffline(
      [&] {
        return ReadbackTransfer::Start(device, texture, {.Width = kWidth, .Height = kHeight});
      },
      std::chrono::seconds(5)));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes && bytes->size() == kPixels * 2 * sizeof(uint16_t),
        "metallic/roughness retains its actual two-channel half-float format");
  if (!bytes || bytes->size() != kPixels * 2 * sizeof(uint16_t)) { Failed(); }
  std::vector<uint16_t> pixels(kPixels * 2);
  std::memcpy(pixels.data(), bytes->data(), bytes->size());
  bool correct = true;
  constexpr uint16_t one = 0x3c00;
  for (size_t i = 0; i < kPixels; ++i) {
    correct &= pixels[i * 2] == 0 && pixels[i * 2 + 1] == (depths[i] > 0 ? one : 0);
  }
  CHECK(correct, "every metallic/roughness pixel preserves zero metal and unit roughness");
}

inline std::vector<float> PatternControl(Device &device,
                                         const HeightPattern &pattern,
                                         std::span<const Binding> bindings,
                                         std::span<const ColourAttachment> attachments) {
  GroundPipelineBinding reference;
  std::array<ColourTarget, kTargetFormats.size()> formats;
  for (size_t i = 0; i < formats.size(); ++i) { formats[i].Texels = kTargetFormats[i]; }
  Require(reference.ConfigureLit(
      device,
      {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
      formats));
  auto target = Target(device, Format::Depth32Float, true);
  auto control = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  DrawGroundReference(
      device,
      *control,
      reference,
      {.Fractions = pattern.Fractions, .Heights = pattern.Nodes, .Tiles = pattern.Tiles},
      bindings,
      kWidth,
      kHeight,
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}});
  const auto completed = Require(device.Submit(std::move(control)));
  CHECK(Require(device.Wait(completed, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the independent complete-grid terrain control finishes");
  return Read(device, *target, 1);
}

inline void Lit(Device &device,
                GroundLattice &ground,
                const std::string &prefix,
                const HeightPattern *pattern = nullptr) {
  constexpr size_t colours = 5;
  std::array<std::unique_ptr<Texture>, colours> targets;
  std::array<ColourAttachment, colours> attachments;
  for (size_t i = 0; i < colours; ++i) {
    targets[i] = Target(device, kTargetFormats[i], false);
    attachments[i] = {.Image = {.Image = targets[i].get()}, .Begin = Load::Clear};
  }
  auto depth = Target(device, Format::Depth32Float, true);
  std::array<uint32_t, 3> classWords{0, 0, ~0u};
  std::array<float, kPaletteFloats> palette{};
  palette[0] = std::bit_cast<float>(1u);
  palette[1] = std::bit_cast<float>(0u);
  palette[2] = 1;
  for (size_t row = 0; row < 2; ++row) {
    for (size_t c = 0; c < 4; ++c) { palette[4 + row * 4 + c] = kSurface[c]; }
  }
  palette[12] = palette[13] = kNoRockSlope;
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  const auto classes = Upload(device, *commands, classWords);
  const auto groundPalette =
      Upload(device,
             *commands,
             std::span(reinterpret_cast<const uint32_t *>(palette.data()), palette.size()));
  FrameContext frame;
  frame.Mvp = kIdentity;
  frame.Mvp[5] = -1;
  if (pattern != nullptr) {
    frame.Mvp[3] = kDepthPerspective[0];
    frame.Mvp[7] = kDepthPerspective[1];
    frame.Mvp[11] = kDepthPerspective[2];
  }
  Require(ground.Cull(frame, {}, *commands));
  CHECK(ground.Drawn() == 2,
        "CPU frustum culling preserves both visible classes and rejects the distant tile");
  Require(commands->BeginRenderPass(
      {.Colours = attachments,
       .Depth = DepthAttachment{.Image = {.Image = depth.get()}, .Begin = Load::Clear}}));
  SubjectView view;
  view.Current = frame.Mvp;
  SubjectLightingUniform lighting{};
  lighting.Up = {{0, 0, 1, 0}};
  const auto material = PackSubjectMaterial(SubjectMaterial{}, 1);
  const std::array bindings{
      Binding{.Set = 1, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(&view, 1))}},
      Storage(2, 0, *classes),
      Storage(2, 1, *groundPalette),
      Binding{.Set = 3, .Slot = 0, .Value = Uniform{.Bytes = std::as_bytes(std::span(material))}},
      Binding{
          .Set = 3, .Slot = 1, .Value = Uniform{.Bytes = std::as_bytes(std::span(&lighting, 1))}}};
  StageSubmission submission;
  Require(ground.Encode({.Commands = commands.get(),
                         .Submission = submission,
                         .Area = {.Width = kWidth, .Height = kHeight},
                         .Clip = {.Width = kWidth, .Height = kHeight}},
                        bindings));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the actual five-target terrain material pass completes");
  auto depths = Read(device, *depth, 1);
  if (pattern != nullptr) {
    const auto expected = PatternControl(device, *pattern, bindings, attachments);
    CHECK(depths == expected,
          "all patch pixels exactly match independently drawn full-grid triangles");
    CHECK(std::count_if(expected.begin(), expected.end(), [](float value) { return value > 0; }) >
              static_cast<ptrdiff_t>(kPixels / 3),
          "the non-affine control is nonempty");
    return;
  }
  CheckDepth(depths, true);
  const auto base = Read(device, *targets[3], 4);
  CheckTarget({.Colour = base, .Depth = depths},
              kSurface,
              "the native material target preserves the exact procedural fallback palette");
  if (!prefix.empty()) {
    std::ofstream file(prefix + "-base.bin", std::ios::binary);
    file.write(reinterpret_cast<const char *>(base.data()),
               static_cast<std::streamsize>(base.size() * sizeof(float)));
    file.flush();
    CHECK(file, "optional material diagnostic retains actual lossless terrain pixels");
  }
  const auto shaded = Read(device, *targets[0], 4);
  const auto normal = Read(device, *targets[1], 4);
  const auto identity = Read(device, *targets[2], 4);
  CheckMetalRough(device, *targets[4], depths);
  constexpr std::array<float, 4> unlit{0, 0, 0, 0};
  constexpr std::array<float, 4> normalValue{0, 0, 1, 1};
  constexpr std::array<float, 4> identityValue{1, 0, 0, 1};
  CheckTarget({.Colour = shaded, .Depth = depths},
              unlit,
              "material capture retains its unlit radiance target");
  CheckTarget({.Colour = normal, .Depth = depths},
              normalValue,
              "terrain normals and front faces reach their own target");
  CheckTarget({.Colour = identity, .Depth = depths},
              identityValue,
              "terrain identity reaches its own target");
}

inline void NonAffine(Device &device, GroundLattice &ground) {
  HeightPattern pattern;
  for (size_t i = 0; i < pattern.Fractions.size(); ++i) {
    pattern.Fractions[i] =
        std::pow(static_cast<float>(i) / (GroundLattice::kSide - 1), kFractionPower);
  }
  for (size_t j = 0; j < GroundLattice::kPageSide; ++j) {
    for (size_t i = 0; i < GroundLattice::kPageSide; ++i) {
      pattern.Nodes[j * GroundLattice::kPageSide + i] =
          kHeightBase + kHeightAmpE * std::sin(static_cast<float>(i) * kHeightRateE) +
          kHeightAmpN * std::cos(static_cast<float>(j) * kHeightRateN);
    }
  }
  Require(RetryOffline([&] { return ground.SetGrid(pattern.Fractions); }, std::chrono::seconds(5)));
  const PageId page = Require(
      RetryOffline([&] { return ground.PlacePage(pattern.Nodes); }, std::chrono::seconds(5)));
  pattern.Tiles = {Tile(page, {kLeft, -kInner}), Tile(page, {kInner, -kLeft})};
  const std::span real(pattern.Tiles.data(), size_t{1});
  const std::span virtualTiles(pattern.Tiles.data() + 1, size_t{1});
  Require(RetryOffline([&] { return ground.SetInstances(real, virtualTiles); },
                       std::chrono::seconds(5)));
  ground.SetDrawEnabled(true);
  Lit(device, ground, "", &pattern);
}

inline int RunGroundLatticeProbe(const std::string &prefix) {
  auto device = Require(CreateDevice({.Validation = true}));
  GroundLattice ground;
  std::array<ColourTarget, kTargetFormats.size()> targets;
  for (size_t at = 0; at < targets.size(); ++at) { targets[at].Texels = kTargetFormats[at]; }
  const SurfaceOutputs outputs{
      .NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4};
  Require(RetryOffline([&] { return ground.Configure(*device, outputs, targets); },
                       std::chrono::seconds(5)));
  std::vector<float> nodes(GroundLattice::kPageNodes, kSurface[0]);
  const auto left =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  std::ranges::fill(nodes, kSurface[2]);
  auto right =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  for (uint32_t page = 2; page <= GroundLattice::kPagesPerLayer; ++page) {
    right = Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  }
  CHECK(right == GroundLattice::kPagesPerLayer,
        "real texture-array growth preserves the first layer and adds the next");
  auto distant = Tile(left, {kLeft, -kInner});
  distant.Instance.Row[12] = 10;
  const std::array real{Tile(left, {kLeft, -kInner}), distant};
  const std::array virtualTiles{Tile(right, {kInner, -kLeft})};
  Require(RetryOffline([&] { return ground.SetInstances(real, virtualTiles); },
                       std::chrono::seconds(5)));
  Lit(*device, ground, prefix);
  Cast(*device, ground, prefix, true);
  ground.SetDrawEnabled(false);
  Cast(*device, ground, "", false);
  NonAffine(*device, ground);
  return Report();
}

}
#endif
