#include "Check.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "test/harness/render/NativeOfflineDevice.h"
#include "ReadbackTransfer.h"
#include "StageSubmission.h"
#include "SurfaceReprojectionStage.h"

#include <chrono>
#include <memory>
#include <fstream>
#include <ios>
#include <string>
#include <span>
#include <cstddef>
#include <utility>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numbers>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
using namespace outshine::Render::Native;
using namespace outshine::Test::Gpu;
std::string capturePrefix;

void Export(std::span<const std::byte> bytes, const char *suffix) {
  if (capturePrefix.empty()) { return; }
  std::ofstream file(capturePrefix + suffix, std::ios::binary);
  file.write(reinterpret_cast<const char *>(bytes.data()),
             static_cast<std::streamsize>(bytes.size()));
  CHECK(file, "the explicitly requested reprojection artifact is saved losslessly");
}

constexpr uint32_t kSide = 17;
constexpr size_t kPixels = size_t{kSide} * kSide;
constexpr float kQuarter = 0.25f, kThreeQuarters = 0.75f, kEighth = 0.125f;
constexpr float kDepthTolerance = 1e-6f;
constexpr double kDepthToleranceDouble = 1e-6;
constexpr double kMovementM = 0.001;
constexpr float kRightAngleDeg = 90;
constexpr size_t kCentre = kSide * (kSide / 2) + kSide / 2;

std::unique_ptr<Texture> Image(Device *device, Format format, UseMask<TextureUse> usage) {
  return Require(device->CreateTexture(
      {.Width = kSide, .Height = kSide, .Texels = format, .Use = usage | TextureUse::CopySource}));
}

std::unique_ptr<Texture> Upload(Device *device, const std::array<float, 4> &value, bool hole) {
  auto image =
      Image(device, Format::Rgba32Float, TextureUse::Sampled | TextureUse::CopyDestination);
  std::vector<float> samples(kPixels * 4);
  for (size_t at = 0; at < samples.size() / 4; ++at) {
    std::ranges::copy(value, samples.begin() + static_cast<ptrdiff_t>(at * 4));
  }
  if (hole) { samples[0] = 0; }
  const auto bytes = std::as_bytes(std::span(samples));
  auto staging = Require(device->CreateBuffer(
      {.Bytes = bytes.size(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  auto mapped = Require(device->MapWrite(*staging, {.Bytes = bytes.size()}));
  std::memcpy(mapped.data(), bytes.data(), bytes.size());
  Require(device->Unmap(*staging));
  auto commands = Require(device->Begin());
  Require(commands->Copy(*staging,
                         *image,
                         {.RowBytes = uint64_t{kSide} * 4 * sizeof(float),
                          .Region = {.Width = kSide, .Height = kSide}}));
  Require(device->Submit(std::move(commands)));
  return image;
}

struct Output {
  std::array<std::unique_ptr<Texture>, 5> Channels;
  std::unique_ptr<Texture> Depth;
};

Output Targets(Device *device) {
  Output out;
  constexpr std::array formats{Format::Rgba16Float,
                               Format::Rgba16Float,
                               Format::Rgba32Float,
                               Format::Rgba16Float,
                               Format::Rg16Float};
  for (size_t at = 0; at < formats.size(); ++at) {
    out.Channels[at] = Image(device, formats[at], TextureUse::ColourAttachment);
  }
  out.Depth = Image(device, Format::Depth32Float, TextureUse::DepthAttachment);
  return out;
}

std::vector<std::byte> Read(Device *device, Texture *image) {
  auto read = Require(ReadbackTransfer::Start(*device, *image, {.Width = kSide, .Height = kSide}));
  auto bytes = Require(read.Wait(*device, std::chrono::seconds(5)));
  CHECK(bytes, "actual native channel reads back within the offline budget");
  if (!bytes) { Failed(); }
  return {bytes->begin(), bytes->end()};
}

std::vector<float> Depths(Device *device, Texture *image) {
  const auto bytes = Read(device, image);
  std::vector<float> values(kPixels);
  CHECK(bytes.size() == values.size() * sizeof(float), "depth retains its exact float format");
  if (bytes.size() == values.size() * sizeof(float)) {
    std::memcpy(values.data(), bytes.data(), bytes.size());
  }
  return values;
}

void Materials(Device *device, const Output &out) {
  const auto identity = Read(device, out.Channels[2].get());
  CHECK(identity.size() == kPixels * 16, "reprojected identity reads back");
  std::array<float, 4> id{};
  if (identity.size() == kPixels * 16) {
    std::memcpy(id.data(), identity.data() + kCentre * 16, 16);
  }
  CHECK(id[0] == 7 && id[3] == 1, "reprojection retains the original material identity");
  constexpr std::array<std::array<uint16_t, 4>, 4> expected{{{{0x3400, 0x3800, 0x3a00, 0x3c00}},
                                                             {{0, 0, 0x3c00, 0x3c00}},
                                                             {{0x3400, 0x3800, 0x3a00, 0x3c00}},
                                                             {{0x3000, 0x3800, 0, 0}}}};
  constexpr std::array<size_t, 4> channels{0, 1, 3, 4};
  for (size_t at = 0; at < channels.size(); ++at) {
    const uint32_t bytes = at == 3 ? 4u : 8u;
    const auto channel = Read(device, out.Channels[channels[at]].get());
    CHECK(channel.size() == kPixels * bytes, "reprojected native channel reads back");
    std::array<uint16_t, 4> actual{};
    if (channel.size() == kPixels * bytes) {
      std::memcpy(actual.data(), channel.data() + kCentre * bytes, bytes);
    }
    CHECK(
        actual == expected[at],
        "HDR stays untouched and normal, base colour and metallic roughness retain native values");
  }
}

bool Draw(Device *device,
          const SurfaceReprojectionStage &stage,
          Output &out,
          const CameraBasis &camera,
          const Lens &lens,
          float occluder = 0) {
  auto commands = Require(device->Begin());
  std::array<ColourAttachment, 5> targets{};
  for (size_t at = 0; at < targets.size(); ++at) {
    targets[at] = {.Image = {.Image = out.Channels[at].get()}, .Begin = Load::Clear};
  }
  targets[0].Clear = {kQuarter, 0.5f, kThreeQuarters, 1};
  Require(commands->BeginRenderPass({.Colours = targets,
                                     .Depth = DepthAttachment{.Image = {.Image = out.Depth.get()},
                                                              .Begin = Load::Clear,
                                                              .Clear = occluder}}));
  StageSubmission submission;
  const auto drawn = stage.Encode(camera,
                                  lens,
                                  {.Commands = commands.get(),
                                   .Submission = submission,
                                   .Area = {.Width = kSide, .Height = kSide},
                                   .Clip = {.Width = kSide, .Height = kSide}});
  Require(commands->EndRenderPass());
  if (!drawn) { return false; }
  Require(device->Submit(std::move(commands)));
  submission.Commit();
  return true;
}

void Probe(Device *device) {
  std::array<std::unique_ptr<Texture>, 5> inputs{
      Upload(device, {{0.5f, 0, 0, 0}}, true),
      Upload(device, {{0, 0, 1, 1}}, false),
      Upload(device, {{7, 0, 0, 1}}, false),
      Upload(device, {{kQuarter, 0.5f, kThreeQuarters, 1}}, false),
      Upload(device, {{kEighth, 0.5f, 0, 0}}, false)};
  CHECK(std::ranges::all_of(inputs, [](const auto &image) { return static_cast<bool>(image); }),
        "captured material and depth channels upload");
  auto out = Targets(device);
  auto sampler = Require(device->CreateSampler(
      {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
  outshine::Render::Gpu gpu{.Device = device,
                            .HdrFormat = Format::Rgba16Float,
                            .Width = kSide,
                            .Height = kSide,
                            .SceneColours = {}};
  for (auto resource : {Resource::SceneHdr,
                        Resource::SceneShadingNormal,
                        Resource::SceneSurfaceIdentity,
                        Resource::SceneSurfaceBase,
                        Resource::SceneSurfaceMetalRough}) {
    CHECK(gpu.SceneColours.Add(resource), "capture targets retain native binding order");
  }
  const Lens lens{.WidePx = kSide, .HighPx = kSide, .FovDeg = kRightAngleDeg, .NearM = 1};
  SurfaceReprojectionStage::Capture capture{.Camera = {}, .Projection = lens};
  for (size_t at = 0; at < inputs.size(); ++at) {
    capture.DepthNormalIdentityBaseMetalRough[at] = inputs[at].get();
  }
  SurfaceReprojectionStage stage;
  CHECK(stage.Configure(gpu, capture, sampler.get()).has_value(),
        "real reprojection shaders configure");
  CHECK(Draw(device, stage, out, {}, lens), "unchanged view replays the capture");
  auto depths = Depths(device, out.Depth.get());
  CHECK(depths.size() == kPixels && depths[0] == 0 &&
            std::abs(depths[kCentre] - 0.5f) < kDepthTolerance,
        "uncovered samples stay empty and covered samples retain geometric depth");
  Materials(device, out);
  Export(std::as_bytes(std::span(depths)), "-depth.bin");
  const auto base = Read(device, out.Channels[3].get());
  Export(base, "-base.bin");
  capturePrefix.clear();
  CHECK(Draw(device, stage, out, {}, lens, kThreeQuarters),
        "existing nearer depth participates in replay");
  depths = Depths(device, out.Depth.get());
  CHECK(depths.size() == kPixels && depths[kCentre] == kThreeQuarters,
        "captured surfaces cannot overwrite nearer native geometry");
  const double angle = 20 * std::numbers::pi / 180;
  auto turn = Viewpoint::LookAt({.EyeM = {}, .AimM = {{std::sin(angle), 0, -std::cos(angle)}}}, 0);
  CHECK(turn && Draw(device, stage, out, *turn, lens), "rotation reuses the same capture position");
  depths = Depths(device, out.Depth.get());
  CHECK(depths.size() == kPixels &&
            std::abs(depths[kCentre] - 0.5 * std::cos(angle)) < kDepthToleranceDouble,
        "reprojection changes depth to the independently calculated plane intersection");
  Materials(device, out);
  Lens nearer = lens;
  nearer.NearM = kQuarter;
  CHECK(Draw(device, stage, out, {}, nearer),
        "a changed near plane uses the current depth convention");
  depths = Depths(device, out.Depth.get());
  CHECK(depths.size() == kPixels && std::abs(depths[kCentre] - kEighth) < kDepthTolerance,
        "depth is transformed rather than copied between projections");
  CameraBasis opposed;
  opposed.Forward = {{0, 0, 1}};
  opposed.Right = {{-1, 0, 0}};
  CHECK(Draw(device, stage, out, opposed, lens),
        "the opposing camera remains a legal replay request");
  depths = Depths(device, out.Depth.get());
  CHECK(depths.size() == kPixels &&
            std::ranges::all_of(depths, [](float depth) { return depth == 0; }),
        "a capture behind the camera cannot fill the opposite hemisphere");
  CameraBasis moved;
  moved.EyeM[0] = kMovementM;
  CHECK(!Draw(device, stage, out, moved, lens), "movement explicitly retains native geometry");
  Lens invalid = lens;
  invalid.FovDeg = std::numeric_limits<float>::quiet_NaN();
  CHECK(!Draw(device, stage, out, {}, invalid),
        "nonfinite projections are rejected before lens evaluation");
  invalid = lens;
  invalid.OrthoM = 2;
  CHECK(!Draw(device, stage, out, {}, invalid),
        "orthographic sources need a different reprojection contract");
  auto incomplete = capture;
  incomplete.DepthNormalIdentityBaseMetalRough[0] = nullptr;
  CHECK(!stage.Configure(gpu, incomplete, sampler.get()),
        "incomplete capture channels are rejected");
  CHECK(Draw(device, stage, out, {}, lens),
        "a rejected replacement preserves the last complete capture");
}
}

int main(int argc, char **argv) {
  if (argc == 2) { capturePrefix = argv[1]; }
  auto actual = Require(CreateDevice({.Validation = true}));
  NativeOfflineDevice device(*actual);
  Probe(&device);
  return Report();
}
