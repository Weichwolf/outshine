#include "ImpostorPoseBounds.h"
#include "ImpostorViewFrame.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

Vec3 Direction(const Mat4 &frame, const Vec3 &local) {
  Vec3 result;
  for (size_t row = 0; row < 3; ++row) {
    for (size_t column = 0; column < 3; ++column) {
      result[row] += frame.At(row, column) * local[column];
    }
  }
  return result;
}

CameraBasis WorldCamera(const Mat4 &frame, const CameraBasis &local) {
  return {.EyeM = frame.Translation() + Direction(frame, local.EyeM),
          .Forward = Direction(frame, local.Forward),
          .Right = Direction(frame, local.Right),
          .Up = Direction(frame, local.Up)};
}

struct GeographicAnchor {
  double LatitudeDeg;
  double LongitudeDeg;
};

Mat4 Placement(GeographicAnchor anchor) {
  const double p = anchor.LatitudeDeg * std::acos(-1.0) / 180;
  const double l = anchor.LongitudeDeg * std::acos(-1.0) / 180;
  const Vec3 east{{-std::sin(l), std::cos(l), 0}};
  const Vec3 up{{std::cos(p) * std::cos(l), std::cos(p) * std::sin(l), std::sin(p)}};
  const std::array columns{east, up, Cross(up, east) * -1.0};
  Mat4 result;
  for (size_t column = 0; column < 3; ++column) {
    for (size_t row = 0; row < 3; ++row) { result.At(row, column) = columns[column][row]; }
  }
  result.SetTranslation(up * 6378137.0);
  return result;
}

void Array(std::ostream &out, const auto &values) {
  out << '[';
  bool first = true;
  for (const auto value : values) {
    if (!first) { out << ','; }
    out << value;
    first = false;
  }
  out << ']';
}

void Oracle(std::ostream &out,
            const Mat4 &world,
            const CameraBasis &camera,
            const Lens &lens,
            const Vec3 &centre,
            const Vec3 &direction,
            double extent,
            const ImpostorPoseBounds::Error &bound,
            std::optional<double> unsafeL1 = std::nullopt) {
  const auto capture = ImpostorViewFrame::From(direction);
  CHECK(capture, "the oracle records the actual capture basis");
  if (!capture) { return; }
  out << std::setprecision(17) << "{\"frame\":";
  Array(out, world);
  out << ",\"eye\":";
  Array(out, camera.EyeM);
  out << ",\"basis\":[";
  Array(out, camera.Right);
  out << ',';
  Array(out, camera.Up);
  out << ',';
  Array(out, camera.Forward * -1.0);
  out << "],\"projection\":";
  Array(out, lens.Projection());
  out << ",\"centre\":";
  Array(out, centre);
  out << ",\"direction\":";
  Array(out, direction);
  out << ",\"capture\":[";
  Array(out, capture->Right);
  out << ',';
  Array(out, capture->Up);
  out << "],\"extent\":" << extent << ",\"raster\":[" << lens.WidePx << ',' << lens.HighPx
      << "],\"bound\":";
  Array(out, bound.AxisUpperPx);
  out << ",\"l1\":" << bound.L1UpperPx;
  if (unsafeL1) { out << ",\"unsafe_l1\":" << *unsafeL1; }
  out << "}\n";
}

void CheckCamera(const Mat4 &world, const Vec3 &direction, double distance, std::ostream *oracle) {
  const Vec3 centre{{1, 2, 3}};
  const auto hull = ImpostorPoseBounds::From(centre, direction, 3);
  CHECK(hull, "the tilted whole capture is valid");
  if (!hull) { return; }
  const auto local = Viewpoint::LookAt(
      {.EyeM = centre + direction * distance + Vec3{{20, -5, 0}}, .AimM = centre}, Vec3{{0, 1, 0}});
  CHECK(local, "the independent local camera is finite");
  if (!local) { return; }
  const auto camera = WorldCamera(world, *local);
  for (const bool orthographic : {false, true}) {
    Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 90, .NearM = .1f, .FarM = 1000000};
    lens.Jitter = {{.25f, -.125f}};
    if (orthographic) {
      lens.OrthoM = 4000;
      lens.OrthoWidthM = 8000;
    }
    const auto bound = hull->Project(camera, lens, world);
    const auto control = hull->Project(*local, lens);
    CHECK(bound && bound->DepthInside && control && control->DepthInside,
          "World and local perspective/orthographic capture domains are wholly inside");
    if (!bound || !control) { continue; }
    CHECK(std::abs(bound->L1UpperPx - control->L1UpperPx) < 1e-7,
          "geographic placement retains the local geometric scale");
    if (oracle != nullptr) { Oracle(*oracle, world, camera, lens, centre, direction, 3, *bound); }
  }
}

void GeographicPoses(std::ostream *oracle) {
  const std::array<GeographicAnchor, 5> anchors{{{.LatitudeDeg = 0, .LongitudeDeg = 0},
                                                 {.LatitudeDeg = 55, .LongitudeDeg = 179.9},
                                                 {.LatitudeDeg = -42, .LongitudeDeg = -179.9},
                                                 {.LatitudeDeg = 89.9, .LongitudeDeg = 40},
                                                 {.LatitudeDeg = -89.9, .LongitudeDeg = -75}}};
  for (const auto anchor : anchors) {
    const auto world = Placement(anchor);
    for (const Vec3 direction : {Vec3{{0, 0, 1}}, Vec3{{.5, std::sqrt(.5), .5}}}) {
      for (const double distance : {2000.0, 64000.0, 240000.0}) {
        CheckCamera(world, direction, distance, oracle);
      }
    }
  }
}

void EncodedFrameControls(std::ostream *oracle) {
  const Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 90, .NearM = .1f, .FarM = 1000000};
  const Vec3 direction{{0, 0, 1}};
  const auto hull = ImpostorPoseBounds::From({}, direction, 1);
  CHECK(hull, "the precision control has a complete source domain");
  if (!hull) { return; }
  Mat4 world;
  world.SetTranslation({{0, 0, 6378137.125}});
  const CameraBasis camera{.EyeM = world.Translation() + Vec3{{1.5, 0, 64.375}}};
  const auto exact = hull->Project(camera, lens, world);
  auto rounded = world;
  auto roundedCamera = camera;
  for (size_t i = 0; i < 16; ++i) { rounded[i] = static_cast<float>(world[i]); }
  for (size_t i = 0; i < 3; ++i) { roundedCamera.EyeM[i] = static_cast<float>(camera.EyeM[i]); }
  const auto wrong = hull->Project(roundedCamera, lens, rounded);
  CHECK(exact && wrong && wrong->L1UpperPx < exact->L1UpperPx * .999,
        "early Float32 ECEF rounding can falsely admit a smaller pixel budget");
  if (oracle != nullptr && exact && wrong) {
    Oracle(*oracle, world, camera, lens, {}, direction, 1, *exact, wrong->L1UpperPx);
  }
  world[0] += 2e-11;
  world.At(0, 1) = 2e-11;
  const auto residual = hull->Project(camera, lens, world);
  CHECK(residual && residual->DepthInside,
        "schema-sized rotation residuals are preserved without a nominal transpose inverse");
  if (oracle != nullptr && residual) {
    Oracle(*oracle, world, camera, lens, {}, direction, 1, *residual);
  }
  world = Placement({.LatitudeDeg = 0, .LongitudeDeg = 0});
  const auto local = Viewpoint::LookAt({.EyeM = {{0, 64, 64}}, .AimM = {}}, Vec3{{0, 1, 0}});
  CHECK(local, "the direct World-as-local counterexample has a camera");
  if (!local) { return; }
  const auto worldCamera = WorldCamera(world, *local);
  const auto correct = hull->Project(worldCamera, lens, world);
  const auto wrongSpace = hull->Project(worldCamera, lens);
  CHECK(correct && (!wrongSpace || wrongSpace->L1UpperPx < correct->L1UpperPx * .001),
        "World operands require the bound frame rather than the local overload");
  if (oracle != nullptr && correct) {
    Oracle(*oracle, world, worldCamera, lens, {}, direction, 1, *correct);
  }
}

void OriginAndInvalidFrames() {
  const auto hull = ImpostorPoseBounds::From({}, {{0, 0, 1}}, 1);
  CHECK(hull, "the origin control has a complete domain");
  if (!hull) { return; }
  Mat4 world;
  world.SetTranslation({{6378137.125, 800, -256}});
  auto camera = CameraBasis{.EyeM = world.Translation() + Vec3{{25.125, -4.25, 64000.5}}};
  const Lens lens{.WidePx = 1280, .HighPx = 720, .FovDeg = 90, .NearM = .1f, .FarM = 1000000};
  const auto first = hull->Project(camera, lens, world);
  const Vec3 shift{{6370000, 0, -1024}};
  world.SetTranslation(world.Translation() - shift);
  camera.EyeM = camera.EyeM - shift;
  const auto second = hull->Project(camera, lens, world);
  CHECK(first && second && first->AxisUpperPx == second->AxisUpperPx &&
            first->L1UpperPx == second->L1UpperPx,
        "an exactly represented origin shift leaves relative projection unchanged");
  for (const double bad : {-1.0, 2.0, std::numeric_limits<double>::quiet_NaN()}) {
    auto invalid = world;
    invalid[0] = bad;
    CHECK(!hull->Project(camera, lens, invalid),
          "reflection, scale and nonfinite frames retain originals");
  }
  auto invalid = world;
  invalid.At(3, 0) = .01;
  CHECK(!hull->Project(camera, lens, invalid), "projective frames cannot be interpreted as rigid");
  invalid = world;
  invalid.At(0, 1) = .01;
  CHECK(!hull->Project(camera, lens, invalid), "sheared frames retain originals");
  camera.EyeM = world.Translation() + Vec3{{0, 0, -10}};
  CHECK(!hull->Project(camera, lens, world),
        "World support retains the existing back-facing guard");
  camera.EyeM = world.Translation() + Vec3{{0, 0, 1}};
  CHECK(!hull->Project(camera, lens, world), "World hull crossing the eye retains originals");
  camera.EyeM = world.Translation() + Vec3{{0, 0, 10}};
  auto near = lens;
  near.NearM = 9;
  const auto clipped = hull->Project(camera, near, world);
  CHECK(clipped && !clipped->DepthInside, "World near clipping is not geometrically admitted");
}
}

int main(int argc, const char *const *argv) {
  std::ofstream oracle;
  if (argc == 2) {
    oracle.open(argv[1]);
    CHECK(oracle.is_open(), "the requested exact-rational oracle input opens");
  }
  GeographicPoses(oracle.is_open() ? &oracle : nullptr);
  EncodedFrameControls(oracle.is_open() ? &oracle : nullptr);
  OriginAndInvalidFrames();
  return Report();
}
