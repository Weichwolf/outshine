#include "CloudViewProbe.h"
#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

namespace outshine::Test::CloudViewProbe {
namespace {
using namespace Gpu;
using namespace MediumViewProbe;
constexpr uint32_t kLutRowBytes = 256;
constexpr std::array<uint16_t, 4> kSolarHalf{0x3a00, 0x3800, 0x3400, 0x3c00};
constexpr std::array<uint16_t, 4> kPsiHalf{0x2400, 0x2800, 0x2c00, 0x3c00};
constexpr std::array<float, 3> kAlongM{500.0f, 1600.0f, 4500.0f};
constexpr std::array<Pixel, 3> kLit{Pixel{0.8f, 0.2f, 0.1f, 0.25f},
                                    Pixel{0.2f, 0.7f, 0.9f, 0.5f},
                                    Pixel{0.9f, 0.85f, 0.25f, 0.75f}};

Scissor Band(Viewport area, size_t index) {
  constexpr std::array<float, 3> left{0.03f, 0.36f, 0.70f};
  constexpr std::array<float, 3> right{0.28f, 0.62f, 0.97f};
  const auto x = static_cast<uint32_t>(std::ceil(area.X + area.Width * left[index] - 0.5f));
  const auto y = static_cast<uint32_t>(std::ceil(area.Y + area.Height * 0.08f - 0.5f));
  const auto stopX = static_cast<uint32_t>(std::ceil(area.X + area.Width * right[index] - 0.5f));
  const auto stopY = static_cast<uint32_t>(std::ceil(area.Y + area.Height * 0.92f - 0.5f));
  return {.X = x, .Y = y, .Width = stopX - x, .Height = stopY - y};
}
}

Targets::Targets(Device &device, ImageShape shape) : Shape_(shape) {
  Inputs_[0] = Require(device.CreateTexture(
      {.Width = Shape_.Width,
       .Height = Shape_.Height,
       .Texels = Format::Rgba32Float,
       .Use = TextureUse::ColourAttachment | TextureUse::Sampled | TextureUse::CopySource}));
  Inputs_[1] =
      Require(device.CreateTexture({.Width = Shape_.Width,
                                    .Height = Shape_.Height,
                                    .Texels = Format::Depth32Float,
                                    .Use = TextureUse::DepthAttachment | TextureUse::Sampled}));
  for (size_t index = 2; index < Inputs_.size(); ++index) {
    Inputs_[index] =
        Require(device.CreateTexture({.Width = 2,
                                      .Height = 2,
                                      .Texels = Format::Rgba16Float,
                                      .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  }
  for (auto &output : Outputs_) {
    output = Require(
        device.CreateTexture({.Width = Shape_.Width,
                              .Height = Shape_.Height,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  }
  for (auto &readback : Readbacks_) {
    readback = Require(device.CreateBuffer({.Bytes = uint64_t{Shape_.RowBytes} * Shape_.Height,
                                            .Use = BufferUse::CopyDestination,
                                            .Placement = Memory::Readback}));
  }
  Exact_ = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  Linear_ = Require(device.CreateSampler({.Mip = Filter::Nearest}));
  auto commands = Require(device.Begin());
  for (size_t index = 2; index < Inputs_.size(); ++index) {
    UploadImage(device,
                *commands,
                *Inputs_[index],
                ImageBytes<std::array<uint16_t, 4>>(
                    {.Width = 2, .Height = 2, .RowBytes = kLutRowBytes},
                    [index](uint32_t, uint32_t) { return index == 2 ? kPsiHalf : kSolarHalf; }),
                kLutRowBytes);
  }
  Finish(device, std::move(commands));
}

void Targets::Scene(Commands &commands,
                    const GraphicsPipeline &pipeline,
                    const Case &scenario) const {
  const std::array colours{
      ColourAttachment{.Image = {.Image = Inputs_[0].get()},
                       .Begin = Load::Clear,
                       .Clear = scenario.BoundarySky ? scenario.Boundary : kCloudViewBoundary}};
  Require(commands.BeginRenderPass(
      {.Colours = colours,
       .Depth = DepthAttachment{.Image = {.Image = Inputs_[1].get()}, .Begin = Load::Clear}}));
  const auto projection = scenario.View.Projection.Projection();
  for (size_t index = 0; !scenario.SkyOnly && index < kAlongM.size(); ++index) {
    const float distanceM = kAlongM[index];
    const Vec4f clip{{(projection[14] - projection[10] * distanceM) /
                          (projection[15] - projection[11] * distanceM),
                      0.0f,
                      0.0f,
                      0.0f}};
    const std::array bindings{
        Binding{.Set = 1, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&clip, 1))}},
        Binding{.Set = 3, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&kLit[index], 1))}}};
    Require(commands.Draw({.State = {.Pipeline = &pipeline,
                                     .Bindings = bindings,
                                     .Vertices = {},
                                     .Area = scenario.View.Area,
                                     .Clip = Band(scenario.View.Area, index)},
                           .Range = {.Count = 3}}));
  }
  Require(commands.EndRenderPass());
}

Capture Targets::Draw(Device &device, const Pipelines &pipelines, const Case &scenario) const {
  auto frame = FrameFor(scenario.View, ControlledAir());
  auto weather = PrepareWeather(scenario);
  const auto recordedWeather = weather;
  auto commands = Require(device.Begin());
  Scene(*commands, pipelines.Scene, scenario);
  std::array<Binding, 6> bindings;
  for (uint32_t slot = 0; slot < Inputs_.size(); ++slot) {
    bindings[slot] = {.Set = 2,
                      .Slot = slot,
                      .Value = SampledTexture{.Image = {.Image = Inputs_[slot].get()},
                                              .Sampling = slot < 2 ? Exact_.get() : Linear_.get()}};
  }
  bindings[4] = {.Set = 3, .Slot = 2, .Value = Uniform{std::as_bytes(std::span(&frame, 1))}};
  bindings[5] = {.Set = 3, .Slot = 3, .Value = Uniform{std::as_bytes(std::span(&weather, 1))}};
  std::array<ColourAttachment, 4> colours;
  for (size_t index = 0; index < colours.size(); ++index) {
    colours[index] = {.Image = {.Image = Outputs_[index].get()}, .Begin = Load::Clear};
  }
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = &pipelines.Front,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = scenario.View.Area,
                                    .Clip = ViewClip(scenario.View)},
                          .Range = {.Count = 3}}));
  frame.PixelToNdc = {};
  weather.DensityCameraOriginM = {};
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *Inputs_[0],
      *Readbacks_[0],
      {.RowBytes = Shape_.RowBytes, .Region = {.Width = Shape_.Width, .Height = Shape_.Height}}));
  for (size_t index = 0; index < Outputs_.size(); ++index) {
    Require(commands->Copy(
        *Outputs_[index],
        *Readbacks_[index + 1],
        {.RowBytes = Shape_.RowBytes, .Region = {.Width = Shape_.Width, .Height = Shape_.Height}}));
  }
  Finish(device, std::move(commands));
  Capture result{.Images = {}, .Weather = recordedWeather};
  for (size_t index = 0; index < result.Images.size(); ++index) {
    result.Images[index] = ReadImage(device, *Readbacks_[index], Shape_);
  }
  return result;
}
}
