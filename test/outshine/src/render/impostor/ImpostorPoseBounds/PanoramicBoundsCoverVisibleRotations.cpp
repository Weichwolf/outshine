#include "ImpostorPoseBounds.h"
#include "RadianceSupport.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::array<long double, 4>
Clip(const Vec3 &point, const Mat4 &frame, const CameraBasis &camera, const Lens &lens) {
  std::array<long double, 3> relative{};
  for (size_t row = 0; row < 3; ++row) {
    relative[row] = static_cast<long double>(frame[12 + row]) - camera.EyeM[row];
    for (size_t col = 0; col < 3; ++col) {
      relative[row] += static_cast<long double>(frame.At(row, col)) * point[col];
    }
  }
  std::array<long double, 4> local{0, 0, 0, 1};
  const std::array basis{camera.Right, camera.Up, camera.Forward * -1.0};
  for (size_t row = 0; row < 3; ++row) {
    for (size_t col = 0; col < 3; ++col) { local[row] += basis[row][col] * relative[col]; }
  }
  std::array<long double, 4> result{};
  const auto projection = lens.Projection();
  for (size_t row = 0; row < 4; ++row) {
    for (size_t col = 0; col < 4; ++col) { result[row] += projection.At(row, col) * local[col]; }
  }
  return result;
}

bool Visible(const std::array<long double, 4> &clip) {
  return clip[3] > 0 && std::abs(clip[0]) <= clip[3] && std::abs(clip[1]) <= clip[3] &&
         clip[2] >= 0 && clip[2] <= clip[3];
}

struct PointPair {
  Vec3 Original, Flattened;
};

std::array<PointPair, 27> Samples(const RadiancePlane &plane,
                                  const RadiancePlane::Support &support) {
  std::array<PointPair, 27> points;
  size_t at = 0;
  for (const double x : {-1., 0., 1.}) {
    for (const double y : {-1., 0., 1.}) {
      for (const double z : {-2., 0., 2.}) {
        const Vec3 point = plane.Centre + (support.Capture.Right * x + support.Capture.Up * y +
                                           plane.TowardEye * z) *
                                              plane.HalfExtentM;
        const double height =
            Dot(point - support.Centre, support.Normal) / Dot(plane.TowardEye, support.Normal);
        points[at++] = {.Original = point, .Flattened = point - plane.TowardEye * height};
      }
    }
  }
  return points;
}

bool CheckProjection(const PointPair &point,
                     const Mat4 &frame,
                     const CameraBasis &camera,
                     const Lens &lens,
                     const ImpostorPoseBounds::Error &bound) {
  const auto original = Clip(point.Original, frame, camera, lens);
  const auto flattened = Clip(point.Flattened, frame, camera, lens);
  if (!Visible(original) && !Visible(flattened)) { return false; }
  CHECK(original[3] > 0 && flattened[3] > 0 && original[2] >= 0 && flattened[2] >= 0 &&
            original[2] <= original[3] && flattened[2] <= flattened[3],
        "any visible endpoint keeps its complete source/support domain depth-safe");
  long double error = 0;
  const std::array raster{lens.WidePx, lens.HighPx};
  for (size_t axis = 0; axis < 2; ++axis) {
    const auto displacement =
        std::abs(original[axis] / original[3] - flattened[axis] / flattened[3]) * raster[axis] / 2;
    CHECK(displacement <= bound.AxisUpperPx[axis],
          "the all-rotation axis bound includes the independent native projection");
    error += displacement;
  }
  CHECK(error <= bound.L1UpperPx,
        "the coupled all-rotation bound includes visible corners and interiors");
  return true;
}

size_t Rotations(const RadiancePlane &plane, const Mat4 &frame, Vec3 localEye) {
  const Lens lens{.WidePx = 1280,
                  .HighPx = 720,
                  .FovDeg = 70,
                  .NearM = .1f,
                  .FarM = 240000,
                  .Jitter = {{.375f, -.25f}}};
  const auto bound = ImpostorPoseBounds::From(plane);
  const auto support = plane.SupportFrame();
  CHECK(bound && support, "the complete fitted capture has valid metadata");
  if (!bound || !support) { return 0; }
  const auto eye = frame.TransformPoint(localEye);
  const auto panoramic = bound->ProjectPanoramic(eye, lens, frame);
  CHECK(panoramic && panoramic->DepthInside, "the fixed eye has a bounded panoramic domain");
  if (!panoramic || !panoramic->DepthInside) { return 0; }
  const auto points = Samples(plane, *support);
  size_t checked = 0;
  for (const double yaw : {-.7, -.4, 0., .4, .7, std::numbers::pi}) {
    for (const double pitch : {-.3, 0., .3}) {
      const Vec3 forward{
          {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)}};
      const auto camera = Viewpoint::LookAt({.EyeM = eye, .AimM = eye + forward}, .7);
      CHECK(camera, "every query rotation has an orthonormal camera");
      if (!camera) { continue; }
      for (const auto &point : points) {
        checked += CheckProjection(point, frame, *camera, lens, *panoramic) ? 1u : 0u;
      }
    }
  }
  return checked;
}

void Boundaries() {
  const auto bound = ImpostorPoseBounds::From({}, {{0, 0, 1}}, 10);
  CHECK(bound, "boundary cases bind one complete source domain");
  if (!bound) { return; }
  Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 70, .NearM = .1f, .FarM = 240000};
  CHECK(!bound->ProjectPanoramic({{0, 0, -100}}, lens), "backside support cannot gain authority");
  const auto near = bound->ProjectPanoramic({{0, 0, 30}}, lens);
  CHECK(near && !near->DepthInside, "a capture near the eye cannot admit every rotation");
  const auto far = bound->ProjectPanoramic({{0, 0, 240000}}, lens);
  CHECK(far && !far->DepthInside, "a capture crossing the far plane remains unqualified");
  Mat4 invalid;
  invalid[0] = 2;
  CHECK(!bound->ProjectPanoramic({{0, 0, 1000}}, lens, invalid),
        "an invalid World frame cannot enter the panoramic domain");
  lens.OrthoM = lens.OrthoWidthM = 100;
  CHECK(!bound->ProjectPanoramic({{0, 0, 1000}}, lens),
        "perspective bounds cannot silently qualify orthographic rendering");
}
}

int main() {
  size_t checked = 0;
  for (const Vec3 fit : {Vec3{}, Vec3{{.2, -.3, .1}}, Vec3{{0, 0, -2}}}) {
    const RadiancePlane plane{
        .Centre = {}, .TowardEye = {{0, 0, 1}}, .HalfExtentM = 30, .DepthCoefficients = fit};
    checked += Rotations(plane, {}, {{0, 0, 3000}});
    Mat4 frame;
    frame[0] = frame[5] = 0;
    frame[1] = 1;
    frame[4] = -1;
    frame.SetTranslation({{6000000, 4000000, -2000000}});
    checked += Rotations(plane, frame, {{50, -10, 64000}});
  }
  CHECK(checked > 1000, "independent projections exercise visible edge and interior rotations");
  Boundaries();
  return Report();
}
