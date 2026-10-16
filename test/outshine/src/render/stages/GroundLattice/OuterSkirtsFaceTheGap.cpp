#include "test/harness/render/NativeGroundLatticeProbe.h"
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

int main() {
  using namespace outshine::Test::GroundProbe;
  constexpr float high = 0.6f, low = 0.2f, eyeHeight = 0.4f, nearM = 0.1f, eyeEast = 2;
  auto device = Require(CreateDevice({.Validation = true}));
  GroundLattice ground;
  std::array<ColourTarget, kTargetFormats.size()> formats;
  for (size_t i = 0; i < formats.size(); ++i) { formats[i].Texels = kTargetFormats[i]; }
  Require(
      ground.Configure(*device,
                       {.NormalIndex = 1, .IdentityIndex = 2, .BaseIndex = 3, .MetalRoughIndex = 4},
                       formats));
  std::vector<float> nodes(GroundLattice::kPageNodes, high);
  const auto highPage =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  std::ranges::fill(nodes, low);
  const auto lowPage =
      Require(RetryOffline([&] { return ground.PlacePage(nodes); }, std::chrono::seconds(5)));
  const auto tile = [](PageId page, float west, float east, float height) {
    GroundTile result;
    result.Instance.Page = static_cast<float>(page);
    result.Instance.Corners = {west, 0.5f, east, 0.5f, west, -0.5f, east, -0.5f};
    result.Instance.StepE = result.Instance.StepN = 1.0f / (GroundLattice::kSide - 1);
    result.LowM = height - 0.5f;
    result.HighM = height;
    return result;
  };
  const std::array real{tile(highPage, -1, 0, high), tile(lowPage, 0, 1, low)};
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
