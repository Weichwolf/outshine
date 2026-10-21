#include "Check.h"
#include "EnvironmentSpecularLayout.h"
#include "EnvironmentSpecularStage.h"
#include "IrradianceLayout.h"
#include "test/harness/render/NativeImageReadback.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <numbers>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;

struct Sources {
  std::unique_ptr<Texture> Sky, Atlas;
  std::unique_ptr<Buffer> Irradiance;
  std::unique_ptr<Sampler> Sample;

  explicit Sources(Device &device) {
    Sky =
        Require(device.CreateTexture({.Width = 16,
                                      .Height = 8,
                                      .Texels = Format::Rgba32Float,
                                      .Use = TextureUse::ColourAttachment | TextureUse::Sampled}));
    Atlas = Require(device.CreateTexture(
        {.Width = kEnvironmentWidth,
         .Height = kEnvironmentHeight,
         .Texels = Format::Rgba16Float,
         .Use = TextureUse::Storage | TextureUse::Sampled | TextureUse::CopySource}));
    Irradiance =
        Require(device.CreateBuffer({.Bytes = kIrradianceFloats * sizeof(float),
                                     .Use = BufferUse::Storage | BufferUse::CopyDestination}));
    Sample = Require(device.CreateSampler({.Mip = Filter::Nearest}));
  }
};

void UploadSources(Device &device, const Sources &source) {
  const std::array<float, kIrradianceFloats> sky{0.25f * std::numbers::pi_v<float>,
                                                 0.5f * std::numbers::pi_v<float>,
                                                 std::numbers::pi_v<float>,
                                                 0,
                                                 0,
                                                 0};
  auto transfer = Require(device.CreateBuffer(
      {.Bytes = sizeof(sky), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*transfer, {.Bytes = sizeof(sky)}));
  std::memcpy(mapped.data(), sky.data(), sizeof(sky));
  Require(device.Unmap(*transfer));
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  const std::array colours{ColourAttachment{.Image = {.Image = source.Sky.get()},
                                            .Begin = Load::Clear,
                                            .Clear = {0.25f, 0.5f, 1.0f, 1.0f}}};
  Require(commands->BeginRenderPass({.Colours = colours}));
  Require(commands->EndRenderPass());
  Require(commands->Copy(*transfer, *source.Irradiance, {.Bytes = sizeof(sky)}));
  const auto done = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the constant sky and Lambertian ground are submitted");
}

void Filter(Device &device, EnvironmentSpecularStage &stage, bool commit) {
  auto commands = Require(RetryOffline([&] { return device.Begin(); }, std::chrono::seconds(5)));
  StageSubmission submission;
  Require(stage.Encode({.Commands = commands.get(), .Submission = submission}));
  if (!commit) { return; }
  const auto done = Require(device.Submit(std::move(commands)));
  submission.Commit();
  CHECK(Require(device.Wait(done, std::chrono::seconds(5))) == CompletionStatus::Ready,
        "the accepted filtering submission completes");
}

void CheckAtlas(Device &device, const Sources &source) {
  const auto read =
      ReadImage(device, *source.Atlas, {.Width = kEnvironmentWidth, .Height = kEnvironmentHeight});
  constexpr std::array<uint16_t, 4> expected{0x3400, 0x3800, 0x3c00, 0x3c00};
  bool constant = true;
  for (uint32_t row = 0; row < kEnvironmentHeight; ++row) {
    for (uint32_t col = 0; col < kEnvironmentWidth; ++col) {
      std::array<uint16_t, 4> texel{};
      std::memcpy(texel.data(), read.Rows.data() + row * read.RowBytes + col * 8u, sizeof(texel));
      for (size_t channel = 0; channel < texel.size(); ++channel) {
        const int error = static_cast<int>(texel[channel]) - expected[channel];
        constant &= error >= -1 && error <= 1;
      }
    }
  }
  CHECK(constant, "all roughness slices, edges and corners preserve constant incident radiance");
}

void CheckDiffuseHalfspace(Device &device, const Sources &source) {
  const auto read =
      ReadImage(device, *source.Atlas, {.Width = kEnvironmentWidth, .Height = kEnvironmentHeight});
  constexpr std::array<float, 3> sky{0.25f, 0.5f, 1.0f};
  double maximumError = 0;
  for (uint32_t y = 0; y < kEnvironmentSide; ++y) {
    for (uint32_t x = 0; x < kEnvironmentSide; ++x) {
      const double px = 2.0 * (x + 0.5) / kEnvironmentSide - 1.0;
      const double py = 2.0 * (y + 0.5) / kEnvironmentSide - 1.0;
      const double z = 1.0 - std::abs(px) - std::abs(py);
      const double nx = z < 0 ? std::copysign(1.0 - std::abs(py), px) : px;
      const double ny = z < 0 ? std::copysign(1.0 - std::abs(px), py) : py;
      const double expected = 0.5 * (1.0 + z / std::sqrt(nx * nx + ny * ny + z * z));
      const uint32_t row = kEnvironmentDiffuseLevel * kEnvironmentWidth + kEnvironmentBorder + y;
      const uint32_t column = kEnvironmentBorder + x;
      std::array<uint16_t, 4> pixel{};
      std::memcpy(
          pixel.data(), read.Rows.data() + row * read.RowBytes + column * 8u, sizeof(pixel));
      for (size_t channel = 0; channel < sky.size(); ++channel) {
        const uint16_t bits = pixel[channel];
        const int exponent = (bits >> 10u) & 31;
        const float fraction = static_cast<float>(bits & 1023u);
        const float value = exponent == 0 ? std::ldexp(fraction, -24)
                                          : std::ldexp(1.0f + fraction / 1024.0f, exponent - 15);
        CHECK((bits & 0x8000u) == 0 && exponent != 31,
              "diffuse radiance is finite and nonnegative");
        maximumError = std::max(maximumError, std::abs(value / sky[channel] - expected));
      }
    }
  }
  CHECK(maximumError < 0.035,
        "128 cosine samples match the analytic bright-sky/dark-ground view factor within 3.5 "
        "percent");
}

void CheckFiltering(Device &device) {
  const Sources source(device);
  UploadSources(device, source);
  EnvironmentSpecularStage stage;
  Require(stage.Configure({.Device = &device},
                          {.Sky = source.Sky.get(),
                           .Irradiance = source.Irradiance.get(),
                           .Sampler = source.Sample.get(),
                           .Target = source.Atlas.get()}));
  stage.Declare(kEarthAir, 0.5f, 0, {{1, 1, 1}});
  Filter(device, stage, false);
  CHECK(!stage.Settled(), "cancelled compute leaves the product dirty");
  Filter(device, stage, true);
  CHECK(stage.Settled(), "only accepted submission publishes the filtered product");
  CheckAtlas(device, source);
  stage.Declare(kEarthAir, 0.5f, 0, {{1, 1, 1}});
  CHECK(stage.Settled(), "an unchanged lighting state retains its filtered product");
  stage.Declare(kEarthAir, 0.5f, 0, {{0, 0, 0}});
  CHECK(!stage.Settled(), "changed ground radiance invalidates the product");
  Filter(device, stage, true);
  CheckDiffuseHalfspace(device, source);
  const auto read =
      ReadImage(device, *source.Atlas, {.Width = kEnvironmentWidth, .Height = kEnvironmentHeight});
  uint16_t ground = 1;
  std::memcpy(&ground, read.Rows.data() + read.RowBytes + 8, sizeof(ground));
  CHECK(ground == 0, "a downward mirror ray sees the changed ground rather than sky");
}
}

int main() {
  auto device = Require(CreateDevice({}));
  CheckFiltering(*device);
  return Report();
}
