#include "MediumView.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kNdcTolerance = 2e-6;
constexpr double kDepthToleranceM = 0.01;
constexpr double kHeightM = 1250;
constexpr double kRadiusToleranceKm = 0.0005;
constexpr double kOrthoExtentToleranceM = 0.0001;
constexpr Native::Viewport kFullViewport{.Width = 1280, .Height = 720};
constexpr Native::Viewport kInsetViewport{
    .X = 11.25f, .Y = 7.5f, .Width = 107, .Height = 63, .MinDepth = 0.2f, .MaxDepth = 0.85f};

void CheckProjection(Lens lens, Native::Viewport area) {
  const CameraBasis basis;
  const SkyStanding sky{
      .SunDir = {{0, 1, 0}}, .WorldUp = {{0, 1, 0}}, .IlluminanceLux = 4, .EyeHeightM = kHeightM};
  const auto frame = MakeMediumViewParameters(lens, area, basis, sky, kEarthAir);
  CHECK(frame, "valid lens and actual viewport produce a medium frame");
  if (!frame) { return; }
  CHECK_NEAR(frame->EyeRadiusKm,
             kEarthAir.BottomRadiusKm + kHeightM / 1000,
             kRadiusToleranceKm,
             "km",
             "physical eye height receives no artificial ground lift");
  const Mat4f projection = lens.Projection();
  const bool ortho = lens.OrthoM > 0;
  for (const double alongM : {1.0, 25.0, 250.0}) {
    for (const double x : {-0.7, 0.0, 0.6}) {
      for (const double y : {-0.5, 0.2, 0.8}) {
        const double viewX = x / projection[0] * (ortho ? 1 : alongM);
        const double viewY = y / projection[5] * (ortho ? 1 : alongM);
        const double clipW = projection[15] - projection[11] * alongM;
        const double clipX = projection[0] * viewX - projection[8] * alongM + projection[12];
        const double clipY = projection[5] * viewY - projection[9] * alongM + projection[13];
        const double clipZ = projection[14] - projection[10] * alongM;
        const double pixelX = area.X + (clipX / clipW + 1) * area.Width / 2;
        const double pixelY = area.Y + (1 - clipY / clipW) * area.Height / 2;
        const double written = area.MinDepth + (area.MaxDepth - area.MinDepth) * clipZ / clipW;
        CHECK_NEAR(pixelX * frame->PixelToNdc[0] + frame->PixelToNdc[2],
                   x,
                   kNdcTolerance,
                   "NDC",
                   "actual projection, viewport offset and X jitter invert");
        CHECK_NEAR(pixelY * frame->PixelToNdc[1] + frame->PixelToNdc[3],
                   y,
                   kNdcTolerance,
                   "NDC",
                   "native top origin and Y jitter invert");
        const auto &depth = frame->DepthReconstruction;
        const double actualM = (depth[0] - written * depth[1]) / (depth[2] + written * depth[3]);
        CHECK_NEAR(actualM,
                   alongM,
                   kDepthToleranceM,
                   "m",
                   "reversed depth and the actual window depth range reconstruct view distance");
      }
    }
  }
  CHECK((frame->OrthoProjection[2] > 0) == ortho, "the shader receives the actual projection kind");
  if (ortho) {
    CHECK_NEAR(frame->OrthoProjection[0],
               lens.OrthoWidthM / 2,
               kOrthoExtentToleranceM,
               "m",
               "Lens full width becomes the explicit orthographic half extent");
    CHECK_NEAR(frame->OrthoProjection[1],
               lens.OrthoM / 2,
               kOrthoExtentToleranceM,
               "m",
               "Lens full height becomes the explicit orthographic half extent");
  }
}

void Rejected() {
  const Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 60, .NearM = 1, .FarM = 1000};
  const CameraBasis basis;
  const SkyStanding sky{.SunDir = {{0, 1, 0}}, .WorldUp = {{0, 1, 0}}};
  const Native::Viewport area{.Width = 1280, .Height = 720};
  for (const auto bad :
       {Native::Viewport{},
        Native::Viewport{.Width = -1, .Height = 10},
        Native::Viewport{.Width = 10, .Height = 10, .MinDepth = 1, .MaxDepth = 1}}) {
    const auto result = MakeMediumViewParameters(lens, bad, basis, sky, kEarthAir);
    CHECK(!result && result.error() == MediumViewError::InvalidViewport,
          "invalid and zero depth-range viewports fail before division");
  }
  auto nonorthogonal = basis;
  nonorthogonal.Right = basis.Up;
  CHECK(!MakeMediumViewParameters(lens, area, nonorthogonal, sky, kEarthAir),
        "a nonorthogonal camera cannot silently produce a different physical ray");
  auto invalidAir = kEarthAir;
  invalidAir.MieExtinctionPerKm = std::numeric_limits<float>::quiet_NaN();
  CHECK(!MakeMediumViewParameters(lens, area, basis, sky, invalidAir),
        "nonfinite medium parameters do not enter a GPU frame");
  CHECK(!MakeMediumViewParameters(Lens{}, area, basis, sky, kEarthAir),
        "an empty lens fails before projection assertions or division");
}
}

int main() {
  for (const bool ortho : {false, true}) {
    for (const Vec2f jitter : {Vec2f{}, Vec2f{{0.375f, -0.25f}}}) {
      for (const float farM : {1000.0f, 0.0f}) {
        if (ortho && farM == 0) { continue; }
        const Lens lens{.WidePx = 1280,
                        .HighPx = 720,
                        .FovDeg = ortho ? 0.0f : 75.0f,
                        .OrthoWidthM = ortho ? 400.0f : 0.0f,
                        .OrthoM = ortho ? 200.0f : 0.0f,
                        .NearM = 1,
                        .FarM = farM,
                        .Jitter = jitter};
        CheckProjection(lens, kFullViewport);
        CheckProjection(lens, kInsetViewport);
      }
    }
  }
  Rejected();
  return outshine::Test::Report();
}
