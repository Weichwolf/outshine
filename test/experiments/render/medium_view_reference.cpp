#include "MediumViewProbe.h"
#include <cstddef>
#include "NativeGpuCheck.h"
#include "Check.h"
#include <vector>
#include <cstdint>
#include <algorithm>
#include <array>
#include <cmath>

namespace outshine::Test::MediumViewProbe {

MediumViewParameters FrameFor(const Scenario &scenario, const Medium &medium) {
  auto frame = MakeMediumViewParameters(
      scenario.Projection, scenario.Area, scenario.Basis, scenario.Sky, medium);
  CHECK(frame, "actual camera/viewport inputs produce the native224 frame");
  if (!frame) { outshine::Test::Gpu::Failed(); }
  return *frame;
}

namespace {
constexpr float kGroundHeightM = 500;
constexpr float kFovDeg = 60;
constexpr float kNearSceneFarM = 1000;
constexpr float kOrthoWidthM = 360;
constexpr float kOrthoHeightM = 240;
constexpr Viewport kInset{
    .X = 4.25f, .Y = 3.75f, .Width = 29, .Height = 21, .MinDepth = 0.2f, .MaxDepth = 0.85f};
constexpr Vec2f kJitter{{0.375f, -0.25f}};

Scenario BaseScenario(bool ortho, bool rotated) {
  Scenario scenario{.Projection = {.WidePx = kWidth,
                                   .HighPx = kHeight,
                                   .FovDeg = ortho ? 0.0f : kFovDeg,
                                   .OrthoWidthM = ortho ? kOrthoWidthM : 0.0f,
                                   .OrthoM = ortho ? kOrthoHeightM : 0.0f,
                                   .NearM = 1,
                                   .FarM = kNearSceneFarM},
                    .Basis = {},
                    .Sky = {.SunDir = rotated ? Vec3f{{1, 0, 0}} : Vec3f{{0, 1, 0}},
                            .WorldUp = rotated ? Vec3f{{1, 0, 0}} : Vec3f{{0, 1, 0}},
                            .IlluminanceLux = 1,
                            .EyeHeightM = kGroundHeightM},
                    .Area = {.Width = kWidth, .Height = kHeight},
                    .AlongViewM = 0};
  if (rotated) {
    const double diagonal = std::sqrt(0.5);
    scenario.Basis.Right = {{diagonal, 0, -diagonal}};
    scenario.Basis.Up = {{0, 1, 0}};
    scenario.Basis.Forward = {{-diagonal, 0, -diagonal}};
  }
  return scenario;
}

void AppendViewPairs(std::vector<Scenario> &result, Scenario scenario) {
  const auto full = scenario.Area;
  for (const Vec2f jitter : {Vec2f{}, kJitter}) {
    for (const auto area : {full, kInset}) {
      for (const float alongM : {25.0f, 250.0f}) {
        scenario.Projection.Jitter = jitter;
        scenario.Area = area;
        scenario.AlongViewM = alongM;
        result.push_back(scenario);
      }
    }
  }
}

void AppendClippingPairs(std::vector<Scenario> &result) {
  constexpr float kFarM = 6000;
  constexpr float kHighEyeM = 2500;
  for (const bool ortho : {false, true}) {
    for (const float alongM : {800.0f, 3000.0f}) {
      auto scenario = BaseScenario(ortho, false);
      scenario.Projection.FarM = kFarM;
      scenario.Basis.Forward = {{0, -1, 0}};
      scenario.Basis.Up = {{0, 0, -1}};
      scenario.Sky.EyeHeightM = kHighEyeM;
      scenario.AlongViewM = alongM;
      result.push_back(scenario);
    }
  }
}

void AppendFarDepthPairs(std::vector<Scenario> &result) {
  constexpr float kFarM = 6000;
  constexpr float kFov = 55;
  constexpr double kPitchCos = 0.8;
  constexpr double kPitchSin = 0.6;
  for (const bool ortho : {false, true}) {
    for (const Vec2f jitter : {Vec2f{}, kJitter}) {
      auto scenario = BaseScenario(ortho, false);
      scenario.Projection.FovDeg = ortho ? 0.0f : kFov;
      scenario.Projection.FarM = kFarM;
      scenario.Projection.Jitter = jitter;
      scenario.Area = kInset;
      scenario.Basis.Up = {{0, kPitchCos, kPitchSin}};
      scenario.Basis.Forward = {{0, kPitchSin, -kPitchCos}};
      for (const float alongM : {4500.0f, 5000.0f}) {
        scenario.AlongViewM = alongM;
        result.push_back(scenario);
      }
    }
  }
}
}

std::vector<Scenario> Scenarios() {
  std::vector<Scenario> result;
  for (const bool ortho : {false, true}) {
    for (const bool rotated : {false, true}) {
      AppendViewPairs(result, BaseScenario(ortho, rotated));
    }
  }
  AppendClippingPairs(result);
  return result;
}

std::vector<Scenario> GeometryScenarios() {
  auto result = Scenarios();
  AppendFarDepthPairs(result);
  return result;
}

Scissor ViewClip(const Scenario &scenario) {
  const auto &area = scenario.Area;
  const auto left = static_cast<uint32_t>(std::ceil(area.X - 0.5));
  const auto top = static_cast<uint32_t>(std::ceil(area.Y - 0.5));
  const auto right = static_cast<uint32_t>(std::ceil(area.X + area.Width - 0.5));
  const auto bottom = static_cast<uint32_t>(std::ceil(area.Y + area.Height - 0.5));
  return {.X = left, .Y = top, .Width = right - left, .Height = bottom - top};
}

bool InViewport(const Scenario &scenario, size_t pixel) {
  const auto &area = scenario.Area;
  const size_t x = pixel % kWidth;
  const size_t y = pixel / kWidth;
  const double centreX = static_cast<double>(x) + 0.5;
  const double centreY = static_cast<double>(y) + 0.5;
  return centreX >= area.X && centreX < area.X + area.Width && centreY >= area.Y &&
         centreY < area.Y + area.Height;
}

GeometricRay ReferenceRay(const Scenario &scenario, Vec3 fragment, const Medium &medium) {
  const Mat4f projection = scenario.Projection.Projection();
  const bool ortho = scenario.Projection.OrthoM > 0;
  const auto &area = scenario.Area;
  const double clipX = 2 * (fragment[0] - area.X) / area.Width - 1;
  const double clipY = 1 - 2 * (fragment[1] - area.Y) / area.Height;
  const double clipZ = (fragment[2] - area.MinDepth) / (area.MaxDepth - area.MinDepth);
  const double viewZ =
      (clipZ * projection[15] - projection[14]) / (projection[10] - clipZ * projection[11]);
  const double viewW = projection[11] * viewZ + projection[15];
  const double viewX = (clipX * viewW - projection[8] * viewZ - projection[12]) / projection[0];
  const double viewY = (clipY * viewW - projection[9] * viewZ - projection[13]) / projection[5];
  const Vec3 pointM =
      scenario.Basis.Right * viewX + scenario.Basis.Up * viewY - scenario.Basis.Forward * viewZ;
  const Vec3 originM = ortho ? scenario.Basis.Right * viewX + scenario.Basis.Up * viewY : Vec3{};
  Vec3 direction = pointM - originM;
  const double distanceM = std::sqrt(Dot(direction, direction));
  direction = direction * (1 / distanceM);
  Vec3 radial{{scenario.Sky.WorldUp[0], scenario.Sky.WorldUp[1], scenario.Sky.WorldUp[2]}};
  radial = radial * (1 / std::sqrt(Dot(radial, radial)));
  const double cameraRadiusKm =
      medium.BottomRadiusKm + static_cast<double>(scenario.Sky.EyeHeightM) / 1000;
  constexpr double kKmPerM = 0.001;
  const Vec3 positionKm = radial * cameraRadiusKm + originM * kKmPerM;
  const double radiusKm = std::sqrt(Dot(positionKm, positionKm));
  Vec3 sun{{scenario.Sky.SunDir[0], scenario.Sky.SunDir[1], scenario.Sky.SunDir[2]}};
  sun = sun * (1 / std::sqrt(Dot(sun, sun)));
  return {.OriginM = originM,
          .Direction = direction,
          .RadiusKm = radiusKm,
          .CosView = Dot(direction, positionKm) / radiusKm,
          .CosSun = Dot(sun, positionKm) / radiusKm,
          .ViewSun = Dot(direction, sun),
          .DistanceKm = distanceM * kKmPerM};
}
}
