#include "ImpostorPoseBounds.h"
#include "ImpostorViewFrame.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kHalfExtentM = 473;

Lens Optics(bool orthographic = false) {
  Lens result{.WidePx = 1280, .HighPx = 720, .FovDeg = 90, .NearM = .1f, .FarM = 1000000};
  if (orthographic) {
    result.OrthoM = 4000;
    result.OrthoWidthM = 8000;
  }
  return result;
}

std::array<long double, 2>
PointError(const Vec3 &point, const Vec3 &direction, const CameraBasis &camera, const Lens &lens) {
  const auto dot = [](const auto &a, const auto &b) {
    long double result = 0;
    for (size_t i = 0; i < 3; ++i) { result += static_cast<long double>(a[i]) * b[i]; }
    return result;
  };
  const long double height = dot(point, direction) / dot(direction, direction);
  const auto matrix = lens.Projection();
  std::array<std::array<long double, 4>, 2> clips{};
  for (size_t sample = 0; sample < 2; ++sample) {
    std::array<long double, 3> relative{};
    for (size_t axis = 0; axis < 3; ++axis) {
      relative[axis] = static_cast<long double>(point[axis]) - camera.EyeM[axis] -
                       (sample == 0 ? 0 : height * direction[axis]);
    }
    const std::array local{dot(relative, camera.Right),
                           dot(relative, camera.Up),
                           -dot(relative, camera.Forward),
                           1.0L};
    for (size_t row = 0; row < 4; ++row) {
      for (size_t column = 0; column < 4; ++column) {
        clips[sample][row] += matrix.At(row, column) * local[column];
      }
    }
  }
  return {std::abs((clips[0][0] / clips[0][3] - clips[1][0] / clips[1][3]) * lens.WidePx / 2),
          std::abs((clips[0][1] / clips[0][3] - clips[1][1] / clips[1][3]) * lens.HighPx / 2)};
}

void CheckSamples(const Vec3 &direction,
                  const ImpostorViewFrame &frame,
                  const CameraBasis &camera,
                  const Lens &lens,
                  const ImpostorPoseBounds::Error &error) {
  for (const double x : {-.5, 0.0, .5}) {
    for (const double y : {-.5, 0.0, .5}) {
      for (const double z : {-1.0, 0.0, 1.0}) {
        const Vec3 point = (frame.Right * x + frame.Up * y + direction * z) * kHalfExtentM;
        const auto actual = PointError(point, direction, camera, lens);
        CHECK(actual[0] <= error.AxisUpperPx[0] && actual[1] <= error.AxisUpperPx[1] &&
                  actual[0] + actual[1] <= error.L1UpperPx,
              "independent point projection is enclosed by the whole-domain bound");
      }
    }
  }
}

void Poses() {
  for (const Vec3 direction : {Vec3{{0, 0, 1}}, Vec3{{.5, std::sqrt(.5), .5}}}) {
    const auto hull = ImpostorPoseBounds::From({}, direction, kHalfExtentM);
    const auto frame = ImpostorViewFrame::From(direction);
    CHECK(hull && frame, "finite capture headers produce an oriented whole-domain hull");
    if (!hull || !frame) { continue; }
    double previous = std::numeric_limits<double>::infinity();
    for (const double distance : {2000.0, 32000.0, 64000.0, 240000.0}) {
      const auto view = Viewpoint::LookAt(
          {.EyeM = direction * distance + frame->Right * 30.0, .AimM = {}}, Vec3{{0, 1, 0}});
      CHECK(view, "rotated moving cameras remain finite");
      if (!view) { continue; }
      for (const bool orthographic : {false, true}) {
        auto lens = Optics(orthographic);
        lens.Jitter = {{.25f, -.125f}};
        const auto error = hull->Project(*view, lens);
        CHECK(error && error->DepthInside, "perspective and orthographic source depths are inside");
        if (!error) { continue; }
        if (!orthographic) {
          CHECK(error->L1UpperPx < previous, "distance reduces the allowed geometric displacement");
          previous = error->L1UpperPx;
        }
        CheckSamples(direction, *frame, *view, lens, *error);
      }
    }
  }
}

void Rejections() {
  const Vec3 toward{{0, 0, 1}};
  const auto hull = ImpostorPoseBounds::From({}, toward, 1);
  CHECK(hull, "a small complete capture is valid");
  if (!hull) { return; }
  auto camera = CameraBasis{.EyeM = {{0, 0, 10}}};
  auto lens = Optics();
  CHECK(hull->Project(camera, lens), "the finite positive pose is evaluated");
  camera.EyeM[2] = 1;
  CHECK(!hull->Project(camera, lens), "a complete hull crossing the eye retains originals");
  camera.EyeM[2] = -10;
  CHECK(!hull->Project(camera, lens), "back-facing support cannot become a replacement");
  camera.EyeM[2] = 10;
  lens.NearM = 9;
  const auto near = hull->Project(camera, lens);
  CHECK(near && !near->DepthInside,
        "near clipping remains inadmissible even for tiny displacement");
  lens = Optics();
  lens.FarM = 9;
  const auto far = hull->Project(camera, lens);
  CHECK(far && !far->DepthInside, "far clipping cannot inherit geometric admission");
  lens = Optics();
  lens.WidePx = 1280.5;
  CHECK(!hull->Project(camera, lens), "fractional raster extents are not silently rounded");
  lens = Optics();
  lens.Jitter[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!hull->Project(camera, lens), "nonfinite projection operands retain originals");
  camera.Right[0] = 2;
  CHECK(!hull->Project(camera, Optics()), "a malformed camera basis is rejected");
  CHECK(!ImpostorPoseBounds::From({}, {}, 1) && !ImpostorPoseBounds::From({}, toward, 0) &&
            !ImpostorPoseBounds::From({{0, 0, std::numeric_limits<double>::infinity()}}, toward, 1),
        "unknown capture operands are never interpreted as zero error");
  CHECK(!ImpostorPoseBounds::From({}, toward, std::numeric_limits<double>::max()),
        "overflow in a finite capture header is rejected");
}
}

int main() {
  Poses();
  Rejections();
  return Report();
}
