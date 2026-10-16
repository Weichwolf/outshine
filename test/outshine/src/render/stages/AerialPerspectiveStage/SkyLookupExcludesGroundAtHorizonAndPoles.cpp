#include "AerialPerspectiveStage.h"
#include "Check.h"
#include "StageSubmission.h"
#include "test/harness/render/NativeImageReadback.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kSkyWidth = 16;
constexpr uint32_t kSkyHeight = 8;
constexpr uint32_t kVeilWidth = 64;
constexpr uint32_t kVeilHeight = 32;
constexpr auto kColour = Format::Rgba32Float;
constexpr double kSamplerTolerance = 2.0 / (kSkyWidth - 1u) / 256.0;

std::unique_ptr<Texture>
MakeTexture(Device &device, uint32_t width, uint32_t height, Format format = kColour) {
  return Require(
      device.CreateTexture({.Width = width,
                            .Height = height,
                            .Texels = format,
                            .Use = TextureUse::Sampled | TextureUse::ColourAttachment |
                                   TextureUse::CopyDestination | TextureUse::CopySource}));
}

void Upload(Device &device, Commands &commands, Texture &texture, std::span<const float> pixels) {
  auto transfer = Require(device.CreateBuffer(
      {.Bytes = pixels.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*transfer, {.Bytes = pixels.size_bytes()}));
  std::memcpy(mapped.data(), pixels.data(), pixels.size_bytes());
  Require(device.Unmap(*transfer));
  const auto &shape = texture.Description();
  Require(commands.Copy(*transfer,
                        texture,
                        {.RowBytes = pixels.size_bytes() / shape.Height,
                         .Region = {.Width = shape.Width, .Height = shape.Height}}));
}

struct Sources {
  std::unique_ptr<Texture> Scene, Depth, Sky, Veil, Output;
  std::unique_ptr<Sampler> Exact, Linear;
  Medium Air = kEarthAir;

  explicit Sources(Device &device)
      : Scene(MakeTexture(device, 1, 1)),
        Depth(MakeTexture(device, 1, 1, Format::R32Float)),
        Sky(MakeTexture(device, kSkyWidth, kSkyHeight)),
        Veil(MakeTexture(device, kVeilWidth, kVeilHeight)),
        Output(MakeTexture(device, 1, 1)) {
    Air.BottomRadiusKm = 1;
    Air.TopRadiusKm = 2;
    Exact = Require(device.CreateSampler(
        {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
    Linear = Require(device.CreateSampler({.Mip = Filter::Nearest}));
  }

  void Prepare(Device &device) const {
    std::vector<float> sky(kSkyWidth * kSkyHeight * 4u);
    for (uint32_t y = 0; y < kSkyHeight; ++y) {
      for (uint32_t x = 0; x < kSkyWidth; ++x) {
        const size_t at = (y * kSkyWidth + x) * 4u;
        sky[at] = y < kSkyHeight / 2u ? .25f + 2.0f * x / (kSkyWidth - 1u) : 1000.0f;
        sky[at + 1] = y < kSkyHeight / 2u ? .5f : 0.0f;
        sky[at + 2] = y < kSkyHeight / 2u ? 1.0f : 0.0f;
        sky[at + 3] = 1;
      }
    }
    std::vector<float> veil(kVeilWidth * kVeilHeight * 4u, 1);
    for (uint32_t y = 0; y < kVeilHeight; ++y) {
      for (uint32_t x = 0; x < kVeilWidth; ++x) {
        const auto look = medium_core::mediumTransmittanceTexel(
            Air,
            {.U = (static_cast<float>(x) + .5f) / static_cast<float>(kVeilWidth),
             .V = (static_cast<float>(y) + .5f) / static_cast<float>(kVeilHeight)},
            {.WidthPx = static_cast<float>(kVeilWidth),
             .HeightPx = static_cast<float>(kVeilHeight)});
        const float through = std::exp(-mediumTopReach(Air, look.RadiusKm, look.CosZenith));
        const size_t at = (y * kVeilWidth + x) * 4u;
        for (size_t channel = 0; channel < 3; ++channel) { veil[at + channel] = through; }
      }
    }
    auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
    Upload(device, *commands, *Scene, std::array<float, 4>{0, 0, 0, 1});
    Upload(device, *commands, *Depth, std::array<float, 1>{1});
    Upload(device, *commands, *Sky, sky);
    Upload(device, *commands, *Veil, veil);
    const auto done = Require(device.Submit(std::move(commands)));
    CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
          "independent sky/ground colours and extinction upload");
  }
};

std::array<float, 4> Draw(Device &device,
                          const Sources &source,
                          AerialPerspectiveStage &stage,
                          Vec3f up,
                          Vec3f look,
                          Vec3f sun) {
  stage.Declare(source.Air,
                {.SunDir = sun,
                 .WorldUp = up,
                 .IlluminanceLux = 1,
                 .EyeHeightM = (0.5f - kMediumGroundLiftKm) * 1000});
  stage.SetBasis({.Right = {}, .Up = {}, .Forward = look});
  stage.SetDepthReconstruction({{250, 0, 1, 0}});
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  const std::array colours{
      ColourAttachment{.Image = {.Image = source.Output.get()}, .Begin = Load::Clear}};
  Require(commands->BeginRenderPass({.Colours = colours}));
  StageSubmission submission;
  Require(stage.Encode({},
                       {.Commands = commands.get(),
                        .Submission = submission,
                        .Area = {.Width = 1, .Height = 1},
                        .Clip = {.Width = 1, .Height = 1}}));
  Require(commands->EndRenderPass());
  const auto done = Require(device.Submit(std::move(commands)));
  submission.Commit();
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "aerial shading submits");
  const auto read = ReadImage(device, *source.Output, {.Width = 1, .Height = 1});
  std::array<float, 4> pixel{};
  CHECK(read.Rows.size() >= sizeof(pixel), "the actual fragment output reaches readback");
  if (read.Rows.size() < sizeof(pixel)) { Failed(); }
  std::memcpy(pixel.data(), read.Rows.data(), sizeof(pixel));
  return pixel;
}

void CheckDirections(Device &device) {
  const Sources source(device);
  source.Prepare(device);
  AerialPerspectiveStage stage;
  Require(stage.Configure({.Device = &device},
                          {.Scene = source.Scene.get(),
                           .Depth = source.Depth.get(),
                           .SkyView = source.Sky.get(),
                           .Transmittance = source.Veil.get(),
                           .Exact = source.Exact.get(),
                           .Lut = source.Linear.get()},
                          kColour));
  for (const Vec3f up : {Vec3f{{0, 1, 0}}, Vec3f{{1, 0, 0}}}) {
    const Vec3f along = up[0] == 0 ? Vec3f{{1, 0, 0}} : Vec3f{{0, 1, 0}};
    const Vec3f across{{0, 0, 1}};
    const float diagonal = std::sqrt(0.5f);
    const std::array<Vec3f, 6> suns{
        up, up * -1.0f, along, along * -1.0f, across, (along + across) * diagonal};
    const std::array<float, 6> sunColumns{
        0, 0, 0, 1, diagonal, std::sqrt((1.0f - diagonal) * 0.5f)};
    for (size_t index = 0; index < suns.size(); ++index) {
      for (const Vec3f look : {up, up * -1.0f, along}) {
        const auto pixel = Draw(device, source, stage, up, look, suns[index]);
        for (const float value : pixel) {
          CHECK(std::isfinite(value), "zenith and nadir sun/look directions keep finite radiance");
        }
        CHECK(pixel[2] > .01f && pixel[2] < 1, "a finite segment receives positive sky scattering");
        if (!(pixel[2] > .01f)) { continue; }
        const float column = Dot(look, along) > .5f ? sunColumns[index] : 0;
        CHECK_NEAR(pixel[0] / pixel[2],
                   .25 + 2 * column,
                   kSamplerTolerance,
                   "radiance ratio",
                   "relative sun azimuth uses the correct finite texel centres");
        CHECK_NEAR(pixel[1] / pixel[2],
                   .5,
                   1e-4,
                   "radiance ratio",
                   "horizon and downward rays exclude the bright virtual ground");
      }
    }
  }
}
}

int main() {
  auto device = Require(CreateDevice({}));
  CheckDirections(*device);
  return Report();
}
