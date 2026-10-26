#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "Check.h"
#include <vector>
#include <optional>
#include <utility>
#include <ios>
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
constexpr size_t kTargets = 3;
constexpr float kClear = -9;
constexpr double kRadiusToleranceKm = 0.001, kCosineTolerance = 2e-6;
constexpr double kDistanceToleranceKm = 2e-6, kOriginToleranceM = 1e-4;

struct Targets {
  std::array<std::unique_ptr<Texture>, kTargets> Images;
  std::array<std::unique_ptr<Buffer>, kTargets> Readbacks;

  explicit Targets(Device &device) {
    for (size_t i = 0; i < kTargets; ++i) {
      Images[i] = Require(
          device.CreateTexture({.Width = kWidth,
                                .Height = kHeight,
                                .Texels = Format::Rgba32Float,
                                .Use = TextureUse::ColourAttachment | TextureUse::CopySource}));
      Readbacks[i] = Require(device.CreateBuffer({.Bytes = uint64_t{kRowBytes} * kHeight,
                                                  .Use = BufferUse::CopyDestination,
                                                  .Placement = Memory::Readback}));
    }
  }

  std::array<std::vector<Pixel>, kTargets>
  Draw(Device &device, const GraphicsPipeline &pipeline, const Scenario &scenario) const {
    auto frame = FrameFor(scenario, kEarthAir);
    const Mat4f projection = scenario.Projection.Projection();
    const Vec4f clip{{(projection[14] - projection[10] * scenario.AlongViewM) /
                          (projection[15] - projection[11] * scenario.AlongViewM),
                      0,
                      0,
                      0}};
    const std::array bindings{
        Binding{.Set = 1, .Slot = 0, .Value = Uniform{std::as_bytes(std::span(&clip, 1))}},
        Binding{.Set = 3, .Slot = 2, .Value = Uniform{std::as_bytes(std::span(&frame, 1))}}};
    std::array<ColourAttachment, kTargets> colours;
    for (size_t i = 0; i < kTargets; ++i) {
      colours[i] = {.Image = {.Image = Images[i].get()},
                    .Begin = Load::Clear,
                    .Clear = {kClear, kClear, kClear, kClear}};
    }
    auto commands = Require(device.Begin());
    Require(commands->BeginRenderPass({.Colours = colours, .Depth = std::nullopt}));
    Require(commands->Draw({.State = {.Pipeline = &pipeline,
                                      .Bindings = bindings,
                                      .Vertices = {},
                                      .Area = scenario.Area,
                                      .Clip = ViewClip(scenario)},
                            .Range = {.Count = 3}}));
    frame.PixelToNdc = {};
    Require(commands->EndRenderPass());
    for (size_t i = 0; i < kTargets; ++i) {
      Require(
          commands->Copy(*Images[i],
                         *Readbacks[i],
                         {.RowBytes = kRowBytes, .Region = {.Width = kWidth, .Height = kHeight}}));
    }
    Finish(device, std::move(commands));
    std::array<std::vector<Pixel>, kTargets> pixels;
    for (size_t i = 0; i < kTargets; ++i) { pixels[i] = ReadImage(device, *Readbacks[i]); }
    return pixels;
  }
};

void CheckDepthInvariant(const std::array<std::vector<Pixel>, kTargets> &first,
                         const std::array<std::vector<Pixel>, kTargets> &second) {
  size_t changed = 0;
  for (size_t target = 0; target < kTargets; ++target) {
    for (size_t pixel = 0; pixel < first[target].size(); ++pixel) {
      for (size_t channel = 0; channel < 4; ++channel) {
        if ((target == 0 || target == 2) && channel == 3) { continue; }
        if (first[target][pixel][channel] != second[target][pixel][channel]) { ++changed; }
      }
    }
  }
  CHECK(changed == 0,
        "changing fragment depth preserves physical origin/direction/cosines bitwise");
}

size_t CountMismatches(const Pixel &actual,
                       const std::array<double, 4> &expected,
                       const std::array<double, 4> &tolerances) {
  size_t failures = 0;
  for (size_t channel = 0; channel < 4; ++channel) {
    const double value = actual[channel];
    if (!std::isfinite(value) || std::abs(value - expected[channel]) > tolerances[channel]) {
      ++failures;
    }
  }
  return failures;
}

size_t GeometryErrors(const Scenario &scenario,
                      uint32_t x,
                      uint32_t y,
                      const std::array<std::vector<Pixel>, kTargets> &pixels) {
  const size_t index = size_t{y} * kWidth + x;
  const auto ray = ReferenceRay(scenario, Vec3{{x + 0.5, y + 0.5, pixels[2][index][3]}}, kEarthAir);
  const std::array expected{std::array{ray.RadiusKm, ray.CosView, ray.CosSun, ray.DistanceKm},
                            std::array{ray.OriginM[0], ray.OriginM[1], ray.OriginM[2], ray.ViewSun},
                            std::array{ray.Direction[0],
                                       ray.Direction[1],
                                       ray.Direction[2],
                                       static_cast<double>(pixels[2][index][3])}};
  constexpr std::array tolerances{
      std::array{kRadiusToleranceKm, kCosineTolerance, kCosineTolerance, kDistanceToleranceKm},
      std::array{kOriginToleranceM, kOriginToleranceM, kOriginToleranceM, kCosineTolerance},
      std::array{kCosineTolerance, kCosineTolerance, kCosineTolerance, 0.0}};
  size_t failures = 0;
  for (size_t target = 0; target < kTargets; ++target) {
    failures += CountMismatches(pixels[target][index], expected[target], tolerances[target]);
  }
  return failures;
}

void CheckGeometry(const Scenario &scenario,
                   const std::array<std::vector<Pixel>, kTargets> &pixels,
                   std::ofstream &saved) {
  size_t failures = 0;
  size_t backgroundFailures = 0;
  const Pixel clear{kClear, kClear, kClear, kClear};
  for (uint32_t y = 0; y < kHeight; ++y) {
    for (uint32_t x = 0; x < kWidth; ++x) {
      const size_t index = size_t{y} * kWidth + x;
      for (const auto &target : pixels) {
        saved.write(reinterpret_cast<const char *>(target[index].data()), sizeof(Pixel));
      }
      if (!InViewport(scenario, index)) {
        for (const auto &target : pixels) { backgroundFailures += target[index] != clear ? 1 : 0; }
        continue;
      }
      failures += GeometryErrors(scenario, x, y, pixels);
    }
  }
  Note("geometry_mismatched_components", static_cast<double>(failures), "components");
  CHECK(failures == 0, "actual native fragment rays match independently projected world geometry");
  CHECK(backgroundFailures == 0,
        "the matching viewport scissor leaves outside MRT pixels unchanged");
}
}

void Geometry(Device &device, const GraphicsPipeline &pipeline, const char *output) {
  const Targets targets(device);
  std::ofstream saved(output, std::ios::binary);
  std::array<std::vector<Pixel>, kTargets> previous;
  size_t index = 0;
  for (const auto &scenario : GeometryScenarios()) {
    auto pixels = targets.Draw(device, pipeline, scenario);
    CheckGeometry(scenario, pixels, saved);
    if (index % 2 == 0) {
      previous = std::move(pixels);
    } else {
      CheckDepthInvariant(previous, pixels);
    }
    ++index;
  }
  saved.flush();
  CHECK(saved, "all actual native geometric MRT frames are saved");
}
}
