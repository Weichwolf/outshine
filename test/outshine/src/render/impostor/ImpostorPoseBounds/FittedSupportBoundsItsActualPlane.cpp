#include "Check.h"
#include "ImpostorPoseBounds.h"
#include "ImpostorViewFrame.h"
#include "RadianceSupport.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string_view>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr double kExtent = 473;

Lens Optics(bool parallel) {
  Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 90, .NearM = .1f, .FarM = 1000000};
  if (parallel) {
    lens.OrthoM = 4000;
    lens.OrthoWidthM = 8000;
  }
  return lens;
}

long double DotExact(const auto &a, const Vec3 &b) {
  long double sum = 0;
  for (size_t axis = 0; axis < 3; ++axis) { sum += static_cast<long double>(a[axis]) * b[axis]; }
  return sum;
}

std::array<long double, 2> Screen(const Vec3 &point, const CameraBasis &camera, const Lens &lens) {
  const auto matrix = lens.Projection();
  std::array<long double, 3> relative{};
  for (size_t axis = 0; axis < 3; ++axis) {
    relative[axis] = static_cast<long double>(point[axis]) - camera.EyeM[axis];
  }
  const long double z = -DotExact(relative, camera.Forward);
  const long double w = matrix[11] * z + matrix[15];
  return {(matrix[0] * DotExact(relative, camera.Right) + matrix[8] * z + matrix[12]) / w *
              lens.WidePx * .5,
          (matrix[5] * DotExact(relative, camera.Up) + matrix[9] * z + matrix[13]) / w *
              lens.HighPx * .5};
}

void RecordCase(nlohmann::json &cases,
                const RadiancePlane &source,
                const CameraBasis &camera,
                const Lens &lens,
                const Mat4 *world = nullptr) {
  const auto hull = ImpostorPoseBounds::From(source);
  const auto support = source.SupportFrame();
  const auto frame = ImpostorViewFrame::From(source.TowardEye);
  CHECK(hull && support && frame, "the actual fitted support binds to its complete capture domain");
  if (!hull || !support || !frame) { return; }
  const auto error =
      world == nullptr ? hull->Project(camera, lens) : hull->Project(camera, lens, *world);
  CHECK(error && error->DepthInside, "finite source and support remain inside the clipping volume");
  if (!error) { return; }
  cases.push_back({{"centre", source.Centre.Axis},
                   {"right", frame->Right.Axis},
                   {"up", frame->Up.Axis},
                   {"direction", source.TowardEye.Axis},
                   {"half_extent", source.HalfExtentM},
                   {"fit", source.DepthCoefficients.Axis},
                   {"support_centre", support->Centre.Axis},
                   {"support_normal", support->Normal.Axis},
                   {"eye", camera.EyeM.Axis},
                   {"camera_right", camera.Right.Axis},
                   {"camera_up", camera.Up.Axis},
                   {"camera_forward", camera.Forward.Axis},
                   {"projection", lens.Projection().Column},
                   {"raster", {lens.WidePx, lens.HighPx}},
                   {"axis_upper_px", error->AxisUpperPx.Axis},
                   {"l1_upper_px", error->L1UpperPx}});
  if (world != nullptr) { cases.back()["world_from_field"] = world->Column; }
}

nlohmann::json FittedControls() {
  nlohmann::json cases = nlohmann::json::array();
  Mat4 world;
  world[0] = world[5] = 0;
  world[1] = 1;
  world[4] = -1;
  world.SetTranslation({{6000000, 4000000, -2000000}});
  for (const Vec3 direction : {Vec3{{0, 0, 1}}, Vec3{{.6, 0, .8}}, Vec3{{0, 1, 0}}}) {
    const auto frame = ImpostorViewFrame::From(direction);
    if (!frame) { continue; }
    const Vec3 centre{{11, -7, 29}};
    for (const Vec3 fit : {Vec3{}, Vec3{{0, 0, 1}}, Vec3{{0, 0, -2}}, Vec3{{.25, -.5, .125}}}) {
      for (const double distance : {2000., 64000., 240000.}) {
        const auto camera = Viewpoint::LookAt(
            {.EyeM = centre + direction * distance + frame->Right * 30., .AimM = centre},
            frame->Up);
        CHECK(camera, "the independent query camera has an orthonormal basis");
        if (!camera) { continue; }
        for (const bool parallel : {false, true}) {
          const RadiancePlane source{.Centre = centre,
                                     .TowardEye = direction,
                                     .HalfExtentM = kExtent,
                                     .DepthCoefficients = fit};
          RecordCase(cases, source, *camera, Optics(parallel));
          const CameraBasis mapped{.EyeM = world.TransformPoint(camera->EyeM),
                                   .Forward = world.TransformDirection(camera->Forward),
                                   .Right = world.TransformDirection(camera->Right),
                                   .Up = world.TransformDirection(camera->Up)};
          RecordCase(cases, source, mapped, Optics(parallel), &world);
        }
      }
    }
  }
  return cases;
}

void FlatCounterexample() {
  const auto camera = Viewpoint::LookAt({.EyeM = {{0, 0, 64000}}, .AimM = {}}, {{0, 1, 0}});
  const auto flat = ImpostorPoseBounds::From({}, {{0, 0, 1}}, kExtent);
  const RadiancePlane shifted{.Centre = {},
                              .TowardEye = {{0, 0, 1}},
                              .HalfExtentM = kExtent,
                              .DepthCoefficients = {{0, 0, 1}}};
  const auto fitted = ImpostorPoseBounds::From(shifted);
  CHECK(camera && flat && fitted, "the counterexample has valid independent capture operands");
  if (!camera || !flat || !fitted) { return; }
  const auto lens = Optics(false);
  const auto oldBound = flat->Project(*camera, lens);
  const auto actualBound = fitted->Project(*camera, lens);
  const auto original = Screen({{kExtent, 0, -2 * kExtent}}, *camera, lens);
  const auto support = Screen({{kExtent, 0, kExtent}}, *camera, lens);
  const auto actual = std::abs(original[0] - support[0]);
  CHECK(oldBound && actualBound && actual > oldBound->AxisUpperPx[0] &&
            actual <= actualBound->AxisUpperPx[0],
        "a flat-plane bound cannot qualify the existing displaced physical support");
}

void InvalidSupports() {
  RadiancePlane source{.Centre = {}, .TowardEye = {{0, 0, 1}}, .HalfExtentM = kExtent};
  const auto camera = Viewpoint::LookAt({.EyeM = {{100, 0, 10}}, .AimM = {}}, {{0, 1, 0}});
  CHECK(camera, "the facing counterexample uses a valid camera");
  if (!camera) { return; }
  const auto lens = Optics(false);
  source.DepthCoefficients = {{2, 0, 0}};
  const auto hull = ImpostorPoseBounds::From(source);
  CHECK(hull && !hull->Project(*camera, lens) &&
            !BuildProjectedRadianceSupport(source, *camera, lens),
        "geometric admission and drawn support both refuse the actual back-facing fitted plane");
  for (const double bad : {std::numeric_limits<double>::infinity(),
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::max()}) {
    source.DepthCoefficients = {{0, 0, bad}};
    CHECK(!source.SupportFrame() && !ImpostorPoseBounds::From(source) &&
              !BuildProjectedRadianceSupport(source, *camera, lens),
          "nonfinite or overflowing support offsets fail before geometry or pose publication");
  }
}
}

int main(int argc, char *const *argv) {
  const auto cases = FittedControls();
  FlatCounterexample();
  InvalidSupports();
  if (argc == 3 && std::string_view(argv[1]) == "--operands") {
    std::ofstream output(argv[2]);
    output << cases.dump();
    CHECK(output.good(), "the independent rational reference receives actual native operands");
  }
  return Report();
}
