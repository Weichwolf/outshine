#include "src/render/stages/StagePipelines.h"
#include "src/render/stages/PresentStage.h"
#include "src/render/stages/StageSubmission.h"
#include "src/render/device/SpirvFile.h"
#include "NativeGpuCheck.h"
#include "NativeDeviceProxy.h"
#include "NativeSdr.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kCodes = 256;
constexpr uint32_t kChannels = 4;
constexpr std::array kTargets{
    Format::Bgra8Srgb, Format::Rgba8Srgb, Format::Bgra8Unorm, Format::Rgba8Unorm};
using Pixels = std::array<uint8_t, size_t{kCodes} * kChannels>;

Pixels Input() {
  Pixels result{};
  for (size_t at = 0; at < kCodes; ++at) {
    result[at * kChannels] = static_cast<uint8_t>(at);
    result[at * kChannels + 1] = static_cast<uint8_t>(kCodes - 1 - at);
    result[at * kChannels + 2] = 128;
    result[at * kChannels + 3] = static_cast<uint8_t>(at);
  }
  return result;
}

void Complete(Device &device, Completion completion) {
  CHECK(Require(device.Wait(completion, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the real colour-transfer workload completes within its offline wait budget");
}

std::unique_ptr<Texture> UploadImage(Device &device, const Pixels &pixels) {
  auto image =
      Require(device.CreateTexture({.Width = kCodes,
                                    .Height = 1,
                                    .Texels = Format::Rgba8Srgb,
                                    .Use = TextureUse::Sampled | TextureUse::CopyDestination}));
  auto upload = Require(device.CreateBuffer(
      {.Bytes = sizeof(pixels), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto mapped = Require(device.MapWrite(*upload, {.Bytes = sizeof(pixels)}));
  std::memcpy(mapped.data(), pixels.data(), sizeof(pixels));
  Require(device.Unmap(*upload));
  auto commands = Require(device.Begin());
  Require(commands->Copy(
      *upload, *image, {.RowBytes = sizeof(pixels), .Region = {.Width = kCodes, .Height = 1}}));
  Complete(device, Require(device.Submit(std::move(commands))));
  return image;
}

Pixels Render(Device &device,
              const Texture &source,
              const Sampler &sampler,
              Format format,
              const GraphicsPipeline *pipeline = nullptr) {
  PresentStage present;
  StageSubmission submission;
  if (pipeline == nullptr) {
    Require(present.Configure(&source, &sampler));
    Require(present.For(device, format));
    const auto unsupported = present.For(device, Format::Rgba16Float);
    CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
          "an unsupported target does not replace the accepted SDR pipeline");
    const auto absent = present.Configure(nullptr, &sampler);
    CHECK(!absent && absent.error().Kind == ErrorKind::Invalid,
          "a rejected source does not replace the accepted presentation feeds");
  }
  auto target =
      Require(device.CreateTexture({.Width = kCodes,
                                    .Height = 1,
                                    .Texels = format,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  auto readback = Require(device.CreateBuffer(
      {.Bytes = sizeof(Pixels), .Use = BufferUse::CopyDestination, .Placement = Memory::Readback}));
  auto commands = Require(device.Begin());
  const std::array colours{
      ColourAttachment{.Image = {.Image = target.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
  const Viewport area{.Width = static_cast<float>(kCodes), .Height = 1};
  const Scissor clip{.Width = kCodes, .Height = 1};
  if (pipeline == nullptr) {
    Require(present.Encode(
        {.Commands = commands.get(), .Submission = submission, .Area = area, .Clip = clip}));
  } else {
    const std::array bindings{
        Binding{.Set = 2,
                .Slot = 0,
                .Value = SampledTexture{.Image = {.Image = &source}, .Sampling = &sampler}}};
    Require(commands->Draw({.State = {.Pipeline = pipeline,
                                      .Bindings = bindings,
                                      .Vertices = {},
                                      .Area = area,
                                      .Clip = clip},
                            .Range = {.Count = 3}}));
  }
  Require(commands->EndRenderPass());
  Require(commands->Copy(
      *target, *readback, {.RowBytes = sizeof(Pixels), .Region = {.Width = kCodes, .Height = 1}}));
  Complete(device, Require(device.Submit(std::move(commands))));
  auto mapped = Require(device.MapRead(*readback, {.Bytes = sizeof(Pixels)}));
  Pixels result{};
  std::memcpy(result.data(), mapped.data(), sizeof(result));
  Require(device.Unmap(*readback));
  return result;
}

class PipelineCounter final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  size_t Made = 0;
  bool Reject = false;

  Result<std::unique_ptr<GraphicsPipeline>>
  CreateGraphicsPipeline(const GraphicsDescription &description) override {
    ++Made;
    if (Reject) {
      return std::unexpected(
          Error{.Kind = ErrorKind::Driver,
                .Message = "pipeline creation forbidden during cached presentation"});
    }
    return Backing().CreateGraphicsPipeline(description);
  }
};

void CachedVariants(Device &device, Texture &image, Sampler &sampler) {
  PipelineCounter observed(device);
  PresentStage stage;
  Require(stage.Configure(&image, &sampler));
  for (const auto format : kTargets) { Require(stage.For(observed, format)); }
  const auto built = observed.Made;
  CHECK(built == kTargets.size(), "all actual SDR variants are prepared before frame recording");
  observed.Reject = true;
  for (const auto format : kTargets) { Require(stage.For(observed, format)); }
  CHECK(observed.Made == built,
        "target switches reuse every prepared native presentation pipeline");
}

std::unique_ptr<GraphicsPipeline> MissingOetf(Device &device) {
  auto vertex = ReadSpirv("build/shaders/fullscreen.vert.spv");
  auto fragment = ReadSpirv("build/shaders/present.frag.spv");
  CHECK(vertex && fragment,
        "the original unencoded present shader is available for the negative control");
  if (!vertex || !fragment) { Failed(); }
  const std::array colours{ColourTarget{.Texels = Format::Bgra8Unorm, .Rgb = {}, .Alpha = {}}};
  return Require(device.CreateGraphicsPipeline({.Vertex = {.Spirv = *vertex},
                                                .Fragment = ShaderCode{.Spirv = *fragment},
                                                .Streams = {},
                                                .Attributes = {},
                                                .Colours = colours,
                                                .Depth = {},
                                                .Culling = Cull::None,
                                                .Label = {}}));
}

}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  const auto pixels = Input();
  auto image = UploadImage(*device, pixels);
  auto sampler = Require(device->CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest, .MaxLod = 0}));
  CachedVariants(*device, *image, *sampler);
  for (const auto format : kTargets) {
    const auto result = Render(*device, *image, *sampler, format);
    for (size_t at = 0; at < kCodes; ++at) {
      for (size_t channel = 0; channel < kChannels; ++channel) {
        const auto expectedChannel = IsBgra(format) && channel < 3 ? 2 - channel : channel;
        const int delta = static_cast<int>(result[at * kChannels + channel]) -
                          pixels[at * kChannels + expectedChannel];
        CHECK(delta >= -1 && delta <= 1,
              "every display code roundtrips once; alpha remains linear");
      }
    }
  }
  auto wrong = MissingOetf(*device);
  const auto bad = Render(*device, *image, *sampler, Format::Bgra8Unorm, wrong.get());
  CHECK(bad[size_t{128} * kChannels + 2] < pixels[size_t{128} * kChannels],
        "the unchanged old UNORM copy is independently too dark");
  const auto unsupported = CreatePresentPipeline(*device, Format::Rgba16Float);
  CHECK(!unsupported && unsupported.error().Kind == ErrorKind::Unsupported,
        "SDR transfer cannot silently encode an HDR intermediate");
  return Report();
}
