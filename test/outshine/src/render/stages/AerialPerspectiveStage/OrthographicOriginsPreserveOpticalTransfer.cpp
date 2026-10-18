#include "AerialPerspectiveStage.h"
#include "Check.h"
#include "MediumView.h"
#include "test/harness/render/NativeGpuCheck.h"
#include "StageSubmission.h"
#include "test/harness/render/NativeImageReadback.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Render::Native;
using namespace outshine::Test;
using namespace outshine::Test::Gpu;
constexpr uint32_t kSide = 9;
constexpr uint32_t kSkyWidth = 16, kSkyHeight = 8;
constexpr uint32_t kVeilWidth = 512, kVeilHeight = 64;
constexpr float kPlaneM = 200, kEyeM = 500, kSpanM = 600;
constexpr float kNearM = 1, kFarM = 1000, kPerspectiveDeg = 50;
constexpr float kHalfPixel = 0.5f, kQuarterPixel = 0.25f;
constexpr float kMinDepth = 0.2f, kMaxDepth = 0.8f, kAlpha = 0.375f;
constexpr double kKmPerM = 0.001, kHalfKmPerM = 0.0005;
constexpr double kEyeRadiusKm = 1.5, kRelativeLimit = 0.01;
constexpr auto kWait = std::chrono::seconds(5);
constexpr Format kColour = Format::Rgba32Float;

std::unique_ptr<Texture>
Image(Device &device, uint32_t width, uint32_t height, Format format = kColour) {
  return Require(
      device.CreateTexture({.Width = width,
                            .Height = height,
                            .Texels = format,
                            .Use = TextureUse::Sampled | TextureUse::ColourAttachment |
                                   TextureUse::CopyDestination | TextureUse::CopySource}));
}

void Upload(Device &device, Commands &commands, Texture &texture, std::span<const float> values) {
  auto buffer = Require(device.CreateBuffer(
      {.Bytes = values.size_bytes(), .Use = BufferUse::CopySource, .Placement = Memory::Upload}));
  const auto mapped = Require(device.MapWrite(*buffer, {.Bytes = values.size_bytes()}));
  std::memcpy(mapped.data(), values.data(), values.size_bytes());
  Require(device.Unmap(*buffer));
  const auto &shape = texture.Description();
  Require(commands.Copy(*buffer,
                        texture,
                        {.RowBytes = values.size_bytes() / shape.Height,
                         .Region = {.Width = shape.Width, .Height = shape.Height}}));
}

struct Comparison {
  double Relative = 0;
  size_t Pixels = 0;
  bool Finite = true, Alpha = true, Sky = true, Outside = true;
};

struct Pixel {
  size_t X;
  size_t Y;
};

// The expected line integral is in camera coordinates, without the reconstruction under test.
double Expected(const Lens &lens, Viewport area, Pixel pixel) {
  const double nx = 2 * (static_cast<double>(pixel.X) + 0.5 - area.X) / area.Width - 1 -
                    2 * lens.Jitter[0] / lens.WidePx;
  const double ny = 1 - 2 * (static_cast<double>(pixel.Y) + 0.5 - area.Y) / area.Height -
                    2 * lens.Jitter[1] / lens.HighPx;
  double distance = kPlaneM * kKmPerM;
  double square = kEyeRadiusKm * kEyeRadiusKm;
  double along = 0;
  if (lens.OrthoM > 0) {
    const double ox = nx * lens.OrthoWidthM * kHalfKmPerM;
    const double oy = ny * lens.OrthoM * kHalfKmPerM;
    square = ox * ox + (kEyeRadiusKm + oy) * (kEyeRadiusKm + oy);
  } else {
    constexpr double kHalfTurnDeg = 360;
    const double half = std::tan(lens.FovDeg * std::acos(-1.0) / kHalfTurnDeg);
    const double dx = nx * half * lens.WidePx / lens.HighPx;
    const double dy = ny * half;
    const double norm = std::sqrt(1 + dx * dx + dy * dy);
    distance *= norm;
    along = kEyeRadiusKm * dy / norm;
  }
  return std::exp(
      -(square * distance + along * distance * distance + distance * distance * distance / 3));
}

Comparison Compare(std::span<const float> values, const Lens &lens, Viewport area, bool hole) {
  Comparison result;
  for (size_t y = 0; y < kSide; ++y) {
    for (size_t x = 0; x < kSide; ++x) {
      const size_t at = (y * kSide + x) * 4;
      const auto pixelX = static_cast<double>(x);
      const auto pixelY = static_cast<double>(y);
      const bool inside = pixelX >= area.X && pixelY >= area.Y && pixelX < area.X + area.Width &&
                          pixelY < area.Y + area.Height;
      if (!inside) {
        for (size_t c = 0; c < 4; ++c) { result.Outside &= values[at + c] == 0; }
        continue;
      }
      result.Alpha &= values[at + 3] == (hole ? kAlpha : 1);
      if (hole && pixelX == area.X && pixelY == area.Y) {
        for (size_t c = 0; c < 3; ++c) { result.Sky &= values[at + c] == 1; }
        continue;
      }
      ++result.Pixels;
      const double expected = Expected(lens, area, {.X = x, .Y = y});
      for (size_t c = 0; c < 3; ++c) {
        result.Finite &= std::isfinite(values[at + c]);
        result.Relative = std::max(result.Relative, std::abs(values[at + c] / expected - 1));
      }
    }
  }
  return result;
}

struct Fixture {
  Device &Gpu;
  Medium Air = kEarthAir;
  std::unique_ptr<Texture> Scene, Depth, Sky, Veil, Output;
  std::unique_ptr<Sampler> Exact, Linear;
  AerialPerspectiveStage Stage;

  explicit Fixture(Device &device)
      : Gpu(device),
        Scene(Image(device, kSide, kSide)),
        Depth(Image(device, kSide, kSide, Format::R32Float)),
        Sky(Image(device, kSkyWidth, kSkyHeight)),
        Veil(Image(device, kVeilWidth, kVeilHeight)),
        Output(Image(device, kSide, kSide)) {
    Air.BottomRadiusKm = 1;
    Air.TopRadiusKm = 2;
    Exact = Require(device.CreateSampler(
        {.Min = Filter::Nearest, .Mag = Filter::Nearest, .Mip = Filter::Nearest}));
    Linear = Require(device.CreateSampler(
        {.Min = Filter::Linear, .Mag = Filter::Linear, .Mip = Filter::Nearest}));
    Tables();
    Require(Stage.Configure({.Device = &Gpu, .SceneColours = {}},
                            {.Scene = Scene.get(),
                             .Depth = Depth.get(),
                             .SkyView = Sky.get(),
                             .Transmittance = Veil.get(),
                             .Exact = Exact.get(),
                             .Lut = Linear.get()},
                            kColour));
  }

  // Positive test extinction sigma(r)=r² has a closed-form integral to the sphere top.
  void Tables() {
    std::vector<float> solar(size_t{kVeilWidth} * kVeilHeight * 4u, 1);
    for (uint32_t y = 0; y < kVeilHeight; ++y) {
      for (uint32_t x = 0; x < kVeilWidth; ++x) {
        const auto look = medium_core::mediumTransmittanceTexel(
            Air,
            {.U = (static_cast<float>(x) + kHalfPixel) / kVeilWidth,
             .V = (static_cast<float>(y) + kHalfPixel) / kVeilHeight},
            {.WidthPx = kVeilWidth, .HeightPx = kVeilHeight});
        const double radius = look.RadiusKm;
        const double cosine = look.CosZenith;
        const double span =
            -radius * cosine + std::sqrt(4 - radius * radius * (1 - cosine * cosine));
        const double tau =
            radius * radius * span + radius * cosine * span * span + span * span * span / 3;
        const size_t at = (size_t{y} * kVeilWidth + x) * 4u;
        for (size_t c = 0; c < 3; ++c) { solar[at + c] = static_cast<float>(std::exp(-tau)); }
      }
    }
    auto commands = Require(Gpu.Begin());
    Upload(Gpu, *commands, *Sky, std::vector<float>(size_t{kSkyWidth} * kSkyHeight * 4u));
    Upload(Gpu, *commands, *Veil, solar);
    const auto done = Require(Gpu.Submit(std::move(commands)));
    CHECK(Require(Gpu.Wait(done, kWait)) == CompletionStatus::Ready, "analytic field uploads");
  }

  void Inputs(const Lens &lens, Viewport area, bool hole) {
    std::vector<float> scene(size_t{kSide} * kSide * 4u, 1);
    const auto projection = lens.Projection();
    const float raw = lens.OrthoM > 0 ? projection[14] - kPlaneM * projection[10]
                                      : projection[14] / kPlaneM - projection[10];
    std::vector<float> depths(size_t{kSide} * kSide,
                              area.MinDepth + (area.MaxDepth - area.MinDepth) * raw);
    if (hole) {
      depths[static_cast<size_t>(area.Y) * kSide + static_cast<size_t>(area.X)] = 0;
      for (size_t at = 3; at < scene.size(); at += 4) { scene[at] = kAlpha; }
    }
    auto commands = Require(Gpu.Begin());
    Upload(Gpu, *commands, *Scene, scene);
    Upload(Gpu, *commands, *Depth, depths);
    const auto done = Require(Gpu.Submit(std::move(commands)));
    CHECK(Require(Gpu.Wait(done, kWait)) == CompletionStatus::Ready,
          "physical plane inputs upload");
  }

  Comparison Render(const Lens &lens, Viewport area, bool rotate, bool hole, bool omitOrigins) {
    Inputs(lens, area, hole);
    CameraBasis basis;
    Vec3f up{{0, 1, 0}};
    if (rotate) {
      basis.Up = {{0, 0, 1}};
      basis.Forward = {{0, 1, 0}};
      up = {{0, 0, 1}};
    }
    auto view = MakeMediumViewParameters(
        lens,
        area,
        basis,
        {.SunDir = {{1, 0, 0}}, .WorldUp = up, .IlluminanceLux = 1, .EyeHeightM = kEyeM},
        Air);
    CHECK(view.has_value(), "the real camera produces a valid shared frame");
    if (!view) { Failed(); }
    if (omitOrigins) { view->OrthoProjection[2] = 0; }
    auto commands = Require(Gpu.Begin());
    const std::array colours{
        ColourAttachment{.Image = {.Image = Output.get()}, .Begin = Load::Clear}};
    Require(commands->BeginRenderPass({.Colours = colours, .Depth = {}}));
    StageSubmission submission;
    Require(Stage.Encode(*view,
                         {.Commands = commands.get(),
                          .Submission = submission,
                          .Area = area,
                          .Clip = {.X = static_cast<uint32_t>(area.X),
                                   .Y = static_cast<uint32_t>(area.Y),
                                   .Width = static_cast<uint32_t>(area.Width),
                                   .Height = static_cast<uint32_t>(area.Height)}}));
    Require(commands->EndRenderPass());
    const auto done = Require(Gpu.Submit(std::move(commands)));
    submission.Commit();
    CHECK(Require(Gpu.Wait(done, kWait)) == CompletionStatus::Ready,
          "actual aerial pass completes");
    const auto read = ReadImage(Gpu, *Output, {.Width = kSide, .Height = kSide});
    std::vector<float> values(size_t{kSide} * kSide * 4u);
    for (size_t y = 0; y < kSide; ++y) {
      std::memcpy(values.data() + y * kSide * 4,
                  read.Rows.data() + y * read.RowBytes,
                  size_t{kSide} * 4 * sizeof(float));
    }
    return Compare(values, lens, area, hole);
  }
};

void CheckCase(
    Fixture &fixture, const Lens &lens, Viewport area, bool rotate = false, bool hole = false) {
  const auto result = fixture.Render(lens, area, rotate, hole, false);
  CHECK(result.Pixels > 0 && result.Finite && result.Relative <= kRelativeLimit,
        "each actual pixel matches its independent positive-field line integral");
  CHECK(result.Alpha && result.Sky && result.Outside,
        "alpha, eye-radiance sky and outside-scissor values stay exact");
}
}

int main() {
  auto device = Require(CreateDevice({.Validation = true}));
  Fixture fixture(*device);
  const Viewport full{.Width = kSide, .Height = kSide};
  Lens lens{.WidePx = kSide,
            .HighPx = kSide,
            .OrthoWidthM = kSpanM,
            .OrthoM = kSpanM,
            .NearM = kNearM,
            .FarM = kFarM};
  CheckCase(fixture, lens, full);
  const auto wrong = fixture.Render(lens, full, false, false, true);
  CHECK(wrong.Finite && wrong.Relative > kRelativeLimit && wrong.Alpha && wrong.Sky &&
            wrong.Outside,
        "dropping orthographic origins fails the same optical bound without an interface error");
  lens.Jitter = {{kHalfPixel, -kQuarterPixel}};
  CheckCase(fixture, lens, full);
  CheckCase(fixture, lens, full, true);
  CheckCase(fixture, lens, full, false, true);
  const Viewport inset{
      .X = 2, .Y = 1, .Width = 5, .Height = 7, .MinDepth = kMinDepth, .MaxDepth = kMaxDepth};
  lens.WidePx = inset.Width;
  lens.HighPx = inset.Height;
  CheckCase(fixture, lens, inset);
  lens.OrthoM = 0;
  lens.FovDeg = kPerspectiveDeg;
  CheckCase(fixture, lens, inset);
  lens.WidePx = kSide;
  lens.HighPx = kSide;
  lens.Jitter = {};
  CheckCase(fixture, lens, full);
  return Report();
}
