#include "CloudViewProbe.h"
#include "LowCloud.h"
#include "MediumViewProbe.h"
#include "NativeGpuCheck.h"
#include "WeatherCapture.h"
#include "Check.h"

#include <cmath>
#include <cstddef>
#include <optional>
#include <utility>
#include <vector>
#include <array>

namespace outshine::Test::CloudViewProbe {
namespace {
constexpr float kControlScaleHeightKm = 1e20f;
constexpr float kFovDeg = 55.0f;
constexpr float kFarM = 6000.0f;
constexpr float kOrthoWidthM = 2600.0f;
constexpr float kOrthoHeightM = 1200.0f;
constexpr double kEyeHeightM = 1772.0;
constexpr double kPitchSin = 0.6;
constexpr double kPitchCos = 0.8;
constexpr Vec2f kJitter{{0.375f, -0.25f}};
constexpr Vec3 kOffsetCameraM{{4200.0, kEyeHeightM, -3100.0}};
constexpr Viewport kInset{
    .X = 4.25f, .Y = 3.75f, .Width = 29.0f, .Height = 21.0f, .MinDepth = 0.2f, .MaxDepth = 0.85f};
constexpr double kMotionStepS = 1.0 / 30.0;
constexpr double kCloudEastAdvectionMs = 6.92820323027551;
constexpr Vec4f kCloudSun{{1.0f, 0.97f, 0.92f, 0.0f}};
constexpr Vec4f kCloudSky{{0.1f, 0.12f, 0.16f, 0.0f}};
constexpr std::array kHeightsM{2800.0, 5000.0, 110000.0};
constexpr std::array kHeightNames{"inside", "above", "orbit"};

void SetSky(Case &value) {
  const double radiusM = ControlledAir().BottomRadiusKm * 1000.0;
  const auto position = value.DensityCameraOriginM + Vec3{{0.0, radiusM, 0.0}};
  const auto radial = position * (1.0 / Length(position));
  value.View.Sky = {
      .SunDir = {{-static_cast<float>(kPitchSin), static_cast<float>(kPitchCos), 0.0f}},
      .WorldUp = {{static_cast<float>(radial[0]),
                   static_cast<float>(radial[1]),
                   static_cast<float>(radial[2])}},
      .IlluminanceLux = 1.0f,
      .EyeHeightM = static_cast<float>(CloudAltitude(value.DensityCameraOriginM, radiusM))};
}

Case BaseCase(MediumViewProbe::ImageShape shape) {
  Case value{.Name = "declared-perspective",
             .View = {.Projection = {.WidePx = static_cast<double>(shape.Width),
                                     .HighPx = static_cast<double>(shape.Height),
                                     .FovDeg = kFovDeg,
                                     .NearM = 1.0f,
                                     .FarM = kFarM},
                      .Basis = {.Forward = {{0.0, kPitchSin, -kPitchCos}},
                                .Right = {{1.0, 0.0, 0.0}},
                                .Up = {{0.0, kPitchCos, kPitchSin}}},
                      .Sky = {},
                      .Area = {.Width = static_cast<float>(shape.Width),
                               .Height = static_cast<float>(shape.Height)},
                      .AlongViewM = 0.0f},
             .Snapshot = {},
             .DensityCameraOriginM = {{0.0, kEyeHeightM, 0.0}}};
  value.Snapshot.Values.ValidUtcS = kCapturedWeatherUtcS;
  value.Snapshot.Values.CloudLow = kDeclaredLowCover;
  value.Snapshot.Source.Origin = WeatherOrigin::ScenarioDeclaration;
  value.Snapshot.Source.SourceId = "declared-low-cloud-front";
  SetSky(value);
  return value;
}
}

Medium ControlledAir() {
  auto air = kEarthAir;
  air.RayleighScaleHeightKm = air.MieScaleHeightKm = kControlScaleHeightKm;
  air.OzoneAbsorptionPerKm = {};
  return air;
}

std::vector<Case> Cases(MediumViewProbe::ImageShape shape) {
  auto base = BaseCase(shape);
  auto clear = base;
  const auto capture = RecordedWeatherCapture();
  CHECK(capture, "the unchanged real forecast capture is available before rendering");
  if (!capture) { Gpu::Failed(); }
  const Generators::Weather::OpenMeteoProvider provider(capture->Request);
  auto configured = provider.make(capture->Declaration, {});
  CHECK(configured, "the public native source configures without IO");
  if (!configured) { Gpu::Failed(); }
  auto decoded = (*configured)->DecodeWeather(WeatherBytes(*capture), kCapturedWeatherUtcS);
  CHECK(decoded && *decoded && (**decoded).Values.CloudLow == 0.0,
        "the actual public snapshot keeps Koerbersee's known-zero low cover");
  if (!decoded || !*decoded) { Gpu::Failed(); }
  clear.Snapshot = std::move(**decoded);
  clear.Name = "captured-clear";
  std::vector<Case> result{clear, base};
  for (const bool ortho : {false, true}) {
    auto value = base;
    if (ortho) {
      value.View.Projection.FovDeg = 0.0f;
      value.View.Projection.OrthoWidthM = kOrthoWidthM;
      value.View.Projection.OrthoM = kOrthoHeightM;
    }
    value.Name = ortho ? "declared-ortho" : "perspective-jitter";
    value.View.Projection.Jitter = kJitter;
    result.push_back(value);
    value.Name = ortho ? "ortho-offset" : "perspective-offset";
    const auto width = static_cast<float>(shape.Width);
    const auto height = static_cast<float>(shape.Height);
    const auto referenceWidth = static_cast<float>(MediumViewProbe::kWidth);
    const auto referenceHeight = static_cast<float>(MediumViewProbe::kHeight);
    value.View.Area = {.X = width * kInset.X / referenceWidth,
                       .Y = height * kInset.Y / referenceHeight,
                       .Width = width * kInset.Width / referenceWidth,
                       .Height = height * kInset.Height / referenceHeight,
                       .MinDepth = kInset.MinDepth,
                       .MaxDepth = kInset.MaxDepth};
    value.DensityCameraOriginM = kOffsetCameraM;
    SetSky(value);
    result.push_back(value);
  }
  for (size_t index = 0; index < kHeightsM.size(); ++index) {
    const double heightM = kHeightsM[index];
    auto value = base;
    value.Name = kHeightNames[index];
    value.DensityCameraOriginM[1] = heightM;
    if (heightM > kEyeHeightM) {
      value.View.Basis.Up = {{0.0, kPitchCos, -kPitchSin}};
      value.View.Basis.Forward = {{0.0, -kPitchSin, -kPitchCos}};
    }
    SetSky(value);
    result.push_back(value);
  }
  auto rotated = base;
  rotated.Name = "yaw-rotation";
  rotated.View.Basis.Right = {{0.0, 0.0, -1.0}};
  rotated.View.Basis.Up = {{kPitchSin, kPitchCos, 0.0}};
  rotated.View.Basis.Forward = {{-kPitchCos, kPitchSin, 0.0}};
  result.push_back(rotated);
  auto wind = base;
  wind.Name = "wind-advance";
  wind.PhaseS += kMotionStepS;
  result.push_back(wind);
  auto translated = base;
  translated.Name = "translation";
  translated.DensityCameraOriginM[0] = 1.0;
  SetSky(translated);
  result.push_back(translated);
  base.Name = "wind-motion";
  base.DensityCameraOriginM[0] = 1.0;
  base.PhaseS += kMotionStepS;
  SetSky(base);
  result.push_back(base);
  return result;
}

WeatherFrame PrepareWeather(const Case &scenario) {
  using namespace Generators::Weather;
  const auto state = MakeLowCloud(scenario.Snapshot,
                                  LowCloudRecipe{.BaseAslM = kDeclaredCloudBaseAslM,
                                                 .ThicknessM = kDeclaredCloudThicknessM,
                                                 .ExtinctionPerM = kDeclaredCloudExtinctionPerM,
                                                 .AdvectionEastMs = kCloudEastAdvectionMs,
                                                 .AdvectionNorthMs = 4.0,
                                                 .Profile = scenario.Profile});
  CHECK(state && *state, "the selected native weather snapshot produces the cloud consumer state");
  if (!state || !*state) { Gpu::Failed(); }
  const auto &layer = (**state).Layer;
  const auto frame = MakeCloudFrame(layer, scenario.PhaseS, scenario.FieldOriginM);
  CHECK(frame, "the production C++ frame producer preserves the requested wind and field anchor");
  if (!frame) { Gpu::Failed(); }
  return {.Layer = *frame,
          .DensityCameraOriginM = {{static_cast<float>(scenario.DensityCameraOriginM[0]),
                                    static_cast<float>(scenario.DensityCameraOriginM[1]),
                                    static_cast<float>(scenario.DensityCameraOriginM[2]),
                                    0.0f}},
          .SunIrradiance = kCloudSun,
          .SkyRadiance = kCloudSky};
}
}
