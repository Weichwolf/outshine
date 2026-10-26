#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <vector>
#include <optional>
#include <utility>
#include <ios>
#include <string>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <span>

namespace outshine::Test::MediumViewProbe {
namespace {
using namespace outshine::Test::Gpu;
constexpr uint32_t kSmallRow = 256;
constexpr float kHomogeneousScaleKm = 1e20f;
constexpr double kRelativeTolerance = 1e-5;
constexpr double kAbsoluteTolerance = 1e-7;
constexpr std::array<float, 3> kSolar{0.75f, 0.5f, 0.25f};
constexpr std::array<float, 3> kPsi{0.015625f, 0.03125f, 0.0625f};
constexpr std::array<uint16_t, 4> kSolarHalf{0x3a00, 0x3800, 0x3400, 0x3c00};
constexpr std::array<uint16_t, 4> kPsiHalf{0x2400, 0x2800, 0x2c00, 0x3c00};

constexpr float kBlueBase = 0.125f, kAlphaBase = 0.25f;
constexpr Vec3f kRayleighPerKm{{0.1f, 0.2f, 0.3f}};
constexpr float kMieScatterPerKm = 0.03f, kMieExtinctionPerKm = 0.04f;
constexpr float kOrthoWidthM = 360, kOrthoHeightM = 240;

Pixel ScenePixel(uint32_t x, uint32_t y) {
  return {static_cast<float>(x + 1) / 64,
          static_cast<float>(y + 1) / 32,
          kBlueBase + static_cast<float>(x % 4) / 16,
          kAlphaBase + static_cast<float>(y % 4) / 8};
}

Medium Homogeneous() {
  auto air = kEarthAir;
  air.BottomRadiusKm = 1;
  air.TopRadiusKm = 3;
  air.RayleighScaleHeightKm = air.MieScaleHeightKm = kHomogeneousScaleKm;
  air.RayleighScatteringPerKm = kRayleighPerKm;
  air.MieScatteringPerKm = kMieScatterPerKm;
  air.MieExtinctionPerKm = kMieExtinctionPerKm;
  air.OzoneAbsorptionPerKm = {};
  return air;
}

struct Sources {
  std::array<std::unique_ptr<Texture>, 4> Inputs;
  std::unique_ptr<Texture> Output;
  std::unique_ptr<Buffer> Readback;
  std::unique_ptr<Sampler> Exact, Linear;

  explicit Sources(Device &device) {
    constexpr auto sampled = TextureUse::Sampled | TextureUse::CopyDestination;
    const std::array descriptions{
        TextureDescription{
            .Width = kWidth, .Height = kHeight, .Texels = Format::Rgba32Float, .Use = sampled},
        TextureDescription{
            .Width = kWidth, .Height = kHeight, .Texels = Format::Depth32Float, .Use = sampled},
        TextureDescription{.Width = 2, .Height = 2, .Texels = Format::Rgba16Float, .Use = sampled},
        TextureDescription{.Width = 2, .Height = 2, .Texels = Format::Rgba16Float, .Use = sampled}};
    for (size_t i = 0; i < Inputs.size(); ++i) {
      Inputs[i] = Require(device.CreateTexture(descriptions[i]));
    }
    Output = Require(
        device.CreateTexture({.Width = kWidth,
                              .Height = kHeight,
                              .Texels = Format::Rgba32Float,
                              .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
    Readback = Require(device.CreateBuffer({.Bytes = uint64_t{kRowBytes} * kHeight,
                                            .Use = BufferUse::CopyDestination,
                                            .Placement = Memory::Readback}));
    Exact = Require(device.CreateSampler(
        {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
    Linear = Require(device.CreateSampler({.Mip = Filter::Nearest}));
    auto commands = Require(device.Begin());
    UploadImage(
        device,
        *commands,
        *Inputs[0],
        ImageBytes<Pixel>({.Width = kWidth, .Height = kHeight, .RowBytes = kRowBytes}, ScenePixel),
        kRowBytes);
    UploadImage(
        device,
        *commands,
        *Inputs[2],
        ImageBytes<std::array<uint16_t, 4>>({.Width = 2, .Height = 2, .RowBytes = kSmallRow},
                                            [](uint32_t, uint32_t) { return kPsiHalf; }),
        kSmallRow);
    UploadImage(
        device,
        *commands,
        *Inputs[3],
        ImageBytes<std::array<uint16_t, 4>>({.Width = 2, .Height = 2, .RowBytes = kSmallRow},
                                            [](uint32_t, uint32_t) { return kSolarHalf; }),
        kSmallRow);
    Finish(device, std::move(commands));
  }

  std::vector<Pixel> Draw(Device &device,
                          const GraphicsPipeline &pipeline,
                          const Scenario &scenario,
                          const Medium &air) const {
    auto frame = FrameFor(scenario, air);
    const Mat4f projection = scenario.Projection.Projection();
    const float clipDepth = (projection[14] - projection[10] * scenario.AlongViewM) /
                            (projection[15] - projection[11] * scenario.AlongViewM);
    const float written =
        scenario.Area.MinDepth + (scenario.Area.MaxDepth - scenario.Area.MinDepth) * clipDepth;
    const auto depthBytes =
        ImageBytes<float>({.Width = kWidth, .Height = kHeight, .RowBytes = kSmallRow},
                          [written](uint32_t x, uint32_t) { return x % 7 == 0 ? 0.0f : written; });
    auto commands = Require(device.Begin());
    UploadImage(device, *commands, *Inputs[1], depthBytes, kSmallRow);
    std::array<Binding, 5> bindings;
    for (uint32_t slot = 0; slot < Inputs.size(); ++slot) {
      bindings[slot] = {.Set = 2,
                        .Slot = slot,
                        .Value = SampledTexture{.Image = {.Image = Inputs[slot].get()},
                                                .Sampling = slot < 2 ? Exact.get() : Linear.get()}};
    }
    bindings.back() = {.Set = 3, .Slot = 2, .Value = Uniform{std::as_bytes(std::span(&frame, 1))}};
    const std::array colours{
        ColourAttachment{.Image = {.Image = Output.get()}, .Begin = Load::Clear}};
    Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
    Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                      .Bindings = bindings,
                                      .Vertices = {},
                                      .Area = scenario.Area,
                                      .Clip = ViewClip(scenario)},
                            .Range = {.Count = 3}}));
    frame.PixelToNdc = {};
    Require(commands->EndRenderPass());
    Require(
        commands->Copy(*Output,
                       *Readback,
                       {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
    Finish(device, std::move(commands));
    return ReadImage(device, *Readback);
  }
};

struct AirInterval {
  double BeginKm;
  double EndKm;
};

AirInterval PhysicalAirSpan(const GeometricRay &ray, const Medium &air) {
  if (ray.RadiusKm < air.BottomRadiusKm) { return {}; }
  const double projection = ray.RadiusKm * ray.CosView;
  const double topDiscriminant = projection * projection +
                                 static_cast<double>(air.TopRadiusKm) * air.TopRadiusKm -
                                 ray.RadiusKm * ray.RadiusKm;
  if (topDiscriminant < 0) { return {}; }
  const double topRoot = std::sqrt(topDiscriminant);
  const double begin = std::max(0.0, -projection - topRoot);
  double end = std::min(ray.DistanceKm, -projection + topRoot);
  const double groundDiscriminant = projection * projection +
                                    static_cast<double>(air.BottomRadiusKm) * air.BottomRadiusKm -
                                    ray.RadiusKm * ray.RadiusKm;
  if (ray.CosView < 0 && groundDiscriminant >= 0) {
    const double hit = -projection - std::sqrt(groundDiscriminant);
    if (hit >= 0) { end = std::min(end, hit); }
  }
  return {.BeginKm = begin, .EndKm = std::max(begin, end)};
}

double LitIntegral(const GeometricRay &ray, const Medium &air, AirInterval span, double sigma) {
  const double sunProjection = ray.RadiusKm * ray.CosSun;
  const double viewProjection = ray.RadiusKm * ray.CosView;
  const double quadratic = 1 - ray.ViewSun * ray.ViewSun;
  const double linear = viewProjection - sunProjection * ray.ViewSun;
  const double constant = ray.RadiusKm * ray.RadiusKm - sunProjection * sunProjection -
                          static_cast<double>(air.BottomRadiusKm) * air.BottomRadiusKm;
  const double discriminant = linear * linear - quadratic * constant;
  std::array<double, 5> cuts{span.BeginKm, span.EndKm, span.BeginKm, span.BeginKm, span.BeginKm};
  if (quadratic > 0 && discriminant >= 0) {
    const double root = std::sqrt(discriminant);
    cuts[2] = std::clamp((-linear - root) / quadratic, span.BeginKm, span.EndKm);
    cuts[3] = std::clamp((-linear + root) / quadratic, span.BeginKm, span.EndKm);
  }
  if (ray.ViewSun != 0) {
    cuts[4] = std::clamp(-sunProjection / ray.ViewSun, span.BeginKm, span.EndKm);
  }
  std::ranges::sort(cuts);
  double result = 0;
  for (size_t index = 1; index < cuts.size(); ++index) {
    const double middle = (cuts[index - 1] + cuts[index]) / 2;
    const double toSun = sunProjection + middle * ray.ViewSun;
    const double radiusSquared =
        ray.RadiusKm * ray.RadiusKm + 2 * middle * viewProjection + middle * middle;
    const double outsideShadow = radiusSquared - toSun * toSun -
                                 static_cast<double>(air.BottomRadiusKm) * air.BottomRadiusKm;
    if (toSun <= 0 && outsideShadow <= 0) { continue; }
    const double distance = cuts[index] - cuts[index - 1];
    const double local = sigma == 0 ? distance : -std::expm1(-sigma * distance) / sigma;
    result += std::exp(-sigma * (cuts[index - 1] - span.BeginKm)) * local;
  }
  return result;
}

Pixel ReferencePixel(const Scenario &scenario, const Medium &air, uint32_t x, uint32_t y) {
  if (!InViewport(scenario, size_t{y} * kWidth + x)) { return {}; }
  Pixel result = ScenePixel(x, y);
  if (x % 7 == 0 || scenario.AlongViewM == 0) { return result; }
  const Mat4f projection = scenario.Projection.Projection();
  const float clipDepth = (projection[14] - projection[10] * scenario.AlongViewM) /
                          (projection[15] - projection[11] * scenario.AlongViewM);
  const float written =
      scenario.Area.MinDepth + (scenario.Area.MaxDepth - scenario.Area.MinDepth) * clipDepth;
  const auto ray = ReferenceRay(scenario, Vec3{{x + 0.5, y + 0.5, written}}, air);
  const auto span = PhysicalAirSpan(ray, air);
  const double distanceKm = span.EndKm - span.BeginKm;
  const double g = air.MiePhaseG;
  const double cosine = ray.ViewSun;
  const double pi = std::acos(-1.0);
  const double phaseR = 3 * (1 + cosine * cosine) / (16 * pi);
  const double phaseM = 3 / (8 * pi) * (1 - g * g) / (2 + g * g) * (1 + cosine * cosine) /
                        std::pow(1 + g * g - 2 * g * cosine, 1.5);
  for (size_t channel = 0; channel < 3; ++channel) {
    const double scatterR = air.RayleighScatteringPerKm[channel];
    const double scatterM = air.MieScatteringPerKm;
    const double sigma = scatterR + air.MieExtinctionPerKm;
    const double directSource = kSolar[channel] * (scatterR * phaseR + scatterM * phaseM);
    const double indirectSource = (scatterR + scatterM) * kPsi[channel];
    const double integral = sigma == 0 ? distanceKm : -std::expm1(-sigma * distanceKm) / sigma;
    const double light =
        directSource * LitIntegral(ray, air, span, sigma) + indirectSource * integral;
    result[channel] = static_cast<float>(result[channel] * std::exp(-sigma * distanceKm) +
                                         scenario.Sky.IlluminanceLux * light);
  }
  return result;
}

void CheckOpaque(const Scenario &scenario,
                 const Medium &air,
                 std::span<const Pixel> pixels,
                 std::ofstream &actual,
                 std::ofstream &reference) {
  size_t rgbFailures = 0;
  size_t alphaFailures = 0;
  size_t backgroundFailures = 0;
  double worst = 0;
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const auto &pixel = pixels[size_t{y} * kWidth + x];
      const auto expected = ReferencePixel(scenario, air, x, y);
      actual.write(reinterpret_cast<const char *>(pixel.data()), sizeof(Pixel));
      reference.write(reinterpret_cast<const char *>(expected.data()), sizeof(Pixel));
      for (size_t channel = 0; channel < 3; ++channel) {
        const double error = std::abs(static_cast<double>(pixel[channel]) - expected[channel]);
        const double tolerance =
            std::max(kAbsoluteTolerance, std::abs(expected[channel]) * kRelativeTolerance);
        if (!std::isfinite(pixel[channel]) || error > tolerance) { ++rgbFailures; }
        worst = std::max(worst, error);
      }
      if (pixel[3] != expected[3]) { ++alphaFailures; }
      if ((x % 7 == 0 || !InViewport(scenario, size_t{y} * kWidth + x)) && pixel != expected) {
        ++backgroundFailures;
      }
    }
  }
  Note("maximum_opaque_rgb_error", worst, "linear radiance");
  CHECK(rgbFailures == 0,
        "native opaque transport matches independent homogeneous radiative transfer");
  CHECK(alphaFailures == 0, "opaque medium preserves scene alpha exactly");
  CHECK(backgroundFailures == 0, "background and outside-viewport pixels are unchanged");
}
}

void Opaque(Device &device, const GraphicsPipeline &pipeline, const char *output) {
  const Sources sources(device);
  std::ofstream actual(output, std::ios::binary);
  std::ofstream reference(std::string(output) + ".reference", std::ios::binary);
  const auto air = Homogeneous();
  for (auto scenario : Scenarios()) {
    for (const bool day : {true, false}) {
      if (!day) { scenario.Sky.SunDir = scenario.Sky.SunDir * -1.0f; }
      const auto pixels = sources.Draw(device, pipeline, scenario, air);
      CheckOpaque(scenario, air, pixels, actual, reference);
    }
  }
  for (int edge = 0; edge < 3; ++edge) {
    auto scenario = Scenarios().front();
    auto medium = air;
    if (edge == 0) { scenario.Sky.IlluminanceLux = 0; }
    if (edge == 1) {
      medium.RayleighScatteringPerKm = {};
      medium.MieScatteringPerKm = medium.MieExtinctionPerKm = 0;
    }
    if (edge == 2) {
      scenario.Projection.OrthoWidthM = kOrthoWidthM;
      scenario.Projection.OrthoM = kOrthoHeightM;
      scenario.Projection.NearM = 0;
      scenario.AlongViewM = 0;
    }
    const auto pixels = sources.Draw(device, pipeline, scenario, medium);
    CheckOpaque(scenario, medium, pixels, actual, reference);
  }
  actual.flush();
  reference.flush();
  CHECK(actual && reference, "all actual native opaque and independent reference frames are saved");
}
}
