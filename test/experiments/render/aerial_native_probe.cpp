#include "AerialPerspectiveParameters.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <ios>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <vector>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kWidth = 35, kHeight = 19, kColourRow = 768, kOtherRow = 256;
constexpr size_t kChannels = 4;
constexpr double kRadianceFloor = 1e-7, kRelativeTolerance = 1e-5;
constexpr std::array<float, 3> kSolar{.75f, .5f, .25f};
constexpr std::array<float, 3> kPsi{.015625f, .03125f, .0625f};
constexpr std::array<uint16_t, 4> kSolarHalf{0x3a00, 0x3800, 0x3400, 0x3c00};
constexpr std::array<uint16_t, 4> kPsiHalf{0x2400, 0x2800, 0x2c00, 0x3c00};
using Pixel = std::array<float, kChannels>;
static_assert(sizeof(Pixel) == kChannels * sizeof(uint32_t) &&
              std::endian::native == std::endian::little);
constexpr float kBlueBase = .125f, kAlphaBase = .25f;
constexpr float kHomogeneousScaleHeightKm = 1e20f;
constexpr Vec3f kRayleighPerKm{{.1f, .2f, .3f}};
constexpr float kMieScatteringPerKm = .03f, kMieExtinctionPerKm = .04f;
constexpr Vec2f kTanHalf{{.5f, .3f}};
constexpr float kEyeRadiusKm = 1.5f, kReconstructionM = 250;

struct ImageShape {
  uint32_t Width;
  uint32_t Height;
  uint32_t RowBytes;
};

struct Directions {
  Vec3f WorldUp;
  Vec3f View;
  Vec3f Sun;
};

Pixel ScenePixel(uint32_t x, uint32_t y) {
  return {static_cast<float>(x + 1) / 64,
          static_cast<float>(y + 1) / 32,
          kBlueBase + static_cast<float>(x % 4) / 16,
          kAlphaBase + static_cast<float>(y % 4) / 8};
}

Medium HomogeneousMedium() {
  auto air = kEarthAir;
  air.BottomRadiusKm = 1;
  air.TopRadiusKm = 2;
  air.RayleighScaleHeightKm = air.MieScaleHeightKm = kHomogeneousScaleHeightKm;
  air.RayleighScatteringPerKm = kRayleighPerKm;
  air.MieScatteringPerKm = kMieScatteringPerKm;
  air.MieExtinctionPerKm = kMieExtinctionPerKm;
  air.OzoneAbsorptionPerKm = {};
  return air;
}

template <typename Value, typename Function>
std::vector<std::byte> Pixels(ImageShape shape, Function value) {
  std::vector<std::byte> bytes(size_t{shape.RowBytes} * shape.Height);
  for (uint32_t y = 0; y < shape.Height; ++y) {
    for (uint32_t x = 0; x < shape.Width; ++x) {
      const Value pixel = value(x, y);
      std::memcpy(bytes.data() + size_t{y} * shape.RowBytes + size_t{x} * sizeof(Value),
                  &pixel,
                  sizeof(pixel));
    }
  }
  return bytes;
}

void UploadTexture(Device &device,
                   Commands &commands,
                   Texture &texture,
                   std::span<const std::byte> bytes,
                   uint32_t rowBytes) {
  auto upload = Require(device.CreateBuffer(
      {.Bytes = bytes.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*upload, {.Bytes = bytes.size_bytes()}));
  std::ranges::copy(bytes, mapped.begin());
  Require(device.Unmap(*upload));
  Require(commands.Copy(
      *upload,
      texture,
      {.RowBytes = rowBytes,
       .Region = {.Width = texture.Description().Width, .Height = texture.Description().Height}}));
}

struct Sources {
  std::array<std::unique_ptr<Texture>, 4> Images;
  std::unique_ptr<Texture> Output;
  std::unique_ptr<Buffer> Readback;
  std::unique_ptr<Sampler> Exact, Linear;

  explicit Sources(Device &device) {
    constexpr auto sourceUse = TextureUse::Sampled | TextureUse::CopyDestination;
    const std::array descriptions{
        TextureDescription{
            .Width = kWidth, .Height = kHeight, .Texels = Format::Rgba32Float, .Use = sourceUse},
        TextureDescription{
            .Width = kWidth, .Height = kHeight, .Texels = Format::Depth32Float, .Use = sourceUse},
        TextureDescription{
            .Width = 2, .Height = 2, .Texels = Format::Rgba16Float, .Use = sourceUse},
        TextureDescription{
            .Width = 2, .Height = 2, .Texels = Format::Rgba16Float, .Use = sourceUse}};
    for (size_t i = 0; i < Images.size(); ++i) {
      Images[i] = Require(device.CreateTexture(descriptions[i]));
    }
    Output = Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    Readback = Require(device.CreateBuffer({.Bytes = uint64_t{kColourRow} * kHeight,
                                            .Use = BufferUse::CopyDestination,
                                            .Placement = Memory::Readback}));
    Exact = Require(device.CreateSampler(
        {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
    Linear = Require(device.CreateSampler({.Mip = Filter::Nearest}));
    auto commands = Require(device.Begin());
    UploadTexture(
        device,
        *commands,
        *Images[0],
        Pixels<Pixel>({.Width = kWidth, .Height = kHeight, .RowBytes = kColourRow}, ScenePixel),
        kColourRow);
    UploadTexture(device,
                  *commands,
                  *Images[1],
                  Pixels<float>({.Width = kWidth, .Height = kHeight, .RowBytes = kOtherRow},
                                [](uint32_t x, uint32_t) { return x % 7 == 0 ? 0.0f : 1.0f; }),
                  kOtherRow);
    UploadTexture(device,
                  *commands,
                  *Images[2],
                  Pixels<std::array<uint16_t, 4>>({.Width = 2, .Height = 2, .RowBytes = kOtherRow},
                                                  [](uint32_t, uint32_t) { return kPsiHalf; }),
                  kOtherRow);
    UploadTexture(device,
                  *commands,
                  *Images[3],
                  Pixels<std::array<uint16_t, 4>>({.Width = 2, .Height = 2, .RowBytes = kOtherRow},
                                                  [](uint32_t, uint32_t) { return kSolarHalf; }),
                  kOtherRow);
    const auto completion = Require(device.Submit(std::move(commands)));
    CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
          "independent scene/depth/half-LUT uploads complete");
  }
};

AerialPerspectiveParameters Parameters(const Directions &directions) {
  AerialPerspectiveParameters result{};
  const Vec3f right{{0, 0, 1}};
  const auto viewUp = Cross(right, directions.View);
  for (size_t axis = 0; axis < 3; ++axis) {
    result.Right[axis] = right[axis];
    result.Up[axis] = viewUp[axis];
    result.Fwd[axis] = directions.View[axis];
    result.WorldUp[axis] = directions.WorldUp[axis];
    result.SunDir[axis] = directions.Sun[axis];
  }
  result.TanHalf = kTanHalf;
  result.Illuminance = 1;
  result.EyeRadiusKm = kEyeRadiusKm;
  result.DepthReconstruction = {{kReconstructionM, 0, 1, 0}};
  result.Air = HomogeneousMedium();
  return result;
}

std::vector<Pixel> Draw(Device &device,
                        const GraphicsPipeline &pipeline,
                        const Sources &sources,
                        AerialPerspectiveParameters parameters) {
  std::array<Binding, 5> bindings;
  for (uint32_t slot = 0; slot < sources.Images.size(); ++slot) {
    bindings[slot] = {
        .Set = 2,
        .Slot = slot,
        .Value = SampledTexture{.Image = {.Image = sources.Images[slot].get()},
                                .Sampling = slot < 2 ? sources.Exact.get() : sources.Linear.get()}};
  }
  bindings.back() = {
      .Set = 3, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&parameters, 1))}};
  const std::array attachments{
      ColourAttachment{.Image = {.Image = sources.Output.get()}, .Begin = Load::Clear}};
  auto commands = Require(device.Begin());
  Require(commands->BeginRenderPass({.Colours = attachments, .Depth = std::nullopt}));
  Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                    .Bindings = bindings,
                                    .Vertices = {},
                                    .Area = {.Width = kWidth, .Height = kHeight},
                                    .Clip = {.Width = kWidth, .Height = kHeight}},
                          .Range = {.Count = 3}}));
  parameters.Illuminance = 0;
  Require(commands->EndRenderPass());
  Require(commands->Copy(*sources.Output,
                         *sources.Readback,
                         {.RowBytes = kColourRow, .Region = {.Width = kWidth, .Height = kHeight}}));
  const auto completion = Require(device.Submit(std::move(commands)));
  CHECK(Require(device.Wait(completion, std::chrono::seconds(30))) == CompletionStatus::Ready,
        "the native aerial raster pass completes");
  const auto bytes =
      Require(device.MapRead(*sources.Readback, {.Bytes = uint64_t{kColourRow} * kHeight}));
  std::vector<Pixel> pixels(size_t{kWidth} * kHeight);
  for (uint32_t y = 0; y < kHeight; ++y) {
    std::memcpy(pixels.data() + size_t{y} * kWidth,
                bytes.data() + size_t{y} * kColourRow,
                size_t{kWidth} * sizeof(Pixel));
  }
  Require(device.Unmap(*sources.Readback));
  return pixels;
}

Pixel Reference(uint32_t x, uint32_t y, const AerialPerspectiveParameters &parameters) {
  Pixel expected = ScenePixel(x, y);
  if (x % 7 == 0) { return expected; }
  const double ndcX = 2 * (static_cast<double>(x) + .5) / kWidth - 1;
  const double ndcY = 1 - 2 * (static_cast<double>(y) + .5) / kHeight;
  std::array<double, 3> ray{};
  double lengthSquared = 0;
  double sunLengthSquared = 0;
  double upSun = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    ray[axis] = parameters.Fwd[axis] + ndcX * parameters.TanHalf[0] * parameters.Right[axis] +
                ndcY * parameters.TanHalf[1] * parameters.Up[axis];
    lengthSquared += ray[axis] * ray[axis];
    sunLengthSquared += static_cast<double>(parameters.SunDir[axis]) * parameters.SunDir[axis];
    upSun += static_cast<double>(parameters.WorldUp[axis]) * parameters.SunDir[axis];
  }
  double forward = 0;
  double cosine = 0;
  for (size_t axis = 0; axis < 3; ++axis) {
    ray[axis] /= std::sqrt(lengthSquared);
    forward += ray[axis] * parameters.Fwd[axis];
    cosine += ray[axis] * parameters.SunDir[axis] / std::sqrt(sunLengthSquared);
  }
  constexpr double kKmPerM = .001;
  const double distance = parameters.DepthReconstruction[0] * kKmPerM / forward;
  const auto &air = parameters.Air;
  const double g = air.MiePhaseG;
  const double pi = std::acos(-1.0);
  const double phaseR = 3 * (1 + cosine * cosine) / (16 * pi);
  const double phaseM = 3 / (8 * pi) * (1 - g * g) / (2 + g * g) * (1 + cosine * cosine) /
                        std::pow(1 + g * g - 2 * g * cosine, 1.5);
  const bool lit = upSun >= 0;
  for (size_t channel = 0; channel < 3; ++channel) {
    const double rayleigh = air.RayleighScatteringPerKm[channel];
    const double mie = air.MieScatteringPerKm;
    const double extinction = rayleigh + air.MieExtinctionPerKm;
    const double source = (lit ? kSolar[channel] : 0) * (rayleigh * phaseR + mie * phaseM) +
                          (rayleigh + mie) * kPsi[channel];
    const double integral =
        extinction == 0 ? distance : -std::expm1(-extinction * distance) / extinction;
    expected[channel] = static_cast<float>(expected[channel] * std::exp(-extinction * distance) +
                                           parameters.Illuminance * source * integral);
  }
  return expected;
}

void CheckImage(std::span<const Pixel> pixels,
                const AerialPerspectiveParameters &parameters,
                std::ofstream &actual,
                std::ofstream &reference) {
  size_t badRgb = 0;
  size_t badAlpha = 0;
  size_t badBackground = 0;
  double worst = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const auto &pixel = pixels[size_t{y} * kWidth + x];
      const auto expected = Reference(x, y, parameters);
      for (size_t channel = 0; channel < 3; ++channel) {
        const double error = std::fabs(pixel[channel] - expected[channel]);
        const double tolerance =
            std::max(kRadianceFloor, std::fabs(expected[channel]) * kRelativeTolerance);
        if (!std::isfinite(pixel[channel]) || error > tolerance) { ++badRgb; }
        worst = std::max(worst, error);
      }
      if (pixel[3] != expected[3]) { ++badAlpha; }
      if (x % 7 == 0 && pixel != expected) { ++badBackground; }
      reference.write(reinterpret_cast<const char *>(expected.data()), sizeof(expected));
    }
  }
  actual.write(reinterpret_cast<const char *>(pixels.data()),
               static_cast<std::streamsize>(pixels.size_bytes()));
  CHECK(badRgb == 0, "native finite RGB source agrees with the independent homogeneous solution");
  CHECK(badAlpha == 0, "native aerial shading preserves every alpha exactly");
  CHECK(badBackground == 0, "native background bypass is bit exact");
  std::println("aerial35x19 max_absolute_RGB={} bad_RGB={} bad_alpha={} bad_background={}",
               worst,
               badRgb,
               badAlpha,
               badBackground);
}

void Cases(Device &device, const GraphicsPipeline &pipeline, const char *output) {
  const Sources sources(device);
  std::ofstream actual(output, std::ios::binary);
  std::ofstream reference(std::string(output) + ".reference", std::ios::binary);
  CHECK(actual && reference, "native/reference image files open");
  if (!actual || !reference) { Failed(); }
  for (const Vec3f up : {Vec3f{{0, 1, 0}}, Vec3f{{1, 0, 0}}}) {
    const Vec3f along = up[0] == 0 ? Vec3f{{1, 0, 0}} : Vec3f{{0, 1, 0}};
    const Vec3f across{{0, 0, 1}};
    const float diagonal = std::sqrt(.5f);
    for (const auto &sun :
         {up, up * -1.0f, along, along * -1.0f, across, (along + across) * diagonal}) {
      for (const auto &look : {up, up * -1.0f, along}) {
        auto parameters = Parameters({.WorldUp = up, .View = look, .Sun = sun});
        parameters.Illuminance = up[0] == 0 ? 1 : 4;
        const auto pixels = Draw(device, pipeline, sources, parameters);
        CheckImage(pixels, parameters, actual, reference);
      }
    }
  }
  for (int edge = 0; edge < 3; ++edge) {
    auto parameters = Parameters({.WorldUp = {{0, 1, 0}}, .View = {{1, 0, 0}}, .Sun = {{0, 1, 0}}});
    if (edge == 0) { parameters.Illuminance = 0; }
    if (edge == 1) {
      parameters.Air.RayleighScatteringPerKm = {};
      parameters.Air.MieScatteringPerKm = parameters.Air.MieExtinctionPerKm = 0;
    }
    if (edge == 2) { parameters.DepthReconstruction[0] = 0; }
    const auto pixels = Draw(device, pipeline, sources, parameters);
    CheckImage(pixels, parameters, actual, reference);
  }
  actual.flush();
  reference.flush();
  CHECK(actual && reference, "all 39 native/reference images are saved");
}
}

int main(int argc, char **argv) {
  if (argc != 4) {
    std::println(stderr,
                 "usage: aerial-native-probe fullscreenNdc.vert.spv aerialPerspective.frag.spv "
                 "output.rgba32f");
    return 2;
  }
  auto device = Require(CreateDevice({.Validation = true}));
  const auto vertex = Spirv(argv[1]);
  const auto fragment = Spirv(argv[2]);
  const std::array targets{ColourTarget{.Texels = Format::Rgba32Float, .Rgb = {}, .Alpha = {}}};
  auto pipeline = Require(device->CreateGraphicsPipeline({.Vertex = {.Spirv = vertex},
                                                          .Fragment = ShaderCode{.Spirv = fragment},
                                                          .Streams = {},
                                                          .Attributes = {},
                                                          .Colours = targets,
                                                          .Depth = std::nullopt,
                                                          .Culling = Cull::None,
                                                          .Label = "FiniteAerialControl"}));
  Cases(*device, *pipeline, argv[3]);
  return Report();
}
