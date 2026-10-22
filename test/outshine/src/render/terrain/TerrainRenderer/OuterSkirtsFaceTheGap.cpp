#include "test/harness/render/NativeTerrainRendererProbe.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOffline.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine::Test::GroundProbe;
  constexpr float high = 0.6f;
  constexpr float low = 0.2f;
  constexpr float eyeHeight = 0.4f;
  constexpr float nearM = 0.1f;
  constexpr float eyeEast = 2;
  auto device = Require(CreateDevice({.Validation = true}));
  TerrainRenderer ground;
  std::array<ColourTarget, kTargetFormats.size()> formats;
  for (size_t i = 0; i < formats.size(); ++i) { formats[i].Texels = kTargetFormats[i]; }
  Require(RetryOffline(
      [&] {
        return ground.Configure(
            *device,
            {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
            formats);
      },
      std::chrono::seconds(5)));
  std::vector<float> nodes(TerrainRenderer::kPageNodes, high);
  const auto highPage =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  std::ranges::fill(nodes, low);
  const auto lowPage =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));

  struct TileExtent {
    float West;
    float East;
    float Height;
  };

  const auto tile = [](PageId page, TileExtent extent) {
    GroundTile result;
    result.Instance.Page = static_cast<float>(page);
    result.Instance.Corners = {
        extent.West, 0.5f, extent.East, 0.5f, extent.West, -0.5f, extent.East, -0.5f};
    result.Instance.StepE = result.Instance.StepN = 1.0f / (TerrainRenderer::kSide - 1);
    result.LowM = extent.Height - 0.5f;
    result.HighM = extent.Height;
    return result;
  };
  const std::array real{tile(highPage, {.West = -1, .East = 0, .Height = high}),
                        tile(lowPage, {.West = 0, .East = 1, .Height = low})};
  Require(RetryOffline([&] { return ground.SetInstances(real, {}); }, std::chrono::seconds(5)));
  std::array<std::unique_ptr<Texture>, kTargetFormats.size()> targets;
  std::array<ColourAttachment, kTargetFormats.size()> attachments;
  for (size_t i = 0; i < targets.size(); ++i) {
    targets[i] = Target(*device, kTargetFormats[i], false);
    attachments[i] = {.Image = {.Image = targets[i].get()}, .Begin = Load::Clear};
  }
  auto depth = Target(*device, Format::Depth32Float, true);
  auto commands = Require(RetryOffline([&] { return device->Begin(); }, std::chrono::seconds(5)));
  std::array<uint32_t, 3> classWords{0, 0, ~0u};
  std::array<float, kPaletteFloats> palette{};
  palette[0] = std::bit_cast<float>(1u);
  palette[2] = 1;
  palette[12] = palette[13] = kNoRockSlope;
  const auto classes = Upload(*device, *commands, classWords);
  const auto colours =
      Upload(*device,
             *commands,
             std::span(reinterpret_cast<const uint32_t *>(palette.data()), palette.size()));
  FrameContext frame;
  frame.Mvp = {{0, 0, 0, -1, 1, 0, 0, 0, 0, 2, 0, 0, 0, -2 * eyeHeight, nearM, eyeEast}};
  Require(ground.Cull(frame, {}, *commands));
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
      Storage(2, 1, *colours),
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
  const auto done = Require(device->Submit(std::move(commands)));
  CHECK(Require(device->Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "terrain gap depth completes");
  const auto pixels = Read(*device, *depth, 1);
  const float expected = nearM / eyeEast;
  CHECK_NEAR(pixels[kHeight / 2 * kWidth + kWidth / 2],
             expected,
             kDepthTolerance,
             "reverse depth",
             "the near outer wall closes the terrain gap");
  return Report();
}
