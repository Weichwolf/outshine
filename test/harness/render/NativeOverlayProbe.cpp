#include "OverlayDraw.h"
#include "StageSubmission.h"
#include "ReadbackTransfer.h"
#include "NativeCommandsProxy.h"
#include "NativeOfflineDevice.h"
#include "NativeDeviceProxy.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <fstream>
#include <ios>
#include <string>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
std::string capturePrefix;

void Export(std::span<const uint8_t> bytes, const char *suffix) {
  if (capturePrefix.empty()) { return; }
  std::ofstream file(capturePrefix + suffix, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  CHECK(file, "the explicitly requested native overlay artifact is saved losslessly");
}

constexpr uint32_t kSide = 32;
constexpr float kQuarter = 0.25f, kThreeQuarters = 0.75f;
constexpr size_t kBytes = size_t{kSide} * kSide * 4;
constexpr std::array<uint8_t, 4> kWhite{255, 255, 255, 255};
constexpr std::array<uint8_t, 4> kBlack{0, 0, 0, 255};
enum class Failure { None, Begin, Texture, Vertex, Transfer, Map, Unmap, Copy, Submit };

struct Fault {
  Failure Next = Failure::None;
  unsigned Skip = 0, Hits = 0;

  bool Reject(Failure at) {
    if (Next != at) { return false; }
    if (Skip > 0) {
      --Skip;
      return false;
    }
    Next = Failure::None;
    ++Hits;
    return true;
  }
};

Error Injected() {
  return {.Kind = ErrorKind::Driver, .Message = "injected native overlay failure"};
}

class Copies final : public NativeCommandsProxy {
public:
  Copies(std::unique_ptr<Commands> commands, Fault &fault)
      : NativeCommandsProxy(std::move(commands)), Fault_(fault) {}

  using NativeCommandsProxy::Copy;

  Result<void> Copy(const Buffer &source, Buffer &destination, BufferCopy region) override {
    if (Fault_.Reject(Failure::Copy)) { return std::unexpected(Injected()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }

  Result<void> Copy(const Buffer &source, Texture &destination, TextureTransfer region) override {
    if (Fault_.Reject(Failure::Copy)) { return std::unexpected(Injected()); }
    return NativeCommandsProxy::Copy(source, destination, region);
  }

private:
  Fault &Fault_;
};

class FaultDevice final : public NativeDeviceProxy {
public:
  using NativeDeviceProxy::NativeDeviceProxy;
  Fault Fail;
  unsigned VertexAllocations = 0;

  Result<std::unique_ptr<Commands>> Begin() override {
    if (Fail.Reject(Failure::Begin)) { return std::unexpected(Injected()); }
    auto commands = Backing().Begin();
    if (!commands) { return std::unexpected(commands.error()); }
    return std::make_unique<Copies>(std::move(*commands), Fail);
  }

  Result<std::unique_ptr<Texture>> CreateTexture(TextureDescription description,
                                                 std::string_view label = {}) override {
    if (Fail.Reject(Failure::Texture)) { return std::unexpected(Injected()); }
    return Backing().CreateTexture(description, label);
  }

  Result<std::unique_ptr<Buffer>> CreateBuffer(BufferDescription description,
                                               std::string_view label = {}) override {
    const bool vertex = description.Placement != Memory::Upload;
    if (Fail.Reject(vertex ? Failure::Vertex : Failure::Transfer)) {
      return std::unexpected(Injected());
    }
    auto made = Backing().CreateBuffer(description, label);
    if (made && vertex) { ++VertexAllocations; }
    return made;
  }

  Result<std::span<std::byte>> MapWrite(Buffer &buffer, ByteRange range) override {
    if (Fail.Reject(Failure::Map)) { return std::unexpected(Injected()); }
    return Backing().MapWrite(buffer, range);
  }

  Result<void> Unmap(Buffer &buffer) override {
    auto unmapped = Backing().Unmap(buffer);
    if (Fail.Reject(Failure::Unmap)) { return std::unexpected(Injected()); }
    return unmapped;
  }

  Result<Completion> Submit(std::unique_ptr<Commands> commands) override {
    if (Fail.Reject(Failure::Submit)) { return std::unexpected(Injected()); }
    return Backing().Submit(static_cast<Copies &>(*commands).Take());
  }
};

OverlayQuad FullQuad() {
  OverlayQuad quad;
  quad.WidthPx = quad.HeightPx = quad.ClipWidthPx = quad.ClipHeightPx = kSide;
  quad.U1 = quad.V1 = 1;
  return quad;
}

std::vector<uint8_t>
Read(Device &device, OverlayPipeline &pipeline, OverlayDraw &overlay, Texture &output) {
  auto commands = Require(device.Begin());
  const std::array targets{ColourAttachment{.Image = {.Image = &output}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = targets, .Depth = {}}));
  StageSubmission submission;
  Require(pipeline.Encode(overlay,
                          {},
                          {.Commands = commands.get(),
                           .Submission = submission,
                           .Area = {.Width = kSide, .Height = kSide},
                           .Clip = {.Width = kSide, .Height = kSide}}));
  Require(commands->EndRenderPass());
  Require(device.Submit(std::move(commands)));
  submission.Commit();
  auto read = Require(ReadbackTransfer::Start(device, output, {.Width = kSide, .Height = kSide}));
  auto bytes = Require(read.Wait(device, std::chrono::seconds(5)));
  CHECK(bytes && bytes->size() == kBytes,
        "actual overlay output has its complete native byte extent");
  if (!bytes || bytes->size() != kBytes) { Failed(); }
  std::vector<uint8_t> result(bytes->size());
  std::memcpy(result.data(), bytes->data(), bytes->size());
  return result;
}

void AtlasFailures(FaultDevice &fault,
                   Device &device,
                   OverlayDraw &overlay,
                   OverlayPipeline &pipeline,
                   Texture &output,
                   const std::vector<uint8_t> &baseline) {
  for (auto point : {Failure::Begin,
                     Failure::Texture,
                     Failure::Transfer,
                     Failure::Map,
                     Failure::Unmap,
                     Failure::Copy,
                     Failure::Submit}) {
    const auto before = fault.Fail.Hits;
    fault.Fail.Next = point;
    const auto failed =
        overlay.SetAtlas({.Device = &fault, .SceneColours = {}}, kBlack.data(), 1, 1);
    CHECK(!failed && !failed.error().Message.empty(),
          "atlas failure returns a concrete native error");
    CHECK(fault.Fail.Hits == before + 1 && fault.Fail.Next == Failure::None,
          "requested native failure is reached");
    CHECK(Read(device, pipeline, overlay, output) == baseline,
          "rejected atlas upload preserves actual resident pixels");
    Require(overlay.SetAtlas({.Device = &fault, .SceneColours = {}}, kBlack.data(), 1, 1));
    CHECK(Read(device, pipeline, overlay, output) != baseline,
          "atlas retry publishes different actual pixels");
    Require(overlay.SetAtlas({.Device = &fault, .SceneColours = {}}, kWhite.data(), 1, 1));
  }
}

void QuadFailures(FaultDevice &fault,
                  Device &device,
                  OverlayDraw &overlay,
                  OverlayPipeline &pipeline,
                  Texture &output,
                  const std::vector<uint8_t> &baseline) {
  auto quad = FullQuad();
  auto black = quad;
  black.Red = black.Green = black.Blue = 0;
  const std::array replacement{black, black};
  const auto allocated = fault.VertexAllocations;
  for (auto point : {Failure::Vertex,
                     Failure::Begin,
                     Failure::Transfer,
                     Failure::Map,
                     Failure::Unmap,
                     Failure::Copy,
                     Failure::Submit}) {
    const auto before = fault.Fail.Hits;
    fault.Fail.Next = point;
    CHECK(!overlay.SetQuads(
              {.Device = &fault, .SceneColours = {}}, replacement.data(), replacement.size()),
          "quad upload failures reject the update");
    CHECK(fault.Fail.Hits == before + 1 && fault.Fail.Next == Failure::None,
          "quad failure point is reached");
    CHECK(Read(device, pipeline, overlay, output) == baseline,
          "failed quad upload preserves geometry and count");
    Require(overlay.SetQuads(
        {.Device = &fault, .SceneColours = {}}, replacement.data(), replacement.size()));
    CHECK(Read(device, pipeline, overlay, output) != baseline,
          "quad retry changes the actual image");
    Require(overlay.SetQuads({.Device = &fault, .SceneColours = {}}, &quad, 1));
  }
  CHECK(fault.VertexAllocations == allocated + 1,
        "all further updates reuse the two warmed geometry buffers");
}

void Combined(FaultDevice &fault,
              Device &device,
              OverlayDraw &overlay,
              OverlayPipeline &pipeline,
              Texture &output,
              const std::vector<uint8_t> &baseline) {
  auto quad = FullQuad();
  auto small = quad;
  small.WidthPx = static_cast<float>(kSide) / 2;
  const OverlayDraw::AtlasPixels black{.Rgba = kBlack.data(), .Width = 1, .Height = 1};
  const OverlayDraw::AtlasPixels white{.Rgba = kWhite.data(), .Width = 1, .Height = 1};
  for (auto point : {Failure::Texture,
                     Failure::Begin,
                     Failure::Transfer,
                     Failure::Map,
                     Failure::Unmap,
                     Failure::Copy,
                     Failure::Submit}) {
    fault.Fail.Next = point;
    fault.Fail.Skip = point == Failure::Copy ? 1 : 0;
    const auto before = fault.Fail.Hits;
    CHECK(!overlay.Replace({.Device = &fault, .SceneColours = {}}, &small, 1, &black),
          "combined upload rejects a failed transaction");
    CHECK(fault.Fail.Hits == before + 1 && fault.Fail.Next == Failure::None,
          "combined failure reaches the requested phase");
    CHECK(Read(device, pipeline, overlay, output) == baseline,
          "combined failure preserves both atlas and geometry");
    Require(overlay.Replace({.Device = &fault, .SceneColours = {}}, &small, 1, &black));
    CHECK(Read(device, pipeline, overlay, output) != baseline,
          "combined retry publishes different pixels");
    Require(overlay.Replace({.Device = &fault, .SceneColours = {}}, &quad, 1, &white));
  }
}

void InvalidUpdates(Device &device,
                    OverlayDraw &overlay,
                    OverlayPipeline &pipeline,
                    Texture &output,
                    const std::vector<uint8_t> &baseline) {
  const outshine::Render::Gpu gpu{.Device = &device, .SceneColours = {}};
  auto quad = FullQuad();
  CHECK(!overlay.SetQuads(gpu, nullptr, 1), "nonempty null quad input is rejected");
  CHECK(!overlay.SetQuads(gpu, &quad, kMaxOverlayQuads + 1),
        "oversized quad input is rejected before reading");
  for (const auto dimensions :
       {std::array{0, 1},
        std::array{1, -1},
        std::array{65536, 65536},
        std::array{std::numeric_limits<int>::max(), std::numeric_limits<int>::max()}}) {
    CHECK(!overlay.SetAtlas(gpu, kWhite.data(), dimensions[0], dimensions[1]),
          "invalid atlas dimensions fail before reading texels");
    CHECK(Read(device, pipeline, overlay, output) == baseline,
          "invalid dimensions preserve actual old pixels");
  }
  CHECK(!overlay.SetAtlas(gpu, nullptr, 1, 1), "null atlas pixels are rejected");
  CHECK(Read(device, pipeline, overlay, output) == baseline,
        "invalid updates preserve the published pair");
  Require(overlay.SetQuads(gpu, nullptr, 0));
  CHECK(Read(device, pipeline, overlay, output) != baseline,
        "empty overlay removes the resident draw");
  Require(overlay.SetQuads(gpu, &quad, 1));
}

void Appearance(Device &device, Sampler &sampler, Format format) {
  auto output =
      Require(device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Texels = format,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  OverlayDraw overlay;
  const outshine::Render::Gpu gpu{.Device = &device, .SceneColours = {}};
  auto quad = FullQuad();
  quad.Red = 0.5f;
  quad.Green = kQuarter;
  quad.Blue = kThreeQuarters;
  Require(overlay.SetQuads(gpu, &quad, 1));
  OverlayPipeline pipeline;
  Require(pipeline.Configure(gpu, &sampler, format));
  pipeline.Bind({.WidthPx = kSide, .HeightPx = kSide});
  CHECK(!pipeline.Configure(gpu, nullptr, format),
        "invalid reconfiguration preserves the native pipeline");
  const auto codes = Read(device, pipeline, overlay, *output);
  Export(codes, format == Format::Rgba8Srgb ? "-srgb.bin" : "-unorm.bin");
  for (size_t at = 0; at < codes.size(); at += 4) {
    CHECK(std::abs(int(codes[at]) - 128) <= 1 && std::abs(int(codes[at + 1]) - 64) <= 1 &&
              std::abs(int(codes[at + 2]) - 191) <= 1 && codes[at + 3] == 255,
          "both UNORM and sRGB targets retain authored overlay code colours");
  }
  quad.Red = 1;
  quad.Green = quad.Blue = 0;
  quad.Alpha = 0.5f;
  quad.Opacity = 0.5f;
  quad.RadiusPx = 8;
  quad.ClipLeftPx = 4;
  quad.ClipWidthPx = kSide - 2 * quad.ClipLeftPx;
  Require(overlay.SetQuads(gpu, &quad, 1));
  const auto shaped = Read(device, pipeline, overlay, *output);
  Export(shaped, format == Format::Rgba8Srgb ? "-rounded-srgb.bin" : "-rounded-unorm.bin");
  const size_t centre = (size_t{kSide} * (kSide / 2) + kSide / 2) * 4;
  const int expected = format == Format::Rgba8Srgb ? 137 : 64;
  CHECK(std::abs(int(shaped[centre]) - expected) <= 1 &&
            std::abs(int(shaped[centre + 3]) - 64) <= 1,
        "premultiplied opacity blends correctly through the target transfer function");
  CHECK(shaped[0] == 0 && shaped[3] == 0 && shaped[(kSide * (kSide / 2) + 1) * 4 + 3] == 0,
        "rounded corners and clipping leave the clear target transparent");
}

void Transactions(Device &device, Sampler &sampler) {
  FaultDevice fault(device);
  auto output =
      Require(device.CreateTexture({.Width = kSide,
                                    .Height = kSide,
                                    .Texels = Format::Rgba8Unorm,
                                    .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
  OverlayPipeline pipeline;
  Require(
      pipeline.Configure({.Device = &device, .SceneColours = {}}, &sampler, Format::Rgba8Unorm));
  pipeline.Bind({.WidthPx = kSide, .HeightPx = kSide});
  OverlayDraw overlay;
  auto quad = FullQuad();
  Require(overlay.Replace({.Device = &fault, .SceneColours = {}}, &quad, 1, nullptr));
  const auto baseline = Read(device, pipeline, overlay, *output);
  CHECK(baseline[0] == 255 && baseline[3] == 255,
        "the default atlas produces actual opaque white pixels");
  AtlasFailures(fault, device, overlay, pipeline, *output, baseline);
  QuadFailures(fault, device, overlay, pipeline, *output, baseline);
  Combined(fault, device, overlay, pipeline, *output, baseline);
  // Invalid updates must use the same device identity as the published pair.
  InvalidUpdates(fault, overlay, pipeline, *output, baseline);
}
}

int main(int argc, char **argv) {
  if (argc == 2) { capturePrefix = argv[1]; }
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  auto sampler = Require(device.CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  Transactions(device, *sampler);
  Appearance(device, *sampler, Format::Rgba8Unorm);
  Appearance(device, *sampler, Format::Rgba8Srgb);
  return Report();
}
